/*!
\copyright  Copyright (c) 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       av_headset_chain_roles.h
\brief	    Definitions of chain roles
*/

#ifndef AV_HEADSET_CHAIN_ROLES_H
#define AV_HEADSET_CHAIN_ROLES_H

/*! These names may be used in chain operator definitions.
   If used, the chain definition should include this file using
   \verbatim <include_header name="../av_headset_chain_roles.h"/> \endverbatim and
   be configured with the attribute \verbatim generate_operator_roles_enum="False" \endverbatim.
*/
enum chain_operator_roles
{
    /*! Role identifier used for RTP decoder */
    OPR_RTP_DECODER = 0x1000,
    /*! Role identifier used for APTX demultiplexer (splitter) operator */
    OPR_APTX_DEMUX,
    /*! Role identifier used for switched passthrough operator. */
    OPR_SWITCHED_PASSTHROUGH_CONSUMER,
    /*! Role identifier used for APTX mono decoder operator */
    OPR_APTX_CLASSIC_MONO_DECODER_NO_AUTOSYNC,
    /*! Role identifier used for SBC decoder operator */
    OPR_SBC_DECODER,
    /*! Role identifier used for SBC encoder operator */
    OPR_SBC_ENCODER,
    /*! Role identifier used for the AAC decoder operator */
    OPR_AAC_DECODER,
    /*! Role identifier used for the splitter */
    OPR_SPLITTER,
    /*! Role identifer used for a consumer operator */
    OPR_CONSUMER,

    /* SCO_roles - Operator roles used in the chains for SCO audio */
        /*! The operator processing incoming SCO */
    OPR_SCO_RECEIVE,
        /*! The operator processing outgoing SCO */
    OPR_SCO_SEND,
        /*! The clear voice capture operator for incoming SCO */
    OPR_CVC_RECEIVE,
        /*! The clear voice capture operator for outgoing SCO */
    OPR_CVC_SEND,
        /*! The adaptive echo cancellation operator used in SCO audio chain */
    OPR_SCO_AEC,

        /*! Buffer added to chains when using SCO forwarding chain as 
            long-ish TTPs delay data */
    OPR_SCOFWD_BUFFERING,
        /*! Splitter operator used to double-up SCO ready for forwarding */
    OPR_SCOFWD_SPLITTER,
        /*! Encoder operator used in the SCO forwarding chain */
    OPR_SCOFWD_SEND,
        /*! Decoder operator used in the SCO forwarding receive chain */
    OPR_SCOFWD_RECV,
        /*! Re-sampler used to keep SCO forwarding at 16KHz when receiving narrowband SCO */
    OPR_SCO_UP_SAMPLE,

    /* Common_roles - Common operator roles used between chains */
        /*! Common synchronisation operator role */
    OPR_SOURCE_SYNC,
        /*! Common volume control operator role */
    OPR_VOLUME_CONTROL,

    /*! The buffering (passthrough) operator used in volume control chain */
    OPR_LATENCY_BUFFER,
};

/*! These names may be used in chain endpoint definitions.
   If used, the chain definition should include this file using
   \verbatim <include_header name="../av_headset_chain_roles.h"/>\endverbatim and
   be configured with the attribute \verbatim generate_endpoint_roles_enum="False"\endverbatim.
*/
enum chain_endpoint_roles
{
    /*! The sink of AVDTP media, typically the RTP decoder */
    EPR_SINK_MEDIA = 0x2000,
    /*! The source of decoded PCM, typically the output of a codec decoder */
    EPR_SOURCE_DECODED_PCM,
    /*! The source of encoded media for forwarding, typically the output of
        a codec encoder or a aptx demultiplexer */
    EPR_SOURCE_FORWARDING_MEDIA,
    /*! The sink of the final output volume mixer main channel */
    EPR_SINK_MIXER_MAIN_IN,
    /*! The sink of the final output volume mixer aux channel, typically used for tones */
    EPR_SINK_MIXER_TONE_IN,
    /*! The source of the final output, typically connected to a DAC output */
    EPR_SOURCE_MIXER_OUT,
    /*! The input to SCO receive portion of SCO chain */
    EPR_SCO_FROM_AIR,
    /*! The final output from SCO send portion of SCO chain */
    EPR_SCO_TO_AIR,
    /*! The first, or only, MIC input to echo cancellation of SCO chain */
    EPR_SCO_MIC1,
    /*! The second MIC input to echo cancellation of SCO chain */
    EPR_SCO_MIC2,
    /*! Additional input to volume control embedded in the SCO chain */
    EPR_SCO_VOLUME_AUX,
    /*! The speaker output from SCO chain */
    EPR_SCO_SPEAKER,

    EPR_SCOFWD_RX_OTA,
    EPR_SCOFWD_TX_OTA,
};

#ifndef CAP_ID_DOWNLOAD_AEC_REFERENCE
#define CAP_ID_DOWNLOAD_AEC_REFERENCE 0x4007
#else
#error "Now have define for downloadable AEC"
#endif



#if defined(SFWD_USING_SQIF)
#define SFWD_CAP_ID_ASYNC_WBS_ENC           CAP_ID_ASYNC_WBS_ENC
#define SFWD_CAP_ID_ASYNC_WBS_DEC           CAP_ID_ASYNC_WBS_DEC
#define SFWD_CAP_ID_SWITCHED_PASSTHROUGH    CAP_ID_SWITCHED_PASSTHROUGH_CONSUMER
#define SFWD_CAP_ID_AEC_REF                 CAP_ID_AEC_REFERENCE
#else
#define SFWD_CAP_ID_ASYNC_WBS_ENC           CAP_ID_DOWNLOAD_ASYNC_WBS_ENC
#define SFWD_CAP_ID_ASYNC_WBS_DEC           CAP_ID_DOWNLOAD_ASYNC_WBS_DEC
#define SFWD_CAP_ID_SWITCHED_PASSTHROUGH    CAP_ID_DOWNLOAD_SWITCHED_PASSTHROUGH_CONSUMER
#define SFWD_CAP_ID_AEC_REF                 CAP_ID_DOWNLOAD_AEC_REFERENCE
#endif


#endif // AV_HEADSET_CHAIN_ROLES_H
