/*!
\copyright  Copyright (c) 2017 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       av_headset_kymera.c
\brief      Kymera Manager
*/

#include <a2dp.h>
#include <panic.h>
#include <stream.h>
#include <sink.h>
#include <source.h>
#include <chain.h>
#include <tws_packetiser.h>
#include <vmal.h>

#ifdef __QCC3400_APP__
#include <audio_clock.h>
#include <audio_power.h>
#endif

#include "opmsg_prim.h"

#include "av_headset.h"
#include "av_headset_latency.h"
#include "av_headset_log.h"
#include "av_headset_chain_roles.h"

#include "chains/chain_aptx_mono_no_autosync_decoder.h"
#include "chains/chain_sbc_mono_no_autosync_decoder.h"
#include "chains/chain_aac_stereo_decoder_left.h"
#include "chains/chain_aac_stereo_decoder_right.h"
#include "chains/chain_output_volume.h"
#include "chains/chain_forwarding_input_sbc_left.h"
#include "chains/chain_forwarding_input_aptx_left.h"
#include "chains/chain_forwarding_input_aac_left.h"
#include "chains/chain_forwarding_input_sbc_right.h"
#include "chains/chain_forwarding_input_aptx_right.h"
#include "chains/chain_forwarding_input_aac_right.h"
#include "chains/chain_forwarding_input_aac_stereo_left.h"
#include "chains/chain_forwarding_input_aac_stereo_right.h"
#ifndef HFP_USE_2MIC
#include "chains/chain_sco_nb.h"
#include "chains/chain_sco_wb.h"
#else
#include "chains/chain_sco_nb_2mic.h"
#include "chains/chain_sco_wb_2mic.h"
#endif /* HFP_USE_2MIC */
#ifdef INCLUDE_SCOFWD
#include "chains/chain_scofwd_recv.h"
#include "chains/chain_scofwd_wb.h"
#include "chains/chain_scofwd_nb.h"
#endif
#include "chains/chain_tone_gen.h"

/*! Macro for creating messages */
#define MAKE_KYMERA_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*! Helper macro to get size of fixed arrays to populate structures */
#define DIMENSION_AND_ADDR_OF(ARRAY) ARRAY_DIM((ARRAY)), (ARRAY)

/*! Convert a channel ID to a bit mask */
#define CHANNEL_TO_MASK(channel) ((uint32)1 << channel)

/*!@{ \name Port numbers for the Source Sync operator */
#define KYMERA_SOURCE_SYNC_INPUT_PORT (0)
#define KYMERA_SOURCE_SYNC_OUTPUT_PORT (0)
/*!@} */

/*! Kymera requires gain specified in unit of 1/60th db */
#define KYMERA_DB_SCALE (60)

/*! Kymera ringtone generator has a fixed sample rate of 8 kHz */
#define KYMERA_TONE_GEN_RATE (8000)

/*!@{ \name Defines used in setting up kymera messages
    Kymera operator messages are 3 words long, with the ID in the 2nd word */
#define KYMERA_OP_MSG_LEN (3)
#define KYMERA_OP_MSG_WORD_MSG_ID (1)
#define KYMERA_OP_MSG_ID_TONE_END (0x0001) /*!< Kymera ringtone generator TONE_END message */
/*!@}*/

/*@{ \name System kick periods, in microseconds */
#define KICK_PERIOD_FAST (2000)
#define KICK_PERIOD_SLOW (7500)

#define KICK_PERIOD_SLAVE_SBC (KICK_PERIOD_SLOW)
#define KICK_PERIOD_SLAVE_APTX (KICK_PERIOD_SLOW)
#define KICK_PERIOD_SLAVE_AAC (KICK_PERIOD_SLOW)
#define KICK_PERIOD_MASTER_SBC (KICK_PERIOD_SLOW)
#define KICK_PERIOD_MASTER_AAC (KICK_PERIOD_SLOW)
#define KICK_PERIOD_MASTER_APTX (KICK_PERIOD_SLOW)
#define KICK_PERIOD_TONES (KICK_PERIOD_SLOW)
#define KICK_PERIOD_VOICE (KICK_PERIOD_FAST) /*!< Use low latency for voice */
/*@} */

/*! Maximum sample rate supported by this application */
#define MAX_SAMPLE_RATE (48000)

/*! Maximum codec rate expected by this application */
#define MAX_CODEC_RATE_KBPS (384) /* Stereo aptX */

/*!:{ \name Macros to calculate buffer sizes required to hold a specific (timed) amount of audio */
#define CODEC_BITS_PER_MEMORY_WORD (16)
#define MS_TO_BUFFER_SIZE_MONO_PCM(time_ms, sample_rate) (((time_ms) * (sample_rate)) / MS_PER_SEC)
#define US_TO_BUFFER_SIZE_MONO_PCM(time_us, sample_rate) (((time_us) * (sample_rate)) / US_PER_SEC)
#define MS_TO_BUFFER_SIZE_CODEC(time_ms, codec_rate_kbps) (((time_ms) * (codec_rate_kbps)) / CODEC_BITS_PER_MEMORY_WORD)
/*!@}*/

/*!@{ \name Buffer sizes required to hold enough audio to achieve the TTP latency */
#define PRE_DECODER_BUFFER_SIZE (MS_TO_BUFFER_SIZE_CODEC(PRE_DECODER_BUFFER_MS, MAX_CODEC_RATE_KBPS))
#define PCM_LATENCY_BUFFER_SIZE (MS_TO_BUFFER_SIZE_MONO_PCM(PCM_LATENCY_BUFFER_MS, MAX_SAMPLE_RATE))
/*!@}*/

typedef enum 
{
    PASSTHROUGH_MODE,
    CONSUMER_MODE
} switched_passthrough_states;


/*!@{ \name Macros to set and clear bits in the lock. */
#define appKymeraSetToneLock(theKymera) (theKymera)->lock |= 1U
#define appKymeraClearToneLock(theKymera) (theKymera)->lock &= ~1U
#define appKymeraSetStartingLock(theKymera) (theKymera)->lock |= 2U

#define appKymeraClearStartingLock(theKymera) (theKymera)->lock &= ~2U
/*!@}*/

/*! Convert x into 1.31 format */
#define FRACTIONAL(x) ( (int)( (x) * ((1l<<31) - 1) ))

/*! \brief Macro to help getting an operator from chain.
    \param op The returned operator, or INVALID_OPERATOR if the operator was not
           found in the chain.
    \param chain_handle The chain handle.
    \param role The operator role to get from the chain.
    \return TRUE if the operator was found, else FALSE
 */
#define GET_OP_FROM_CHAIN(op, chain_handle, role) \
    (INVALID_OPERATOR != ((op) = ChainGetOperatorByRole((chain_handle), (role))))

static const chain_join_roles_t slave_inter_chain_connections[] =
{
    { .source_role = EPR_SOURCE_DECODED_PCM, .sink_role = EPR_SINK_MIXER_MAIN_IN }
};

static const chain_join_roles_t tone_music_inter_chain_connections[] =
{
    { .source_role = RESAMPLER_OUT, .sink_role = EPR_SINK_MIXER_TONE_IN }
};

static const chain_join_roles_t tone_voice_inter_chain_connections[] =
{
    { .source_role = RESAMPLER_OUT, .sink_role = EPR_SCO_VOLUME_AUX }
};

/* Configuration of source sync groups and routes */
static const source_sync_sink_group_t sink_groups[] =
{
    {
        .meta_data_required = TRUE,
        .rate_match = FALSE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_INPUT_PORT)
    }
};

static const source_sync_source_group_t source_groups[] =
{
    {
        .meta_data_required = TRUE,
        .ttp_required = TRUE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_OUTPUT_PORT)
    }
};

static source_sync_route_t routes[] =
{
    {
        .input_terminal = KYMERA_SOURCE_SYNC_INPUT_PORT,
        .output_terminal = KYMERA_SOURCE_SYNC_OUTPUT_PORT,
        .transition_samples = 0,
        .sample_rate = 0, /* Overridden later */
        .gain = 0
    }
};

static const capability_bundle_t capability_bundle[] = {
    {
        "download_switched_passthrough_consumer.edkcs",
        capability_bundle_available_p0
    },
    {
        "download_aptx_demux.edkcs",
        capability_bundle_available_p0
    },
#if defined(INCLUDE_SCOFWD) && !defined(SFWD_USING_SQIF)
    /*  Chains for SCO forwarding.
        Likely to update to use the downloadable AEC regardless
        as offers better TTP support (synchronisation) and other
        extensions */
    {
        "download_async_wbs.edkcs",
        capability_bundle_available_p0
    },
    {
        "download_aec_reference.edkcs",
        capability_bundle_available_p0
    },
#endif
};

static const capability_bundle_config_t bundle_config = {capability_bundle, ARRAY_DIM(capability_bundle)};

static void appKymeraToneStop(void);
static void appKymeraHandleInternalScoSetVolume(uint8 volume);


#ifdef INCLUDE_SCOFWD
static bool appKymeraHandleInternalScoStopForwardingTx(void);
#else
#define appKymeraHandleInternalScoStopForwardingTx() TRUE
#endif

#define AWBSDEC_SET_BITPOOL_VALUE    0x0003
#define AWBSENC_SET_BITPOOL_VALUE    0x0001

typedef struct set_bitpool_msg_s
{
    uint16 id;
    uint16 bitpool;
}set_bitpool_msg_t;


#ifdef INCLUDE_SCOFWD
static void OperatorsAwbsSetBitpoolValue(Operator op, uint16 bitpool, bool decoder)
{
    set_bitpool_msg_t bitpool_msg;
    bitpool_msg.id = decoder ? AWBSDEC_SET_BITPOOL_VALUE : AWBSENC_SET_BITPOOL_VALUE;
    bitpool_msg.bitpool = (uint16)(bitpool);

    PanicFalse(VmalOperatorMessage(op, &bitpool_msg, SIZEOF_OPERATOR_MESSAGE(bitpool_msg), NULL, 0));
}
#endif


/*! @brief Convert volume on 0..127 scale to MIN_VOLUME_DB..MAX_VOLUME_DB and return the KYMERA_DB_SCALE value.
           A input of 0 results in muted output.
 */
