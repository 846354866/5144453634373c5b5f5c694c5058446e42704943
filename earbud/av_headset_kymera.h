/*!
\copyright  Copyright (c) 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       av_headset_kymera.h
\brief      Header file for the Kymera Manager

*/

#ifndef AV_HEADSET_KYMERA_H
#define AV_HEADSET_KYMERA_H

#include <chain.h>
#include <transform.h>

#include "av_headset.h"

/*! \brief The kymera module states. */
typedef enum app_kymera_states
{
    /*! Kymera is idle. */
    KYMERA_STATE_IDLE,
    /*! Starting master A2DP kymera in three steps. */
    KYMERA_STATE_A2DP_STARTING_A,
    KYMERA_STATE_A2DP_STARTING_B,
    KYMERA_STATE_A2DP_STARTING_C,
    /*! Kymera is streaming A2DP locally. */
    KYMERA_STATE_A2DP_STREAMING,
    /*! Kymera is streaming A2DP locally and forwarding to the slave. */
    KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING,
    /*! Kymera is streaming SCO locally. */
    KYMERA_STATE_SCO_ACTIVE,
    /*! Kymera is streaming SCO locally, and may be forwarding */
    KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING,
    /*! Kymera is receiving forwarded SCO over a link */
    KYMERA_STATE_SCOFWD_RX_ACTIVE,
    /*! Kymera is playing a tone. */
    KYMERA_STATE_TONE_PLAYING,
} appKymeraState;


/*! \brief Kymera instance structure.

    This structure contains all the information for Kymera audio chains.
*/
typedef struct
{
    /*! The kymera module's task. */
    TaskData          task;
    /*! The current state. */
    appKymeraState    state;

    /*! The input chain is used in TWS master and slave roles for A2DP streaming
        and is typified by containing a decoder. */
    kymera_chain_handle_t chain_input_handle;
    /*! The tone chain is used when a tone is played. */
    kymera_chain_handle_t chain_tone_handle;
    /*! The volume/output chain is used in TWS master and slave roles for A2DP
        streaming. */
    kymera_chain_handle_t chain_output_vol_handle;
    /*! The SCO chain is used for SCO audio. */
    kymera_chain_handle_t chain_sco_handle;

    /*! The TWS master packetiser transform packs compressed audio frames
        (SBC, AAC, aptX) from the audio subsystem into TWS packets for transmission
        over the air to the TWS slave.
        The TWS slave packetiser transform receives TWS packets over the air from
        the TWS master. It unpacks compressed audio frames and writes them to the
        audio subsystem. */
    Transform packetiser;

    /*! The current output sample rate. */
    uint32 output_rate;
    /*! A lock bitfield. Internal messages are typically sent conditionally on
        this lock meaning events are queued until the lock is cleared. */
    uint16 lock;
    /*! The current A2DP stream endpoint identifier. */
    uint8  a2dp_seid;

} kymeraTaskData;

/*! \brief Internal message IDs */
enum app_kymera_internal_message_ids
{
    /*! Internal A2DP start message. */
    KYMERA_INTERNAL_A2DP_START,
    /*! Internal A2DP stop message. */
    KYMERA_INTERNAL_A2DP_STOP,
    /*! Internal A2DP set volume message. */
    KYMERA_INTERNAL_A2DP_SET_VOL,
    /*! Internal SCO start message, including start of SCO forwarding (if supported). */
    KYMERA_INTERNAL_SCO_START,
    /*! Internal message to start SCO forwarding */
    KYMERA_INTERNAL_SCO_START_FORWARDING_TX,
    /*! Internal message to stop forwarding SCO */
    KYMERA_INTERNAL_SCO_STOP_FORWARDING_TX,
    /*! Internal message to set SCO volume */
    KYMERA_INTERNAL_SCO_SET_VOL,
    /*! Internal SCO stop message. */
    KYMERA_INTERNAL_SCO_STOP,
    /*! Internal SCO microphone mute message. */
    KYMERA_INTERNAL_SCO_MIC_MUTE,
    /*! Internal message indicating that forwarded SCO (incoming) is/will be active */
    KYMERA_INTERNAL_SCOFWD_RX_START,
    /*! Internal message to stop playing forwarded SCO */
    KYMERA_INTERNAL_SCOFWD_RX_STOP,
    /*! Internal tone play message. */
    KYMERA_INTERNAL_TONE_PLAY,
};