static int32 volTo60thDbGain(uint16 volume)
{
    int32 gain = -90;
    if (volume)
    {
        int32 minv = appConfigMinVolumedB();
        int32 maxv = appConfigMaxVolumedB();
        gain = volume * (maxv - minv);
        gain /= 127;
        gain += minv;
    }
    return gain * KYMERA_DB_SCALE;
}

static void appKymeraGetA2dpCodecSettingsCore(const a2dp_codec_settings *codec_settings,
                                              uint8 *seid, Sink *sink, uint32 *rate,
                                              bool *cp_enabled, uint16 *mtu)
{
    if (seid)
    {
        *seid = codec_settings->seid;
    }
    if (sink)
    {
        *sink = codec_settings->sink;
    }
    if (rate)
    {
        *rate = codec_settings->rate;
    }
    if (cp_enabled)
    {
        *cp_enabled = !!(codec_settings->codecData.content_protection);
    }
    if (mtu)
    {
        *mtu = codec_settings->codecData.packet_size;
    }
}

static void appKymeraGetA2dpCodecSettingsSBC(const a2dp_codec_settings *codec_settings,
                                             sbc_encoder_params_t *sbc_encoder_params)
{
    uint8 sbc_format = codec_settings->codecData.format;
    sbc_encoder_params->channel_mode = codec_settings->channel_mode;
    sbc_encoder_params->bitpool_size = codec_settings->codecData.bitpool;
    sbc_encoder_params->sample_rate = codec_settings->rate;
    sbc_encoder_params->number_of_subbands = (sbc_format & 1) ? 8 : 4;
    sbc_encoder_params->number_of_blocks = (((sbc_format >> 4) & 3) + 1) * 4;
    sbc_encoder_params->allocation_method =  ((sbc_format >> 2) & 1);
}

/*! \brief Configure power mode and clock frequencies of the DSP for the lowest
 *         power consumption possible based on the current state / codec.
 *
 * Note that calling this function with chains already started will likely
 * cause audible glitches if using I2S output. DAC output should be ok.
 */
static void appKymeraConfigureDspPowerMode(bool tone_playing)
{
#if defined(__QCC3400_APP__) && !defined(SFWD_USING_SQIF)
    kymeraTaskData *theKymera = appGetKymera();

    /* Assume we are switching to the low power slow clock unless one of the
     * special cases below applies */
    audio_dsp_clock_configuration cconfig = {
        .active_mode = AUDIO_DSP_SLOW_CLOCK,
        .low_power_mode = AUDIO_DSP_CLOCK_NO_CHANGE,
        .trigger_mode = AUDIO_DSP_CLOCK_NO_CHANGE
    };
    audio_dsp_clock kclocks;
    audio_power_save_mode mode = AUDIO_POWER_SAVE_MODE_3;

    if (   theKymera->state == KYMERA_STATE_SCO_ACTIVE
        || theKymera->state == KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING
        || theKymera->state == KYMERA_STATE_SCOFWD_RX_ACTIVE)
    {
        /* Never switch clocks at all for voice */
        return;
    }

    if (tone_playing)
    {
        /* Always jump up to normal clock for tones - for most codecs there is
         * not enough MIPs when running on a slow clock to also play a tone */
        cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
        mode = AUDIO_POWER_SAVE_MODE_1;
    }
    else
    {
        /* Either setting up for the first time or returning from a tone, in
         * either case return to the default clock rate for the codec in use */
        switch (theKymera->a2dp_seid)
        {
            case AV_SEID_APTX_SNK:
                /* Not enough MIPs to run aptX master (TWS standard) on slow clock */
                cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
                break;

            default:
                break;
        }
    }

    PanicFalse(AudioDspClockConfigure(&cconfig));
    PanicFalse(AudioPowerSaveModeSet(mode));

    PanicFalse(AudioDspGetClock(&kclocks));
    mode = AudioPowerSaveModeGet();
    DEBUG_LOGF("appKymeraConfigureDspPowerMode, kymera clocks %d %d %d, mode %d", kclocks.active_mode, kclocks.low_power_mode, kclocks.trigger_mode, mode);
#else
    UNUSED(tone_playing);
#endif
}

/*! \brief Configure PIO required for controlling external amplifier. */
static void appKymeraExternalAmpSetup(void)
{
    if (appConfigExternalAmpControlRequired())
    {
        int pio_mask = (1 << (appConfigExternalAmpControlPio() % 32));
        int pio_bank = appConfigExternalAmpControlPio() / 32;

        /* map in PIO */
        PioSetMapPins32Bank(pio_bank, pio_mask, pio_mask);
        /* set as output */
        PioSetDir32Bank(pio_bank, pio_mask, pio_mask);
        /* start disabled */
        PioSet32Bank(pio_bank, pio_mask,
                     appConfigExternalAmpControlDisableMask());
    }
}

/*! \brief Enable or disable external amplifier. */
static void appKymeraExternalAmpControl(bool enable)
{
    if (appConfigExternalAmpControlRequired())
    {
        int pio_mask = (1 << (appConfigExternalAmpControlPio() % 32));
        int pio_bank = appConfigExternalAmpControlPio() / 32;

        PioSet32Bank(pio_bank, pio_mask,
                     enable ? appConfigExternalAmpControlEnableMask() :
                              appConfigExternalAmpControlDisableMask());
    }
}

static void appKymeraConfigureRtpDecoder(Operator op, rtp_codec_type_t codec_type, uint32 rate, bool cp_header_enabled)
{
    rtp_working_mode_t mode = (codec_type == rtp_codec_type_aptx && !cp_header_enabled) ?
                                    rtp_ttp_only : rtp_decode;
    const uint32 filter_gain = FRACTIONAL(0.997);
    const uint32 err_scale = FRACTIONAL(-0.00000001);
    /* Disable the Kymera TTP startup period, the other parameters are defaults. */
    const OPMSG_COMMON_MSG_SET_TTP_PARAMS ttp_params_msg = {
        OPMSG_COMMON_MSG_SET_TTP_PARAMS_CREATE(OPMSG_COMMON_SET_TTP_PARAMS,
            UINT32_MSW(filter_gain), UINT32_LSW(filter_gain),
            UINT32_MSW(err_scale), UINT32_LSW(err_scale),
            0)
    };

    OperatorsRtpSetCodecType(op, codec_type);
    OperatorsRtpSetWorkingMode(op, mode);
    OperatorsStandardSetTimeToPlayLatency(op, TWS_STANDARD_LATENCY_US);
    OperatorsStandardSetBufferSize(op, PRE_DECODER_BUFFER_SIZE);
    OperatorsRtpSetContentProtection(op, cp_header_enabled);
    /* Sending this message trashes the RTP operator sample rate */
    PanicFalse(OperatorMessage(op, ttp_params_msg._data, OPMSG_COMMON_MSG_SET_TTP_PARAMS_WORD_SIZE, NULL, 0));
    OperatorsStandardSetSampleRate(op, rate);
}

/* Set the SPC mode, consumer if is_consumer else passthrough */
static void appKymeraConfigureSpcMode(Operator op, bool is_consumer)
{
    uint16 msg[2];
    msg[0] = 1; /* MSG ID to set SPC mode transition */
    msg[1] = is_consumer;
    PanicFalse(OperatorMessage(op, msg, 2, NULL, 0));
}

/* Set the SPC data format, PCM if is_pcm else encoded-data */
static void appKymeraConfigureSpcDataFormat(Operator op, bool is_pcm)
{
    uint16 msg[2];
    msg[0] = 2; /* MSG ID to set SPC data type */
    msg[1] = is_pcm;
    PanicFalse(OperatorMessage(op, msg, 2, NULL, 0));
}

static void appKymeraConfigureSourceSync(Operator op, uint32 rate, unsigned kick_period_us)
{
    /* Override sample rate in routes config */
    routes[0].sample_rate = rate;

    /* Send operator configuration messages */
    OperatorsStandardSetSampleRate(op, rate);
    OperatorsSourceSyncSetSinkGroups(op, DIMENSION_AND_ADDR_OF(sink_groups));
    OperatorsSourceSyncSetSourceGroups(op, DIMENSION_AND_ADDR_OF(source_groups));
    OperatorsSourceSyncSetRoutes(op, DIMENSION_AND_ADDR_OF(routes));

    /* Output buffer needs to be able to hold at least SS_MAX_PERIOD worth
     * of audio (default = 2 * Kp), but be less than SS_MAX_LATENCY (5 * Kp).
     * The recommendation is 2 Kp more than SS_MAX_PERIOD, so 4 * Kp. */
    OperatorsStandardSetBufferSize(op, US_TO_BUFFER_SIZE_MONO_PCM(4 * kick_period_us, rate));
}

static void appKymeraSetMainVolume(kymera_chain_handle_t chain, uint16 volume)
{
    Operator volop;

    if (GET_OP_FROM_CHAIN(volop, chain, OPR_VOLUME_CONTROL))
    {
        OperatorsVolumeSetMainGain(volop, volTo60thDbGain(volume));
        OperatorsVolumeMute(volop, (volume == 0));
    }
}

static void appKymeraSetVolume(kymera_chain_handle_t chain, uint16 volume)
{
    Operator volop;

    if (GET_OP_FROM_CHAIN(volop, chain, OPR_VOLUME_CONTROL))
    {
        OperatorsVolumeSetMainAndAuxGain(volop, volTo60thDbGain(volume));
        OperatorsVolumeMute(volop, (volume == 0));
    }
}

static void appKymeraMuteVolume(kymera_chain_handle_t chain)
{
    appKymeraSetVolume(chain, 0);
}

/* These SBC encoder parameters are used when forwarding is disabled to save power */
static void appKymeraSetLowPowerSBCParams(kymera_chain_handle_t chain, uint32 rate)
{
    Operator op;

    if (GET_OP_FROM_CHAIN(op, chain, OPR_SBC_ENCODER))
    {
        sbc_encoder_params_t sbc_encoder_params_low_power_default = {
            .number_of_subbands = 4,
            .number_of_blocks = 16,
            .bitpool_size = 1,
            .sample_rate = rate,
            .channel_mode = sbc_encoder_channel_mode_mono,
            .allocation_method = sbc_encoder_allocation_method_loudness
        };
        OperatorsSbcEncoderSetEncodingParams(op, &sbc_encoder_params_low_power_default);
    }
}


/*  Configure the source sync, volume and latency buffer operators in the chain.

    Other than sample rate and kick period, the gain in the volume operator
    is set to level (0db gain) and then muted.
    
    Requires the chain to use standard roles for the operators, namely
    #OPR_SOURCE_SYNC,  #OPR_VOLUME_CONTROL and #OPR_LATENCY_BUFFER.

    If buffer_size is zero, the buffer size is not configured.
 */
static void appKymeraConfigureOutputChainOperators(kymera_chain_handle_t chain,
                                                   uint32 sample_rate, unsigned kick_period,
                                                   unsigned buffer_size, uint8 volume)
{
    Operator sync_op;
    Operator volume_op;

    /* Configure operators */
    if (GET_OP_FROM_CHAIN(sync_op, chain, OPR_SOURCE_SYNC))
    {
        /* SourceSync is optional in chains. */
        appKymeraConfigureSourceSync(sync_op, sample_rate, kick_period);
    }

    volume_op = ChainGetOperatorByRole(chain, OPR_VOLUME_CONTROL);
    OperatorsStandardSetSampleRate(volume_op, sample_rate);

    appKymeraSetVolume(chain, volume);

    if (buffer_size)
    {
        Operator op = ChainGetOperatorByRole(chain, OPR_LATENCY_BUFFER);
        OperatorsStandardSetBufferSize(op, buffer_size);
    }
}

/*! If buffer_size is zero, the buffer size is not configured */
static void appKymeraCreateOutputChain(uint32 rate, unsigned kick_period,
                                       unsigned buffer_size, uint8 volume)
{
    kymeraTaskData *theKymera = appGetKymera();
    Sink dac;
    kymera_chain_handle_t chain;

    /* Create chain */
    chain = ChainCreate(&chain_output_volume_config);
    theKymera->chain_output_vol_handle = chain;

    appKymeraConfigureOutputChainOperators(chain, rate, kick_period, buffer_size, volume);

    /* Configure the DAC channel */
    dac = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, appConfigLeftAudioChannel());
    PanicFalse(SinkConfigure(dac, STREAM_CODEC_OUTPUT_RATE, rate));
    PanicFalse(SinkConfigure(dac, STREAM_RM_ENABLE_DEFERRED_KICK, 0));

    ChainConnect(chain);
    ChainConnectOutput(chain, dac, EPR_SOURCE_MIXER_OUT);
}

static void appKymeraCreateToneChain(const ringtone_note *tone, uint32 out_rate)
{
    kymeraTaskData *theKymera = appGetKymera();
    Operator op;

    /* Create chain with resampler */
    theKymera->chain_tone_handle = ChainCreate(&chain_tone_gen_config);

    /* Configure resampler */
    op = ChainGetOperatorByRole(theKymera->chain_tone_handle, RESAMPLER);
    OperatorsResamplerSetConversionRate(op, KYMERA_TONE_GEN_RATE, out_rate);

    /* Configure ringtone generator */
    op = ChainGetOperatorByRole(theKymera->chain_tone_handle, TONE_GEN);
    OperatorsStandardSetSampleRate(op, KYMERA_TONE_GEN_RATE);
    OperatorsConfigureToneGenerator(op, tone, &theKymera->task);

    ChainConnect(theKymera->chain_tone_handle);
}

static bool appKymeraA2dpStartMaster(const a2dp_codec_settings *codec_settings, uint8 volume)
{
    kymeraTaskData *theKymera = appGetKymera();
    uint16 mtu;
    bool cp_header_enabled;
    uint32 rate;
    uint8 seid;
    Sink sink;
    Source media_source;

    appKymeraGetA2dpCodecSettingsCore(codec_settings, &seid, &sink, &rate, &cp_header_enabled, &mtu);

    switch (theKymera->state)
    {
        case KYMERA_STATE_A2DP_STARTING_A:
        {
            const chain_config_t *config = NULL;
            bool is_left = appConfigIsLeft();
            /* Create input chain */
            switch (seid)
            {
                case AV_SEID_SBC_SNK:
                    DEBUG_LOG("appKymeraA2dpStartMaster, create standard TWS SBC input chain");
                    config = is_left ? &chain_forwarding_input_sbc_left_config :
                                       &chain_forwarding_input_sbc_right_config;
                break;
                case AV_SEID_AAC_SNK:
                    DEBUG_LOG("appKymeraA2dpStartMaster, create standard TWS AAC input chain");
                    switch (appA2dpConvertSeidFromSinkToSource(AV_SEID_AAC_SNK))
                    {
                        case AV_SEID_AAC_STEREO_TWS_SRC:
                            config = is_left ? &chain_forwarding_input_aac_stereo_left_config :
                                               &chain_forwarding_input_aac_stereo_right_config;
                        break;

                        case AV_SEID_SBC_MONO_TWS_SRC:
                            config = is_left ? &chain_forwarding_input_aac_left_config :
                                               &chain_forwarding_input_aac_right_config;
                        break;
                    }
                break;
                case AV_SEID_APTX_SNK:
                    DEBUG_LOG("appKymeraA2dpStartMaster, create standard TWS aptX input chain");
                    config = is_left ? &chain_forwarding_input_aptx_left_config :
                                       &chain_forwarding_input_aptx_right_config;
                break;
            }
            theKymera->chain_input_handle = PanicNull(ChainCreate(config));
        }
        return FALSE;

        case KYMERA_STATE_A2DP_STARTING_B:
        {
            kymera_chain_handle_t chain_handle = theKymera->chain_input_handle;
            rtp_codec_type_t rtp_codec = -1;
            Operator op;
            Operator op_rtp_decoder = ChainGetOperatorByRole(chain_handle, OPR_RTP_DECODER);
            switch (seid)
            {
                case AV_SEID_SBC_SNK:
                    DEBUG_LOG("appKymeraA2dpStartMaster, configure standard TWS SBC input chain");
                    rtp_codec = rtp_codec_type_sbc;
                    appKymeraSetLowPowerSBCParams(chain_handle, rate);
                break;
                case AV_SEID_AAC_SNK:
                    DEBUG_LOG("appKymeraA2dpStartMaster, configure standard TWS AAC input chain");
                    rtp_codec = rtp_codec_type_aac;
                    op = ChainGetOperatorByRole(chain_handle, OPR_AAC_DECODER);
                    OperatorsRtpSetAacCodec(op_rtp_decoder, op);
                    if (GET_OP_FROM_CHAIN(op, chain_handle, OPR_SPLITTER))
                    {
                        OperatorsConfigureSplitter(op, 2048, FALSE, operator_data_format_encoded);
                    }
                    if (GET_OP_FROM_CHAIN(op, chain_handle, OPR_CONSUMER))
                    {
                        appKymeraConfigureSpcDataFormat(op, TRUE);
                    }
                break;
                case AV_SEID_APTX_SNK:
                    DEBUG_LOG("appKymeraA2dpStartMaster, configure standard TWS aptX input chain");
                    rtp_codec = rtp_codec_type_aptx;
                    op = ChainGetOperatorByRole(chain_handle, OPR_APTX_DEMUX);
                    OperatorsStandardSetSampleRate(op, rate);
                    appKymeraSetLowPowerSBCParams(chain_handle, rate);
                break;
            }
            appKymeraConfigureRtpDecoder(op_rtp_decoder, rtp_codec, rate, cp_header_enabled);
            ChainConnect(theKymera->chain_input_handle);
        }
        return FALSE;

        case KYMERA_STATE_A2DP_STARTING_C:
        {
            unsigned kick_period = KICK_PERIOD_FAST;
            uint8 volume_config = appConfigEnableSoftVolumeRampOnStart() ? 0 : volume;
            DEBUG_LOG("appKymeraA2dpStartMaster, creating output chain, completing startup");
            switch (seid)
            {
                case AV_SEID_SBC_SNK:  kick_period = KICK_PERIOD_MASTER_SBC;  break;
                case AV_SEID_AAC_SNK:  kick_period = KICK_PERIOD_MASTER_AAC;  break;
                case AV_SEID_APTX_SNK: kick_period = KICK_PERIOD_MASTER_APTX; break;
            }
            OperatorsFrameworkSetKickPeriod(kick_period);
            appKymeraCreateOutputChain(rate, kick_period, PCM_LATENCY_BUFFER_SIZE, volume_config);

            /* Connect input and output chains together */
            ChainConnectInput(theKymera->chain_output_vol_handle,
                            ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM),
                            EPR_SINK_MIXER_MAIN_IN);

            /* Enable external amplifier if required */
            appKymeraExternalAmpControl(TRUE);

            /* Configure DSP for low power */
            appKymeraConfigureDspPowerMode(FALSE);

            /* Connect media source to chain */
            media_source = StreamSourceFromSink(sink);
            StreamDisconnect(media_source, 0);
            /* Ignore if this fails, it means the media_source has gone away, which
            will be cleaned up by the a2dp module */
            if (ChainConnectInput(theKymera->chain_input_handle, media_source, EPR_SINK_MEDIA))
            {
                /* Start chains */
                ChainStart(theKymera->chain_output_vol_handle);
                ChainStart(theKymera->chain_input_handle);
                if (volume_config == 0)
                {
                    /* Setting volume after start results in ramp up */
                    appKymeraSetVolume(theKymera->chain_output_vol_handle, volume);
                }
            }
        }
        return TRUE;

        default:
            Panic();
            return FALSE;
    }
}