/*! \brief External message IDs */
typedef enum app_kymera_external_message_ids
{
    /*! Start confirmation. */
    KYMERA_A2DP_START_CFM = KYMERA_MESSAGE_BASE,
    /*! Stop confirmation. */
    KYMERA_A2DP_STOP_CFM,
    /*! End of external messages. */
    KYMERA_MESSAGE_TOP,
} kymeraMessages;

/*! \brief The #KYMERA_INTERNAL_A2DP_START message content. */
typedef struct
{
    /*! The client task requesting the start */
    Task task;
    /*! The A2DP codec settings */
    a2dp_codec_settings codec_settings;
    /*! The starting volume */
    uint8 volume;
    /*! The number of times remaining the kymera module will resend this message to
        itself (having entered the locked KYMERA_STATE_A2DP_STARTING) state before
        proceeding to commence starting kymera. Starting will commence when received
        with value 0. Only applies to starting the master. */
    uint8 master_pre_start_delay;
} KYMERA_INTERNAL_A2DP_START_T;


/*! \brief The #KYMERA_INTERNAL_A2DP_SET_VOL message content. */
typedef struct
{
    /*! The volume to set. */
    uint16 volume;
} KYMERA_INTERNAL_A2DP_SET_VOL_T;

/*! \brief The #KYMERA_INTERNAL_A2DP_STOP message content. */
typedef struct
{
    /*! The client task requesting the stop */
    Task task;
    /*! The A2DP seid */
    uint8 seid;
    /*! The media sink */
    Source source;
} KYMERA_INTERNAL_A2DP_STOP_T;

/*! \brief The #KYMERA_INTERNAL_SCO_START message content. */
typedef struct
{
    /*! The SCO audio sink. */
    Sink audio_sink;
    /*! WB-Speech codec bit masks. */
    hfp_wbs_codec_mask codec;
    /*! The link Wesco. */
    uint8 wesco;
    /*! The starting volume. */
    uint8 volume;
    /*! The number of times remaining the kymera module will resend this message to
        itself before starting kymera SCO. */
    uint8 pre_start_delay;
} KYMERA_INTERNAL_SCO_START_T;

typedef struct
{
    /*! The audio source from the air */
    Source link_source;
    /*! The starting volume */
    uint8 volume;
} KYMERA_INTERNAL_SCOFWD_RX_START_T;

typedef struct
{
    Sink forwarding_sink;
} KYMERA_INTERNAL_SCO_START_FORWARDING_TX_T;


/*! \brief The #KYMERA_INTERNAL_SCO_SET_VOL message content. */
typedef struct
{
    /*! The volume to set. */
    uint8 volume;
} KYMERA_INTERNAL_SCO_SET_VOL_T;


/*! \brief The #KYMERA_INTERNAL_SCO_MIC_MUTE message content. */
typedef struct
{
    /*! TRUE to enable mute, FALSE to disable mute. */
    bool mute;
} KYMERA_INTERNAL_SCO_MIC_MUTE_T;

/*! \brief #KYMERA_INTERNAL_TONE_PLAY message content */
typedef struct
{
    /*! Pointer to he ringtone structure to play. */
    const ringtone_note *tone;
    /*! If TRUE, the tone may be interrupted by another event before it is
        completed. If FALSE, the tone may not be interrupted by another event
        and will play to completion. */
    bool interruptible;
} KYMERA_INTERNAL_TONE_PLAY_T;