static void appKymeraA2dpCommonStop(Source source)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraA2dpCommonStop, source(%p)", source);

    PanicNull(theKymera->chain_input_handle);
    PanicNull(theKymera->chain_output_vol_handle);

    /* If there is a tone still playing at this point, it must be interruptable,
    so cut it off. */
    appKymeraMuteVolume(theKymera->chain_output_vol_handle);

    /* Stop chains before disconnecting */
    ChainStop(theKymera->chain_input_handle);
    ChainStop(theKymera->chain_output_vol_handle);
    if (theKymera->chain_tone_handle)
    {
        ChainStop(theKymera->chain_tone_handle);
    }

    /* Disable external amplifier if required */
    appKymeraExternalAmpControl(FALSE);

    /* Disconnect A2DP source from the RTP operator then dispose */
    StreamDisconnect(source, 0);
    StreamConnectDispose(source);

    /* Destroy chains now that input has been disconnected */
    ChainDestroy(theKymera->chain_input_handle);
    theKymera->chain_input_handle = NULL;
    ChainDestroy(theKymera->chain_output_vol_handle);
    theKymera->chain_output_vol_handle = NULL;
    if (theKymera->chain_tone_handle)
    {
        ChainDestroy(theKymera->chain_tone_handle);
        theKymera->chain_tone_handle = NULL;
    }

    /* Destroy packetiser */
    if (theKymera->packetiser)
    {
        TransformStop(theKymera->packetiser);
        theKymera->packetiser = NULL;
    }
}

static void appKymeraA2dpStartForwarding(const a2dp_codec_settings *codec_settings)
{
    kymeraTaskData *theKymera = appGetKymera();
    sbc_encoder_params_t sbc_encoder_params;
    uint16 mtu;
    bool cp_enabled;
    Operator op;
    vm_transform_packetise_codec p0_codec = VM_TRANSFORM_PACKETISE_CODEC_APTX;
    vm_transform_packetise_mode mode = VM_TRANSFORM_PACKETISE_MODE_TWSPLUS;
    uint8 seid;
    Sink sink;
    kymera_chain_handle_t inchain = theKymera->chain_input_handle;

    appKymeraGetA2dpCodecSettingsCore(codec_settings, &seid, &sink, NULL, &cp_enabled, &mtu);
    appKymeraGetA2dpCodecSettingsSBC(codec_settings, &sbc_encoder_params);
    Source audio_source = ChainGetOutput(inchain, EPR_SOURCE_FORWARDING_MEDIA);
    DEBUG_LOGF("appKymeraA2dpStartForwarding, sink %p, source %p, seid %d, state %u",
                sink, audio_source, seid, theKymera->state);

    switch (seid)
    {
        case AV_SEID_APTX_MONO_TWS_SRC:
        break;

        case AV_SEID_SBC_MONO_TWS_SRC:
        {
            Operator sbc_encoder= ChainGetOperatorByRole(inchain, OPR_SBC_ENCODER);
            OperatorsSbcEncoderSetEncodingParams(sbc_encoder, &sbc_encoder_params);
            p0_codec = VM_TRANSFORM_PACKETISE_CODEC_SBC;
        }
        break;

        case AV_SEID_AAC_STEREO_TWS_SRC:
            /* The packetiser doesn't currently have a AAC codec type, but the
               behavior with aptX is the same as required for AAC. */
            p0_codec = VM_TRANSFORM_PACKETISE_CODEC_APTX;
            mode = VM_TRANSFORM_PACKETISE_MODE_TWS;
        break;

        default:
            /* Unsupported SEID, control should never reach here */
            Panic();
        break;
    }

    theKymera->packetiser = TransformPacketise(audio_source, sink);
    TransformConfigure(theKymera->packetiser, VM_TRANSFORM_PACKETISE_CODEC, p0_codec);
    TransformConfigure(theKymera->packetiser, VM_TRANSFORM_PACKETISE_MODE, mode);
    TransformConfigure(theKymera->packetiser, VM_TRANSFORM_PACKETISE_MTU, mtu);
    TransformConfigure(theKymera->packetiser, VM_TRANSFORM_PACKETISE_TIME_BEFORE_TTP, appConfigTwsTimeBeforeTx() / 1000);
    TransformConfigure(theKymera->packetiser, VM_TRANSFORM_PACKETISE_LATEST_TIME_BEFORE_TTP, appConfigTwsDeadline() / 1000);
    TransformConfigure(theKymera->packetiser, VM_TRANSFORM_PACKETISE_CPENABLE, cp_enabled);
    TransformStart(theKymera->packetiser);

    if (GET_OP_FROM_CHAIN(op, inchain, OPR_SPLITTER))
    {
        OperatorsSplitterEnableSecondOutput(op, TRUE);
    }
    if (GET_OP_FROM_CHAIN(op, inchain, OPR_SWITCHED_PASSTHROUGH_CONSUMER))
    {
        appKymeraConfigureSpcMode(op, FALSE);
    }
}

static void appKymeraA2dpStopForwarding(void)
{
    Operator op;
    kymeraTaskData *theKymera = appGetKymera();
    kymera_chain_handle_t inchain = theKymera->chain_input_handle;

    DEBUG_LOG("appKymeraA2dpStopForwarding");

    if (GET_OP_FROM_CHAIN(op, inchain, OPR_SPLITTER))
    {
        OperatorsSplitterEnableSecondOutput(op, FALSE);
    }
    if (GET_OP_FROM_CHAIN(op, inchain, OPR_SWITCHED_PASSTHROUGH_CONSUMER))
    {
        appKymeraConfigureSpcMode(op, TRUE);
    }

    if (theKymera->packetiser)
    {
        TransformStop(theKymera->packetiser);
        theKymera->packetiser = NULL;
    }

    appKymeraSetLowPowerSBCParams(inchain, theKymera->output_rate);
}

static void appKymeraA2dpStartSlave(a2dp_codec_settings *codec_settings, uint8 volume)
{
    kymeraTaskData *theKymera = appGetKymera();
    unsigned kick_period = 0;
    vm_transform_packetise_codec p0_codec = VM_TRANSFORM_PACKETISE_CODEC_APTX;
    vm_transform_packetise_mode mode = VM_TRANSFORM_PACKETISE_MODE_TWSPLUS;
    Operator op;
    uint16 mtu;
    bool cp_enabled;
    uint32 rate;
    uint8 seid;
    Sink sink;
    Source media_source;
    uint8 volume_config = appConfigEnableSoftVolumeRampOnStart() ? 0 : volume;

    appKymeraGetA2dpCodecSettingsCore(codec_settings, &seid, &sink, &rate, &cp_enabled, &mtu);

    /* Create input chain */
    switch (seid)
    {
        case AV_SEID_APTX_MONO_TWS_SNK:
        {
            DEBUG_LOG("appKymeraA2dpStartSlave, TWS+ aptX");
            theKymera->chain_input_handle = ChainCreate(&chain_aptx_mono_no_autosync_decoder_config);
            kick_period = KICK_PERIOD_SLAVE_APTX;
        }
        break;
        case AV_SEID_SBC_MONO_TWS_SNK:
        {
            DEBUG_LOG("appKymeraA2dpStartSlave, TWS+ SBC");
            theKymera->chain_input_handle = ChainCreate(&chain_sbc_mono_no_autosync_decoder_config);
            kick_period = KICK_PERIOD_SLAVE_SBC;
            p0_codec = VM_TRANSFORM_PACKETISE_CODEC_SBC;
        }
        break;
        case AV_SEID_AAC_STEREO_TWS_SNK:
        {
            DEBUG_LOG("appKymeraA2dpStartSlave, TWS AAC");
            const chain_config_t *config = appConfigIsLeft() ?
                                        &chain_aac_stereo_decoder_left_config :
                                        &chain_aac_stereo_decoder_right_config;
            theKymera->chain_input_handle = ChainCreate(config);
            if (GET_OP_FROM_CHAIN(op, theKymera->chain_input_handle, OPR_CONSUMER))
            {
                appKymeraConfigureSpcDataFormat(op, TRUE);
            }
            kick_period = KICK_PERIOD_SLAVE_AAC;
            /* The packetiser doesn't currently have a AAC codec type, but the
               behavior with aptX is the same as required for AAC. */
            p0_codec = VM_TRANSFORM_PACKETISE_CODEC_APTX;
            mode = VM_TRANSFORM_PACKETISE_MODE_TWS;
        }
        break;
        default:
            Panic();
        break;
    }
    op = ChainGetOperatorByRole(theKymera->chain_input_handle, OPR_LATENCY_BUFFER);
    OperatorsStandardSetBufferSize(op, 0x1000);
    appKymeraConfigureSpcDataFormat(op, FALSE);

    OperatorsFrameworkSetKickPeriod(kick_period);
    ChainConnect(theKymera->chain_input_handle);

    /* Configure DSP for low power */
    appKymeraConfigureDspPowerMode(FALSE);

    /* Create output chain */
    appKymeraCreateOutputChain(rate, kick_period, 0, volume_config);

    /* Connect chains together */
    ChainJoin(theKymera->chain_input_handle, theKymera->chain_output_vol_handle,
              DIMENSION_AND_ADDR_OF(slave_inter_chain_connections));

    /* Disconnect A2DP from dispose sink */
    media_source = StreamSourceFromSink(sink);
    StreamDisconnect(media_source, 0);

    theKymera->packetiser = TransformPacketise(media_source, ChainGetInput(theKymera->chain_input_handle, EPR_SINK_MEDIA));
    TransformConfigure(theKymera->packetiser, VM_TRANSFORM_PACKETISE_CODEC, p0_codec);
    TransformConfigure(theKymera->packetiser, VM_TRANSFORM_PACKETISE_MODE, mode);
    TransformConfigure(theKymera->packetiser, VM_TRANSFORM_PACKETISE_SAMPLE_RATE, (uint16)(rate & 0xffff));
    TransformConfigure(theKymera->packetiser, VM_TRANSFORM_PACKETISE_CPENABLE, cp_enabled);
    TransformStart(theKymera->packetiser);

    /* Enable external amplifier if required */
    appKymeraExternalAmpControl(TRUE);

    /* Switch to passthrough now the operator is fully connected */
    appKymeraConfigureSpcMode(op, FALSE);

    /* Start chains */
    ChainStart(theKymera->chain_input_handle);
    ChainStart(theKymera->chain_output_vol_handle);
    if (volume_config == 0)
    {
        /* Setting volume after start results in ramp up */
        appKymeraSetVolume(theKymera->chain_output_vol_handle, volume);
    }
}

static void appKymeraPreStartSanity(kymeraTaskData *theKymera)
{
    /* Can only start streaming if we're currently idle */
    PanicFalse(theKymera->state == KYMERA_STATE_IDLE);

    /* Ensure there are no audio chains already */
    PanicNotNull(theKymera->chain_input_handle);
    PanicNotNull(theKymera->chain_output_vol_handle);
}

static void appKymeraHandleInternalA2dpStart(const KYMERA_INTERNAL_A2DP_START_T *msg)
{
    kymeraTaskData *theKymera = appGetKymera();
    uint8 seid = msg->codec_settings.seid;
    uint32 rate = msg->codec_settings.rate;

    DEBUG_LOGF("appKymeraHandleInternalA2dpStart(%p), state(%u)", msg->task, theKymera->state);

    if (msg->master_pre_start_delay)
    {
        /* Send another message before starting kymera. */
        MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
        *message = *msg;
        --message->master_pre_start_delay;
        MessageSend(&theKymera->task, KYMERA_INTERNAL_A2DP_START, message);
        appKymeraSetStartingLock(theKymera);
        return;
    }

    if (theKymera->state == KYMERA_STATE_TONE_PLAYING)
    {
        /* If there is a tone still playing at this point,
         * it must be an interruptable tone, so cut it off */
        appKymeraToneStop();
    }

    if (appA2dpIsSeidNonTwsSink(seid))
    {
        switch (theKymera->state)
        {
            case KYMERA_STATE_IDLE:
            {
                appKymeraPreStartSanity(theKymera);
                theKymera->output_rate = rate;
                theKymera->a2dp_seid = seid;
                theKymera->state = KYMERA_STATE_A2DP_STARTING_A;
            }
            // fall-through
            case KYMERA_STATE_A2DP_STARTING_A:
            case KYMERA_STATE_A2DP_STARTING_B:
            case KYMERA_STATE_A2DP_STARTING_C:
            {
                if (!appKymeraA2dpStartMaster(&msg->codec_settings, msg->volume))
                {
                    /* Start incomplete, send another message. */
                    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                    *message = *msg;
                    MessageSend(&theKymera->task, KYMERA_INTERNAL_A2DP_START, message);
                    theKymera->state++;
                    return;
                }
                /* Startup is complete, now streaming */
                theKymera->state = KYMERA_STATE_A2DP_STREAMING;
            }
            break;
            default:
                Panic();
            break;
        }
    }
    else if (appA2dpIsSeidTwsSink(seid))
    {
        appKymeraPreStartSanity(theKymera);
        appKymeraA2dpStartSlave(&msg->codec_settings, msg->volume);
        theKymera->state = KYMERA_STATE_A2DP_STREAMING;
        theKymera->output_rate = rate;
        theKymera->a2dp_seid = seid;
    }
    else if (appA2dpIsSeidSource(seid))
    {
        PanicFalse(theKymera->state == KYMERA_STATE_A2DP_STREAMING);
        appKymeraA2dpStartForwarding(&msg->codec_settings);
        theKymera->state = KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING;
    }
    else
    {
        /* Unsupported SEID, control should never reach here */
        Panic();
    }
    if (msg->task)
    {
        MessageSend(msg->task, KYMERA_A2DP_START_CFM, NULL);
    }
    appKymeraClearStartingLock(theKymera);
}

static void appKymeraHandleInternalA2dpStop(const KYMERA_INTERNAL_A2DP_STOP_T *msg)
{
    kymeraTaskData *theKymera = appGetKymera();
    uint8 seid = msg->seid;

    DEBUG_LOGF("appKymeraHandleInternalA2dpStop(%p), state(%u) seid(%u)",
                msg->task, theKymera->state, seid);

    if (appA2dpIsSeidNonTwsSink(seid) || appA2dpIsSeidTwsSink(seid))
    {
        switch (theKymera->state)
        {
            case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
                appKymeraA2dpStopForwarding();
                // Fall-through
            case KYMERA_STATE_A2DP_STREAMING:
                /* Common stop code for master/slave */
                appKymeraA2dpCommonStop(msg->source);
                theKymera->output_rate = 0;
                theKymera->a2dp_seid = AV_SEID_INVALID;
                theKymera->state = KYMERA_STATE_IDLE;
            break;
            case KYMERA_STATE_IDLE:
            break;
            default:
                // Report, but ignore attempts to stop in invalid states
                DEBUG_LOGF("appKymeraHandleInternalA2dpStop(%p), invalid state(%u)",
                           msg->task, theKymera->state);
            break;
        }
    }
    else if (appA2dpIsSeidSource(seid))
    {
        if (theKymera->state == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING)
        {
            appKymeraA2dpStopForwarding();
            theKymera->state = KYMERA_STATE_A2DP_STREAMING;
        }
        /* Ignore attempts to stop forwarding when not forwarding */
    }
    else
    {
        /* Unsupported SEID, control should never reach here */
        Panic();
    }
    if (msg->task)
    {
        MessageSend(msg->task, KYMERA_A2DP_STOP_CFM, NULL);
    }
}

static void appKymeraHandleInternalA2dpSetVolume(uint16 volume)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraHandleInternalA2dpSetVolume, vol %u", volume);

    switch (theKymera->state)
    {
        case KYMERA_STATE_A2DP_STREAMING:
        case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
            appKymeraSetMainVolume(theKymera->chain_output_vol_handle, volume);
            break;

        default:
            break;
    }
}


#ifdef INCLUDE_SCOFWD
static const bool sco_forwarding_supported = TRUE;
#else
static const bool sco_forwarding_supported = FALSE;
#endif

/* SCO chain selection is wrapped in this function.
   
   This code relies on the NB and WB chains being basically 
   "the same" other than operators.
 */
static void appKymeraGetScoChainConfig(const chain_config_t **chosen,
                                       hfp_wbs_codec_mask codec,
                                       bool allow_sco_forwarding)
{
#ifndef HFP_USE_2MIC
    DEBUG_LOGF("appKymeraGetScoChainConfig. 1 Mic. Sco Forwarding : supported=%d, requested=%d",
                                                sco_forwarding_supported, allow_sco_forwarding);
#ifdef INCLUDE_SCOFWD
    if (allow_sco_forwarding)
    {
       /* Select narrowband or wideband chain depending on CODEC */
       *chosen = (codec == hfp_wbs_codec_mask_msbc) ? &chain_scofwd_wb_config :
                                                      &chain_scofwd_nb_config;
    }
    else
#endif /* INCLUDE_SCOFWD */
    {
        /* Select narrowband or wideband chain depending on CODEC */
        *chosen = (codec == hfp_wbs_codec_mask_msbc) ? &chain_sco_wb_config :
                                                       &chain_sco_nb_config;
    }
#else /* HFP_USE_2MIC */
    DEBUG_LOGF("appKymeraGetScoChainConfig. 2 Mic. Sco Forwarding : supported=%d. requested=%d",
                                                sco_forwarding_supported, allow_sco_forwarding);
    /* Select narrowband or wideband chain depending on CODEC */
    *chosen = (codec == hfp_wbs_codec_mask_msbc) ? &chain_sco_wb_2mic_config :
                                                   &chain_sco_nb_2mic_config;
#ifdef INCLUDE_SCOFWD
#error "No support for SCO forwarding with 2 Mic yet"
#endif
#endif /* HFP_USE_2MIC */
}


static void appKymeraHandleInternalScoStart(Sink audio_sink, hfp_wbs_codec_mask codec,
                                            uint8 wesco, uint16 volume)
{
    UNUSED(wesco);

    kymeraTaskData *theKymera = appGetKymera();
    const uint32_t rate = (codec == hfp_wbs_codec_mask_msbc) ? 16000 : 8000;
    const chain_config_t *chain_config;
    kymera_chain_handle_t chain;
    bool allow_scofwd = FALSE;

    DEBUG_LOGF("appKymeraHandleInternalScoStart, sink 0x%x, rate %u, wesco %u, state %u", audio_sink, rate, wesco, theKymera->state);

    if (theKymera->state == KYMERA_STATE_TONE_PLAYING)
    {
        /* If there is a tone still playing at this point,
         * it must be an interruptible tone, so cut it off */
        appKymeraToneStop();
    }

    /* Can't start voice chain if we're not idle */
    PanicFalse(theKymera->state == KYMERA_STATE_IDLE);

    /* SCO chain must be destroyed if we get here */
    PanicNotNull(theKymera->chain_sco_handle);

#ifdef INCLUDE_SCOFWD
    tp_bdaddr sink_bdaddr;

    SinkGetBdAddr(audio_sink, &sink_bdaddr);
    allow_scofwd = (FALSE == appDeviceIsTwsPlusHandset(&sink_bdaddr.taddr.addr));
#endif

    appKymeraGetScoChainConfig(&chain_config,codec,allow_scofwd);

    /* Create chain */
    chain = ChainCreate(chain_config);
    theKymera->chain_sco_handle = chain;

    /* Get sources and sinks for chain */
    Source sco_src = ChainGetOutput(chain, EPR_SCO_TO_AIR);
    Sink sco_sink = ChainGetInput(chain, EPR_SCO_FROM_AIR);
    Source audio_source = StreamSourceFromSink(audio_sink);
    Source mic_src1 = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_A);
    Sink mic_sink1 = ChainGetInput(chain, EPR_SCO_MIC1);
#ifdef HFP_USE_2MIC
    Source mic_src1b = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_B);
    Sink mic_sink1b = ChainGetInput(chain, EPR_SCO_MIC2);
#endif
    Source speaker_src = ChainGetOutput(chain, EPR_SCO_SPEAKER);
    Sink speaker_snk = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, appConfigLeftAudioChannel());

    /* Set AEC REF sample rate */
    Operator aec_op = ChainGetOperatorByRole(chain, OPR_SCO_AEC);
    OperatorsAecSetSampleRate(aec_op, rate, rate);

    Operator sco_op;
    PanicFalse(GET_OP_FROM_CHAIN(sco_op, chain, OPR_SCO_RECEIVE));