/*! \brief Start streaming A2DP audio.
    \param task The client task.
    \param codec_settings The A2DP codec settings.
    \param volume The start volume.
    \param master_pre_start_delay This function always sends an internal message
    to request the module start kymera. The internal message is sent conditionally
    on the completion of other activities, e.g. a tone. The caller may request
    that the internal message is sent master_pre_start_delay additional times before the
    start of kymera commences. The intention of this is to allow the caller to
    delay the starting of kymera (with its long, blocking functions) to match
    the message pipeline of some concurrent message sequence the caller doesn't
    want to be blocked by the starting of kymera. This delay is only applied
    when starting the 'master' (a non-TWS sink SEID).

    \note The client task will receive a #KYMERA_A2DP_START_CFM on kymera start completion.
*/
void appKymeraA2dpStart(Task task, const a2dp_codec_settings *codec_settings,
                        uint8 volume, uint8 master_pre_start_delay);

/*! \brief Stop streaming A2DP audio.
    \param task The client task.
    \param seid The stream endpoint ID to stop.
    \param source The source associatied with the seid.

    \note The client task will receive a #KYMERA_A2DP_STOP_CFM on kymera stop completion.
*/
void appKymeraA2dpStop(Task task, uint8 seid, Source source);

/*! \brief Set the A2DP streaming volume.
    \param volume The desired volume in the range 0 (mute) to 127 (max).
*/
void appKymeraA2dpSetVolume(uint16 volume);

/*! \brief Start SCO audio.
    \param audio_sink The SCO audio sink.
    \param codec WB-Speech codec bit masks.
    \param wesco The link Wesco.
    \param volume The starting volume.
    \param pre_start_delay This function always sends an internal message
    to request the module start SCO. The internal message is sent conditionally
    on the completion of other activities, e.g. a tone. The caller may request
    that the internal message is sent pre_start_delay additional times before
    starting kymera. The intention of this is to allow the caller to
    delay the start of kymera (with its long, blocking functions) to match
    the message pipeline of some concurrent message sequence the caller doesn't
    want to be blocked by the starting of kymera.
*/
void appKymeraScoStart(Sink audio_sink, hfp_wbs_codec_mask codec, uint8 wesco,
                       uint16 volume, uint8 pre_start_delay);


/*! \brief Stop SCO audio.
*/
void appKymeraScoStop(void);

/*! \brief Start a chain for receiving forwarded SCO audio.
*/
void appKymeraScoFwdStartReceive(Source link_source, uint8 volume);

/*! \brief Stop chain receiving forwarded SCO audio.
*/
void appKymeraScoFwdStopReceive(void);

/*! \brief Start sending forwarded audio.

    \note If the SCO is to be forwarded then the full chain,
    including local playback, is started by this call.
*/
bool appKymeraScoStartForwarding(Sink forwarding_sink);

/*! \brief Stop sending received SCO audio to the peer

    Local playback of SCO will continue, although in most cases
    will be stopped by a separate command almost immediately.
 */
bool appKymeraScoStopForwarding(void);

/*! \brief Set SCO volume.
    \param volume [IN] HFP volume in the range 0 (mute) to 15 (max).
 */
void appKymeraScoSetVolume(uint8 volume);

/*! \brief Enable or disable MIC muting.
    \param mute [IN] TRUE to mute MIC, FALSE to unmute MIC.
 */
void appKymeraScoMicMute(bool mute);

/*! \brief Play a tone.
    \param tone The address of the tone to play.
    \param interruptible If TRUE, the tone may be interrupted by another event
           before it is completed. If FALSE, the tone may not be interrupted by
           another event and will play to completion.
*/
void appKymeraTonePlay(const ringtone_note *tone, bool interruptible);

/*! \brief Initialise the kymera module. */
void appKymeraInit(void);

#endif // AV_HEADSET_KYMERA_H