#ifdef INCLUDE_SCOFWD
    if (allow_scofwd)
    {
        Operator awbs_op;
        PanicFalse(GET_OP_FROM_CHAIN(awbs_op, chain, OPR_SCOFWD_SEND));
        OperatorsAwbsSetBitpoolValue(awbs_op, SFWD_MSBC_BITPOOL, FALSE);

        if (rate == 8000)
        {
            Operator upsampler_op;
            PanicFalse(GET_OP_FROM_CHAIN(upsampler_op, chain, OPR_SCO_UP_SAMPLE));
            OperatorsLegacyResamplerSetConversionRate(upsampler_op, 8000, 16000);
        }

        Operator splitter_op;
        PanicFalse(GET_OP_FROM_CHAIN(splitter_op, chain, OPR_SCOFWD_SPLITTER));
        OperatorsStandardSetBufferSize(splitter_op, SFWD_SEND_CHAIN_BUFFER_SIZE);
        OperatorsSplitterSetDataFormat(splitter_op, operator_data_format_pcm);

        // Configure passthrough for PCM so we can connect.
        Operator switch_op;
        PanicFalse(GET_OP_FROM_CHAIN(switch_op, chain, OPR_SWITCHED_PASSTHROUGH_CONSUMER));
        appKymeraConfigureSpcDataFormat(switch_op, TRUE);

        OperatorsStandardSetTimeToPlayLatency(sco_op, SFWD_TTP_DELAY_US);
    }
    else
#endif /* INCLUDE_SCOFWD */
    {
        /*! \todo Need to decide ahead of time if we need any latency.
            Simple enough to do if we are legacy or not. Less clear if
            legacy but no peer connection */
        /* Enable Time To Play if supported */
        if (appConfigScoChainTTP(wesco) != 0)
        {
            OperatorsStandardSetTimeToPlayLatency(sco_op, appConfigScoChainTTP(wesco));
            /*! \todo AEC Gata is V2 silicon, downloadable has native TTP support*/
            OperatorsAecEnableTtpGate(aec_op, TRUE, 50, TRUE);
        }
    }

    appKymeraConfigureOutputChainOperators(chain, rate, KICK_PERIOD_VOICE, 0, 0);

    /* Set DAC and ADC sample rate */
    SourceConfigure(mic_src1, STREAM_CODEC_INPUT_RATE, rate);
    SourceConfigure(mic_src1, STREAM_CODEC_INPUT_GAIN, appConfigMicGain());
    SourceConfigure(mic_src1, STREAM_CODEC_MIC_INPUT_GAIN_ENABLE, appConfigMicPreAmpGain());

#ifdef HFP_USE_2MIC
    SourceConfigure(mic_src1b, STREAM_CODEC_INPUT_RATE, rate);
    SourceConfigure(mic_src1b, STREAM_CODEC_INPUT_GAIN, appConfigMicGain());
    SourceConfigure(mic_src1b, STREAM_CODEC_MIC_INPUT_GAIN_ENABLE, appConfigMicPreAmpGain());
    SourceSynchronise(mic_src1,mic_src1b);
#endif
    SinkConfigure(speaker_snk, STREAM_CODEC_OUTPUT_RATE, rate);

    /* Conect it all together */
    StreamConnect(sco_src, audio_sink);
    StreamConnect(audio_source, sco_sink);
    StreamConnect(mic_src1, mic_sink1);
#ifdef HFP_USE_2MIC
    StreamConnect(mic_src1b, mic_sink1b);
#endif
    StreamConnect(speaker_src, speaker_snk);
    ChainConnect(chain);

    /* Enable external amplifier if required */
    appKymeraExternalAmpControl(TRUE);

    /* Turn on MIC bias */
    MicbiasConfigure(MIC_BIAS_0, MIC_BIAS_ENABLE, MIC_BIAS_FORCE_ON);

    /*! \todo Look at removing the conditional. Cannot start this chain
         until the SCO Forwarding elements also configured. */
    if (!allow_scofwd)
    {
        /* Start chain */
        ChainStart(chain);
    }

    /* Move to SCO active state */
    theKymera->state = KYMERA_STATE_SCO_ACTIVE;
    theKymera->output_rate = rate;
    appKymeraHandleInternalScoSetVolume(volume);
}

static void appKymeraHandleInternalScoStop(void)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraHandleInternalScoStop, state %u", theKymera->state);

    /* Stop forwarding first */
    if (theKymera->state == KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING)
    {
        PanicFalse(appKymeraHandleInternalScoStopForwardingTx());
    }

    PanicFalse(theKymera->state == KYMERA_STATE_SCO_ACTIVE);
    PanicNull(theKymera->chain_sco_handle);

    Source send_src = ChainGetOutput(theKymera->chain_sco_handle, EPR_SCO_TO_AIR);
    Sink rcv_sink = ChainGetInput(theKymera->chain_sco_handle, EPR_SCO_FROM_AIR);
    Sink send_sink1 = ChainGetInput(theKymera->chain_sco_handle, EPR_SCO_MIC1);
#ifdef HFP_USE_2MIC
    Sink send_sink1b = ChainGetInput(theKymera->chain_sco_handle, EPR_SCO_MIC2);
#endif
    Source rcv_src = ChainGetOutput(theKymera->chain_sco_handle, EPR_SCO_SPEAKER);

    /* Mute first. If there is a tone still playing at this
     * point, it must be an interruptible tone, so cut it off. */
    appKymeraMuteVolume(theKymera->chain_sco_handle);

    /* Stop chains */
    ChainStop(theKymera->chain_sco_handle);
    if (theKymera->chain_tone_handle)
    {
        ChainStop(theKymera->chain_tone_handle);
    }

    StreamDisconnect(send_src, send_sink1);
#ifdef HFP_USE_2MIC
    StreamDisconnect((Source)NULL, send_sink1b);
#endif
    StreamDisconnect(rcv_src, rcv_sink);

    /* Destroy chains */
    ChainDestroy(theKymera->chain_sco_handle);
    theKymera->chain_sco_handle = NULL;
    if (theKymera->chain_tone_handle)
    {
        ChainDestroy(theKymera->chain_tone_handle);
        theKymera->chain_tone_handle = NULL;
    }

    /* Disable external amplifier if required */
    appKymeraExternalAmpControl(FALSE);

    /* Turn off MIC bias */
    /*! \todo Check if MIC_BIAS different for 2 mic */
    MicbiasConfigure(MIC_BIAS_0, MIC_BIAS_ENABLE, MIC_BIAS_OFF);

    /* Update state variables */
    theKymera->state = KYMERA_STATE_IDLE;
    theKymera->output_rate = 0;
}

#ifdef INCLUDE_SCOFWD
static void appKymeraSfwdSetSwitchedPassthrough(switched_passthrough_states state)
{
    kymeraTaskData *theKymera = appGetKymera();

    Operator spc_op = ChainGetOperatorByRole(theKymera->chain_sco_handle, OPR_SWITCHED_PASSTHROUGH_CONSUMER);
    appKymeraConfigureSpcMode(spc_op, state);
}


static void appKymeraHandleInternalScoStartForwardingTx(Sink forwarding_sink)
{
    kymeraTaskData *theKymera = appGetKymera();

    if (   KYMERA_STATE_SCO_ACTIVE != theKymera->state
        || NULL == theKymera->chain_sco_handle)
    {
        return;
    }

    Source audio_source = ChainGetOutput(theKymera->chain_sco_handle, EPR_SCOFWD_TX_OTA);
    DEBUG_LOGF("appKymeraHandleInternalScoStartForwardingTx, sink %p, source %p, state %d",
                forwarding_sink, audio_source, theKymera->state);

    PanicNotZero(theKymera->lock);

    appScoFwdInitPacketising(audio_source);
    ChainStart(theKymera->chain_sco_handle);

    appKymeraSfwdSetSwitchedPassthrough(PASSTHROUGH_MODE);

    theKymera->state = KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING;
}

static bool appKymeraHandleInternalScoStopForwardingTx(void)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraHandleInternalScoStopForwardingTx, state %u", theKymera->state);

    if (theKymera->state != KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING)
    {
        return FALSE;
    }

    appKymeraSfwdSetSwitchedPassthrough(CONSUMER_MODE);

    theKymera->state = KYMERA_STATE_SCO_ACTIVE;

    return TRUE;
}

static void appKymeraHandleInternalScoForwardingStartRx(Source link_source, uint8 volume)
{
    kymeraTaskData *theKymera = appGetKymera();
    const uint32_t rate = 16000;
    kymera_chain_handle_t chain;

    DEBUG_LOGF("appKymeraHandleInternalScoForwardingStartRx, start source 0x%x, rate %u, state %u", link_source, rate, theKymera->state);

    PanicNotZero(theKymera->lock);

    if (theKymera->state == KYMERA_STATE_TONE_PLAYING)
    {
        /* If there is a tone still playing at this point,
         * it must be an interruptible tone, so cut it off */
        appKymeraToneStop();
    }

    /* Can't start voice chain if we're not idle */
    PanicFalse(theKymera->state == KYMERA_STATE_IDLE);

    /* SCO chain must be destroyed if we get here */
    PanicNotNull(theKymera->chain_sco_handle);

    /* Create chain */
    chain = ChainCreate(&chain_scofwd_recv_config);
    theKymera->chain_sco_handle = chain;

    /* Get sources and sinks for chain */
    Sink sco_sink = ChainGetInput(chain, EPR_SCOFWD_RX_OTA);
    PanicFalse(SinkMapInit(sco_sink, STREAM_TIMESTAMPED, AUDIO_FRAME_METADATA_LENGTH));

    appScoFwdNotifyIncomingSink(sco_sink);

    Source mic_src1 = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_A);
    Sink mic_sink1 = ChainGetInput(chain, EPR_SCO_MIC1);
#ifdef HFP_USE_2MIC
    Source mic_src1b = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_B);
    Sink mic_sink1b = ChainGetInput(chain, EPR_SCO_MIC2);
#endif
    Source speaker_src = ChainGetOutput(chain, EPR_SCO_SPEAKER);
    Sink speaker_snk = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, appConfigLeftAudioChannel());

    /* Set AEC REF sample rate */
    Operator aec_op = ChainGetOperatorByRole(chain, OPR_SCO_AEC);
    OperatorsAecSetSampleRate(aec_op, rate, rate);

    Operator awbs_op = ChainGetOperatorByRole(chain, OPR_SCOFWD_RECV);
    OperatorsAwbsSetBitpoolValue(awbs_op, SFWD_MSBC_BITPOOL,TRUE);

    Operator buffering_passthrough;
    if (GET_OP_FROM_CHAIN(buffering_passthrough, chain, OPR_SCOFWD_BUFFERING))
    {
        OperatorsStandardSetBufferSize(buffering_passthrough, SFWD_RECV_CHAIN_BUFFER_SIZE);
    }

    /*! \todo Before updating from Products, this was not muting */
    appKymeraConfigureOutputChainOperators(chain, rate, KICK_PERIOD_VOICE, 0, 0);

    /* Set DAC and ADC sample rate */
    SourceConfigure(mic_src1, STREAM_CODEC_INPUT_RATE, rate);

#ifdef HFP_USE_2MIC
    SourceConfigure(mic_src1b, STREAM_CODEC_INPUT_RATE, rate);
    SourceSynchronise(mic_src1,mic_src1b);
#endif
    SinkConfigure(speaker_snk, STREAM_CODEC_OUTPUT_RATE, rate);

    /* Conect it all together */
    PanicFalse(StreamConnect(mic_src1, mic_sink1));
#ifdef HFP_USE_2MIC
    StreamConnect(mic_src1b, mic_sink1b);
#endif
    PanicFalse(StreamConnect(speaker_src, speaker_snk));

    ChainConnect(chain);

    /* Enable external amplifier if required */
    appKymeraExternalAmpControl(TRUE);

    /* Turn on MIC bias */
    MicbiasConfigure(MIC_BIAS_0, MIC_BIAS_ENABLE, MIC_BIAS_FORCE_ON);

    /* Start chain */
    ChainStart(chain);

    /* Move to SCO active state */
    theKymera->state = KYMERA_STATE_SCOFWD_RX_ACTIVE;
    theKymera->output_rate = rate;

    appKymeraHandleInternalScoSetVolume(volume);
}

static void appKymeraHandleInternalScoForwardingStopRx(void)
{
    kymeraTaskData *theKymera = appGetKymera();
    kymera_chain_handle_t chain = theKymera->chain_sco_handle;

    DEBUG_LOGF("appKymeraScoFwdStop now, state %u", theKymera->state);

    PanicNotZero(theKymera->lock);

    PanicFalse(theKymera->state == KYMERA_STATE_SCOFWD_RX_ACTIVE);
    PanicNull(chain);

    Source mic_src1 = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_A);
    Sink mic_sink1 = ChainGetInput(chain, EPR_SCO_MIC1);
#ifdef HFP_USE_2MIC
    Source mic_src1b = StreamAudioSource(AUDIO_HARDWARE_CODEC, appConfigMicAudioInstance(), AUDIO_CHANNEL_B);
    Sink mic_sink1b = ChainGetInput(chain, EPR_SCO_MIC2);
#endif
    Source speaker_src = ChainGetOutput(chain, EPR_SCO_SPEAKER);
    Sink speaker_snk = StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, appConfigLeftAudioChannel());

    /* Mute first. If there is a tone still playing at this
     * point, it must be an interruptible tone, so cut it off. */
    appKymeraMuteVolume(chain);

    /* Stop chains */
    ChainStop(chain);
    if (theKymera->chain_tone_handle)
    {
        ChainStop(theKymera->chain_tone_handle);
    }

    StreamDisconnect(mic_src1, mic_sink1);
#ifdef HFP_USE_2MIC
    StreamDisconnect(mic_src1b, mic_sink1b);
#endif
    StreamDisconnect(speaker_src, speaker_snk);

    /* Destroy chains */
    ChainDestroy(chain);
    theKymera->chain_sco_handle = NULL;
    if (theKymera->chain_tone_handle)
    {
        ChainDestroy(theKymera->chain_tone_handle);
        theKymera->chain_tone_handle = NULL;
    }

    /* Disable external amplifier if required */
    appKymeraExternalAmpControl(FALSE);

    /* Turn off MIC bias */
    /*! \todo Check if MIC_BIAS different for 2 mic */
    MicbiasConfigure(MIC_BIAS_0, MIC_BIAS_ENABLE, MIC_BIAS_OFF);

    /* Update state variables */
    theKymera->state = KYMERA_STATE_IDLE;
    theKymera->output_rate = 0;
}

#endif /* INCLUDE_SCOFWD */

static void appKymeraHandleInternalScoSetVolume(uint8 volume)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraHandleInternalScoSetVolume, vol %u", volume);

    switch (theKymera->state)
    {
        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING:
        {
            uint16 volume_scaled = ((uint16)volume * 127) / 15;
            appKymeraSetMainVolume(theKymera->chain_sco_handle, volume_scaled);
        }
        break;

        case KYMERA_STATE_SCOFWD_RX_ACTIVE:
            appKymeraSetMainVolume(theKymera->chain_sco_handle, appConfigVolumeNoGain127Step());
            break;

        default:
            break;
    }
}

static void appKymeraHandleInternalScoMicMute(bool mute)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraHandleInternalScoMicMute, mute %u", mute);

    switch (theKymera->state)
    {
        case KYMERA_STATE_SCO_ACTIVE:
        {
            Operator aec_op;

            if (GET_OP_FROM_CHAIN(aec_op, theKymera->chain_sco_handle, OPR_SCO_AEC))
            {
                OperatorsAecMuteMicOutput(aec_op, mute);
            }
        }
        break;

        default:
            break;
    }
}


static void appKymeraHandleInternalTonePlay(const ringtone_note *tone, bool interruptible)
{
    kymeraTaskData *theKymera = appGetKymera();
    Operator op;
    kymera_chain_handle_t output_chain;
    const chain_join_roles_t *connections;
    unsigned num_connections;

    DEBUG_LOGF("appKymeraHandleInternalTonePlay, tone %p, int %u", tone, interruptible);

    /* Check if there is already a tone playing (chain exists) */
    if (theKymera->chain_tone_handle != NULL)
    {
        /* If there is a tone still playing at this point,
         * it must be an interruptible tone, so cut it off */
        appKymeraToneStop();
    }

    op = ChainGetOperatorByRole(theKymera->chain_output_vol_handle, OPR_VOLUME_CONTROL);
    output_chain = theKymera->chain_output_vol_handle;
    connections = tone_music_inter_chain_connections;
    num_connections = ARRAY_DIM(tone_music_inter_chain_connections);

    switch (theKymera->state)
    {
        case KYMERA_STATE_TONE_PLAYING:
            /* Tone already playing, control should never reach here */
            DEBUG_LOGF("appKymeraHandleInternalTonePlay, already playing, state %u", theKymera->state);
            Panic();
            break;

        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING:
        case KYMERA_STATE_SCOFWD_RX_ACTIVE:
            op = ChainGetOperatorByRole(theKymera->chain_sco_handle, OPR_VOLUME_CONTROL);
            output_chain = theKymera->chain_sco_handle;
            connections = tone_voice_inter_chain_connections;
            num_connections = ARRAY_DIM(tone_voice_inter_chain_connections);
            /* Fall through */
        case KYMERA_STATE_A2DP_STREAMING:
        case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
            /* Already playing audio, can just mix tone in at output vol AUX_IN */
            appKymeraCreateToneChain(tone, theKymera->output_rate);
            /* Mute aux in port first */
            OperatorsVolumeSetAuxGain(op, volTo60thDbGain(0));
            /* Connect tone chain to output */
            ChainJoin(theKymera->chain_tone_handle, output_chain, num_connections, connections);
            /* Unmute */
            OperatorsVolumeSetAuxGain(op, APP_UI_TONE_VOLUME * KYMERA_DB_SCALE);
            /* May need to exit low power mode to play tone simultaneously */
            appKymeraConfigureDspPowerMode(TRUE);
            /* Start tone */
            ChainStart(theKymera->chain_tone_handle);
            break;

        case KYMERA_STATE_IDLE:
            /* Need to set up audio output chain to play tone from scratch */
            appKymeraCreateOutputChain(KYMERA_TONE_GEN_RATE, KICK_PERIOD_TONES, 0, 0);
            appKymeraCreateToneChain(tone, KYMERA_TONE_GEN_RATE);
            /* Connect chains */
            output_chain = theKymera->chain_output_vol_handle;
            ChainJoin(theKymera->chain_tone_handle, output_chain, num_connections, connections);
            /* Unmute */
            op = ChainGetOperatorByRole(theKymera->chain_output_vol_handle, OPR_VOLUME_CONTROL);
            OperatorsVolumeMute(op, FALSE);
            OperatorsVolumeSetAuxGain(op, APP_UI_TONE_VOLUME * KYMERA_DB_SCALE);
            /* Enable external amplifier if required */
            appKymeraExternalAmpControl(TRUE);
            /* Start tone, Source Sync will drive main vol input with silence */
            ChainStart(theKymera->chain_output_vol_handle);
            ChainStart(theKymera->chain_tone_handle);
            /* Update state variables */
            theKymera->state = KYMERA_STATE_TONE_PLAYING;
            theKymera->output_rate = KYMERA_TONE_GEN_RATE;
            break;

        default:
            /* Unknown state / not supported */
            DEBUG_LOGF("appKymeraHandleInternalTonePlay, unsupported state %u", theKymera->state);
            Panic();
            break;
    }
    if (!interruptible)
    {
        appKymeraSetToneLock(theKymera);
    }
}

void appKymeraTonePlay(const ringtone_note *tone, bool interruptible)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraTonePlay, queue tone %p, int %u", tone, interruptible);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_TONE_PLAY);
    message->tone = tone;
    message->interruptible = interruptible;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_TONE_PLAY, message, &theKymera->lock);
}

static void appKymeraToneStop(void)
{
    kymeraTaskData *theKymera = appGetKymera();
    Operator op;

    DEBUG_LOGF("appKymeraToneStop, state %u", theKymera->state);

    /* Exit if there isn't a tone playing */
    if (theKymera->chain_tone_handle == NULL)
        return;

    op = ChainGetOperatorByRole(theKymera->chain_output_vol_handle, OPR_VOLUME_CONTROL);

    switch (theKymera->state)
    {
        case KYMERA_STATE_IDLE:
            /* No tone playing, control should never reach here */
            DEBUG_LOGF("appKymeraToneStop, already stopped, state %u", theKymera->state);
            Panic();
            break;

        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCOFWD_RX_ACTIVE:
        case KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING:
            op = ChainGetOperatorByRole(theKymera->chain_sco_handle, OPR_VOLUME_CONTROL);
            /* Fall through */
        case KYMERA_STATE_A2DP_STREAMING:
        case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
            /* Just stop tone, leave audio playing */
            OperatorsVolumeSetAuxGain(op, volTo60thDbGain(0));
            ChainStop(theKymera->chain_tone_handle);
            ChainDestroy(theKymera->chain_tone_handle);
            theKymera->chain_tone_handle = NULL;
            /* Return to low power mode (if applicable) */
            appKymeraConfigureDspPowerMode(FALSE);
            break;

        case KYMERA_STATE_TONE_PLAYING:
            /* Mute and stop all chains */
            OperatorsVolumeSetMainAndAuxGain(op, volTo60thDbGain(0));
            OperatorsVolumeMute(op, TRUE);
            ChainStop(theKymera->chain_tone_handle);
            ChainStop(theKymera->chain_output_vol_handle);
            /* Disable external amplifier if required */
            appKymeraExternalAmpControl(FALSE);
            /* Destroy all chains */
            ChainDestroy(theKymera->chain_tone_handle);
            ChainDestroy(theKymera->chain_output_vol_handle);
            theKymera->chain_tone_handle = NULL;
            theKymera->chain_output_vol_handle = NULL;
            /* Move back to idle state */
            theKymera->state = KYMERA_STATE_IDLE;
            theKymera->output_rate = 0;
            break;

        default:
            /* Unknown state / not supported */
            DEBUG_LOGF("appKymeraToneStop, unsupported state %u", theKymera->state);
            Panic();
            break;
    }

    appKymeraClearToneLock(theKymera);
}

void appKymeraA2dpStart(Task task, const a2dp_codec_settings *codec_settings,
                        uint8 volume, uint8 master_pre_start_delay)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraA2dpStart(%p)", task);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
    message->task = task;
    message->codec_settings = *codec_settings;
    message->volume = volume;
    message->master_pre_start_delay = master_pre_start_delay;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_A2DP_START,
                             message, &theKymera->lock);
}

void appKymeraA2dpStop(Task task, uint8 seid, Source source)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraA2dpStop(%p)", task);

    if (!MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_A2DP_START))
    {
        MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_STOP);
        message->task = task;
        message->seid = seid;
        message->source = source;
        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_A2DP_STOP, message, &theKymera->lock);
    }
}

bool appKymeraScoStartForwarding(Sink forwarding_sink)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraScoStartForwarding, queue sink %p, state %u", forwarding_sink, theKymera->state);

    /* Make sure we have a valid sink */
    PanicNull(forwarding_sink);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_START_FORWARDING_TX);
    message->forwarding_sink = forwarding_sink;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_START_FORWARDING_TX, message, &theKymera->lock);

    return TRUE;
}

bool appKymeraScoStopForwarding(void)
{
    kymeraTaskData *theKymera = appGetKymera();

    if (!appKymeraHandleInternalScoStopForwardingTx())
    {
        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_STOP_FORWARDING_TX, NULL, &theKymera->lock);
    }
    return TRUE;
}

void appKymeraA2dpSetVolume(uint16 volume)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraA2dpSetVolume msg, vol %u", volume);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_SET_VOL);
    message->volume = volume;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_A2DP_SET_VOL, message, &theKymera->lock);
}

static void appKymeraScoStartHelper(Sink audio_sink, hfp_wbs_codec_mask codec, uint8 wesco,
                        uint16 volume, uint8 pre_start_delay, bool conditionally)
{
    kymeraTaskData *theKymera = appGetKymera();
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_START);
    PanicNull(audio_sink);

    message->audio_sink = audio_sink;
    message->codec      = codec;
    message->wesco      = wesco;
    message->volume     = volume;
    message->pre_start_delay = pre_start_delay;

    if (conditionally)
    {
        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_START, message, &theKymera->lock);
    }
    else
    {
        MessageSend(&theKymera->task, KYMERA_INTERNAL_SCO_START, message);
    }
}
void appKymeraScoStart(Sink audio_sink, hfp_wbs_codec_mask codec, uint8 wesco,
                       uint16 volume, uint8 pre_start_delay)
{
    DEBUG_LOGF("appKymeraScoStart, queue sink 0x%x", audio_sink);
    appKymeraScoStartHelper(audio_sink, codec, wesco, volume, pre_start_delay, TRUE);
}

void appKymeraScoStop(void)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOG("appKymeraScoStop msg");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_STOP, NULL, &theKymera->lock);
}

void appKymeraScoFwdStartReceive(Source link_source, uint8 volume)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraScoFwdStartReceive, source 0x%x", link_source);

    PanicNull(link_source);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCOFWD_RX_START);
    message->link_source = link_source;
    message->volume = volume;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCOFWD_RX_START, message, &theKymera->lock);
}

void appKymeraScoFwdStopReceive(void)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOG("appKymeraScoFwdStopReceive");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCOFWD_RX_STOP, NULL, &theKymera->lock);
}

void appKymeraScoSetVolume(uint8 volume)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraScoSetVolume msg, vol %u", volume);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_SET_VOL);
    message->volume = volume;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_SET_VOL, message, &theKymera->lock);
}

void appKymeraScoMicMute(bool mute)
{
    kymeraTaskData *theKymera = appGetKymera();

    DEBUG_LOGF("appKymeraScoMicMute msg, mute %u", mute);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_MIC_MUTE);
    message->mute = mute;
    MessageSend(&theKymera->task, KYMERA_INTERNAL_SCO_MIC_MUTE, message);
}


static void kymera_dsp_msg_handler(MessageFromOperator *op_msg)
{
    PanicFalse(op_msg->len == KYMERA_OP_MSG_LEN);

    switch (op_msg->message[KYMERA_OP_MSG_WORD_MSG_ID])
    {
        case KYMERA_OP_MSG_ID_TONE_END:
            appKymeraToneStop();
        break;

        default:
        break;
    }
}

static void kymera_msg_handler(Task task, MessageId id, Message msg)
{
    UNUSED(task);
    switch (id)
    {
        case MESSAGE_FROM_OPERATOR:
            kymera_dsp_msg_handler((MessageFromOperator *)msg);
        break;

        case KYMERA_INTERNAL_A2DP_START:
            appKymeraHandleInternalA2dpStart(msg);
        break;

        case KYMERA_INTERNAL_A2DP_STOP:
            appKymeraHandleInternalA2dpStop(msg);
        break;

        case KYMERA_INTERNAL_A2DP_SET_VOL:
        {
            KYMERA_INTERNAL_A2DP_SET_VOL_T *m = (KYMERA_INTERNAL_A2DP_SET_VOL_T *)msg;
            appKymeraHandleInternalA2dpSetVolume(m->volume);
        }
        break;

        case KYMERA_INTERNAL_SCO_START:
        {
            const KYMERA_INTERNAL_SCO_START_T *m = (const KYMERA_INTERNAL_SCO_START_T *)msg;
            if (m->pre_start_delay)
            {
                /* Resends are sent unconditonally, but the lock is set blocking
                   other new messages */
                appKymeraSetStartingLock(appGetKymera());
                appKymeraScoStartHelper(m->audio_sink, m->codec, m->wesco, m->volume,
                                        m->pre_start_delay - 1, FALSE);
            }
            else
            {
                appKymeraHandleInternalScoStart(m->audio_sink, m->codec, m->wesco, m->volume);
                appKymeraClearStartingLock(appGetKymera());
            }
        }
        break;

#ifdef INCLUDE_SCOFWD
        case KYMERA_INTERNAL_SCO_START_FORWARDING_TX:
        {
            const KYMERA_INTERNAL_SCO_START_FORWARDING_TX_T *m =
                    (const KYMERA_INTERNAL_SCO_START_FORWARDING_TX_T*)msg;
            appKymeraHandleInternalScoStartForwardingTx(m->forwarding_sink);
        }
        break;

        case KYMERA_INTERNAL_SCO_STOP_FORWARDING_TX:
        {
            appKymeraHandleInternalScoStopForwardingTx();
        }
        break;
#endif

        case KYMERA_INTERNAL_SCO_SET_VOL:
        {
            KYMERA_INTERNAL_SCO_SET_VOL_T *m = (KYMERA_INTERNAL_SCO_SET_VOL_T *)msg;
            appKymeraHandleInternalScoSetVolume(m->volume);
        }
        break;

        case KYMERA_INTERNAL_SCO_MIC_MUTE:
        {
            KYMERA_INTERNAL_SCO_MIC_MUTE_T *m = (KYMERA_INTERNAL_SCO_MIC_MUTE_T *)msg;
            appKymeraHandleInternalScoMicMute(m->mute);
        }
        break;


        case KYMERA_INTERNAL_SCO_STOP:
        {
            appKymeraHandleInternalScoStop();
        }
        break;

#ifdef INCLUDE_SCOFWD
        case KYMERA_INTERNAL_SCOFWD_RX_START:
        {
            KYMERA_INTERNAL_SCOFWD_RX_START_T *m = (KYMERA_INTERNAL_SCOFWD_RX_START_T *)msg;
            appKymeraHandleInternalScoForwardingStartRx(m->link_source, m->volume);
        }
        break;

        case KYMERA_INTERNAL_SCOFWD_RX_STOP:
        {
            appKymeraHandleInternalScoForwardingStopRx();
        }
        break;
#endif

        case KYMERA_INTERNAL_TONE_PLAY:
        {
            KYMERA_INTERNAL_TONE_PLAY_T *m = (KYMERA_INTERNAL_TONE_PLAY_T *)msg;
            appKymeraHandleInternalTonePlay(m->tone, m->interruptible);
        }
        break;

        default:
        break;
    }
}

void appKymeraInit(void)
{
    kymeraTaskData *theKymera = appGetKymera();
    theKymera->task.handler = kymera_msg_handler;
    theKymera->state = KYMERA_STATE_IDLE;
    theKymera->output_rate = 0;
    theKymera->lock = 0;
    theKymera->a2dp_seid = AV_SEID_INVALID;
    appKymeraExternalAmpSetup();
#if defined(INCLUDE_SCOFWD) && defined(SFWD_USING_SQIF)
    UNUSED(bundle_config);
    ChainSetDownloadableCapabilityBundleConfig(NULL);
#else
    ChainSetDownloadableCapabilityBundleConfig(&bundle_config);
#endif
}
