/*!
\copyright  Copyright (c) 2015 - 2018 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       
\brief      Interface to implementation of SCO forwarding.

            This file contains code for management and use of an L2CAP link
            to the peer device, used for forwarding timestamped SCO audio.

    @startuml

    [*] -down-> SFWD_STATE_NULL : appScoFwdInit()
    SFWD_STATE_NULL : Initialising SCO forwarding module
    SFWD_STATE_NULL -down-> SFWD_STATE_INITIALISING : appScoFwdInit()

    SFWD_STATE_INITIALISING : Awaiting L2CAP registration
    SFWD_STATE_INITIALISING --> SFWD_STATE_IDLE: CL_L2CAP_REGISTER_CFM

    SFWD_STATE_IDLE : ready for connections
    SFWD_STATE_IDLE --> SFWD_STATE_CONNECTING_MASTER : SFWD_INTERNAL_LINK_CONNECT_REQ
    SFWD_STATE_IDLE --> SFWD_STATE_CONNECTING_SLAVE : CL_L2CAP_CONNECT_IND

    SFWD_STATE_CONNECTING_MASTER : waiting to connect as link master
    SFWD_STATE_CONNECTING_MASTER -down-> SFWD_STATE_CONNECTED : CL_L2CAP_CONNECT_CFM
    SFWD_STATE_CONNECTING_MASTER --> SFWD_STATE_IDLE : CL_L2CAP_CONNECT_CFM (fail)

    SFWD_STATE_CONNECTING_SLAVE : waiting to connect as link slave
    SFWD_STATE_CONNECTING_SLAVE -down-> SFWD_STATE_CONNECTED : CL_L2CAP_CONNECT_CFM

    SFWD_STATE_CONNECTED : L2CAP link up, but no audio traffic
    SFWD_STATE_CONNECTED --> SFWD_STATE_ACTIVE_SEND : appScoFwdInitPacketising()
    SFWD_STATE_CONNECTED --> SFWD_STATE_ACTIVE_RECEIVE : SFWD_INTERNAL_START_CHAIN
    SFWD_STATE_CONNECTED --> SFWD_STATE_IDLE: CON_MANAGER_CONNECTION_IND
    SFWD_STATE_CONNECTED --> SFWD_STATE_IDLE: CL_L2CAP_DISCONNECT_IND

    SFWD_STATE_ACTIVE_RECEIVE : Audio chain, and link should be up - receiving data
    SFWD_STATE_ACTIVE_RECEIVE --> SFWD_STATE_CONNECTED : SFWD_INTERNAL_STOP_CHAIN
    SFWD_STATE_ACTIVE_RECEIVE --> SFWD_STATE_IDLE: CON_MANAGER_CONNECTION_IND
    SFWD_STATE_ACTIVE_RECEIVE --> SFWD_STATE_IDLE: CL_L2CAP_DISCONNECT_IND

    SFWD_STATE_ACTIVE_SEND : Audio chain, and link should be up - receiving data
    SFWD_STATE_ACTIVE_SEND --> SFWD_STATE_CONNECTED : Forwarding stopped
    SFWD_STATE_ACTIVE_SEND --> SFWD_STATE_CONNECTED : SFWD_INTERNAL_STOP_CHAIN
    SFWD_STATE_ACTIVE_SEND --> SFWD_STATE_IDLE: CON_MANAGER_CONNECTION_IND
    SFWD_STATE_ACTIVE_SEND --> SFWD_STATE_IDLE: CL_L2CAP_DISCONNECT_IND

    @enduml
*/

#ifndef _AV_HEADSET_SCO_FWD_H_
#define _AV_HEADSET_SCO_FWD_H_

#ifdef INCLUDE_SCOFWD


#define SFWD_STATE_LOCK_MASK (1 << 4)

/*! \brief SCO Forwarding task state machine states */
typedef enum
{
    SFWD_STATE_NULL              = 0,                        /*!< Startup state */
    SFWD_STATE_INITIALISING      = 1 + SFWD_STATE_LOCK_MASK, /*!< Awaiting L2CAP registration */
    SFWD_STATE_IDLE              = 2,                        /*!< Initialised and ready for connections */
    SFWD_STATE_SDP_SEARCH        = 3 + SFWD_STATE_LOCK_MASK,
    SFWD_STATE_CONNECTING_MASTER = 4 + SFWD_STATE_LOCK_MASK,
    SFWD_STATE_CONNECTING_SLAVE  = 5 + SFWD_STATE_LOCK_MASK,
    SFWD_STATE_CONNECTED         = 6,                        /*!< L2CAP link up, but no traffic */
        SFWD_STATE_CONNECTED_ACTIVE_RECEIVE = 7,             /*!< Chain should be up, receiving data */
        SFWD_STATE_CONNECTED_ACTIVE_SEND    = 8,             /*!< Chain should be up, sending data */
    SFWD_STATE_DISCONNECTING     = 9 + SFWD_STATE_LOCK_MASK,
} scoFwdState;


/*! \brief SCO Forwarding task data
*/
typedef struct
{
    TaskData        task;
    uint16          lock;
    scoFwdState     state;                      /*!< Current state of the state machine */
    uint16          local_psm;                  /*!< L2CAP PSM registered */
    uint16          remote_psm;                 /*!< L2CAP PSM registered by peer device */
    Sink            link_sink;                  /*!< The sink of the L2CAP link */
    Source          link_source;                /*!< The source of the L2CAP link */
    Source          source;                     /*!< The audio source */
    Sink            sink;                       /*!< The audio sink */

    uint16          link_retries;               /*!< Number of consecutive attempts to connect L2CAP */

    wallclock_state_t wallclock;                /*!< Structure giving us a common timebase with the peer */
    rtime_t         ttp_of_last_received_packet;/*!< Tracking information for packets over the air */ 

    bool            peer_incoming_call;         /*!< We are slave, and peer has incoming call */

    unsigned        lost_packets;               /*!< Number of incoming forwarded packets lost or late */
    uint32          packet_history;             /*!< Bit mask showing missed packets in the last 32 */


#ifdef INCLUDE_SCOFWD_TEST_MODE
    unsigned        percentage_to_drop;         /*!< Percentage of packets to not transmit */
    int             drop_multiple_packets;      /*!< A negative number indicates the number of consecutive 
                                                     packets to drop. 0, or a positive number indicates the
                                                     percentage chance a packet should drop after a packet
                                                     was dropped. */
#endif
} scoFwdTaskData;


/*! \brief Message IDs from SCO Forwarding to main application task */
enum av_headset_scofwd_messages
{
    SFWD_INIT_CFM = SFWD_MESSAGE_BASE,

};






#define appScoFwdGetSink()  (appGetScoFwd()->link_sink)


extern void appScoFwdInit(void);

extern void appScoFwdConnectPeer(void);
extern void appScoFwdDisconnectPeer(void);

extern void appScoFwdInitPacketising(Source audio_source);

extern bool appScoFwdHasConnection(void);
extern bool appScoFwdIsStreaming(void);
extern bool appScoFwdIsReceiving(void);

extern void appScoFwdNotifyIncomingSink(Sink sco_sink);

extern bool appScoFwdIsCallIncoming(void);

extern void appScoFwdCallAccept(void);
extern void appScoFwdCallReject(void);
extern void appScoFwdCallHangup(void);
extern void appScoFwdVolumeUp(int step);
extern void appScoFwdVolumeDown(int step);

/*! \todo PSM to be retrieved from the seid */
#define SFWD_L2CAP_LINK_PSM (0x1043) /* has to be odd */

/*! Number of attempts we will make to connect to our peer at
    any one time before giving up */
#define SFWD_L2CAP_MAX_ATTEMPTS     5


    /*! Time to play delay in &mu;s */
#define SFWD_TTP_DELAY_US           (appConfigScoFwdVoiceTtpMs() * 1000)


/*! Size of buffer to use in the send chain. The buffer is required
    to compensate for Time To Play data being backed up before the 
    AEC & speaker output */
#define SFWD_SEND_CHAIN_BUFFER_SIZE     2048

/*! Size of buffer to use in the recv chain. The buffer is required
    to compensate for Time To Play data being backed up. Smaller
    than the send side as transfer over the air uses up some of the
    Time To Play */
#define SFWD_RECV_CHAIN_BUFFER_SIZE     1024


/*! Size of SCO metadata at the start of a SCO block */
#define SFWD_SCO_METADATA_SIZE    (5 * sizeof(uint16))

/*! Amount of data to claim if sending an incorrect packet into the
    WBS decoder, so as to generate a PLC frame */
#define SFWD_WBS_DEC_KICK_SIZE    SFWD_SCO_METADATA_SIZE

/*! Size of header used on the forwarding link. 24 bit TTP only */
#define SFWD_TX_PACKETISER_FRAME_HDR_SIZE   3

/*! Time before the last TTP that we want to feed in "missing" metadata 
 *  Note we have 7500us of data 
 */
#define SFWD_RX_PROCESSING_TIME_NORMAL_US 8000
#define SFWD_PACKET_INTERVAL_US           7500
#define SFWD_PACKET_INTERVAL_MARGIN_US    ((SFWD_PACKET_INTERVAL_US)/2)

/*! Size of bitpool to use for the asynchronouse WBS.
    26 is the same quality of encoding use for wideband SCO (mSBC),
    but won't allow the use of single slot packets */
#define SFWD_MSBC_BITPOOL                22

/*! Number of octets produced by the WBS Encoding for an audio frame */
#define SFWD_AUDIO_FRAME_OCTETS          (((11 + ((15*SFWD_MSBC_BITPOOL + 7)/8)) / 2) * 2 )

/*! Number of octets of WBS header that can be stripped over the air */
#define SFWD_STRIPPED_HEADER_SIZE           5

/*! Size of an audio frame over the air (accounts for header removal) */
#define SFWD_STRIPPED_AUDIO_FRAME_OCTETS    (SFWD_AUDIO_FRAME_OCTETS - SFWD_STRIPPED_HEADER_SIZE)

/* The implementation was based on keeping the audio down to a single slot
   packet. Ensure that the bitpool selected will work with a 2DH1 packet
   allowing for the 4 octet L2CAP header */
#if (SFWD_STRIPPED_AUDIO_FRAME_OCTETS + SFWD_TX_PACKETISER_FRAME_HDR_SIZE) > (54 -4)
#error "bitpool makes packet too large for 2DH1"
#endif


/*! Although there is an accurate time source between each end of
    the SCO forwarding link, processing offsets arise between each end
    of the link.

    This is made up of 4 parts.

    - The WBS encode/decode (including PLC), buffers some data before
      output which offsets the TTP compared to the input (6063).
    - The Packet Loss Concealment skips a complete frame as the system
      is using non standard packet sizes (7500).
    - Small offset due to the alignment of samples to the local clock 
      (not compensated as varies with clock alignment)
      This is not compensated for as direction and actual value vary.
    - Unexplained, but repeatable offset (not compensated, ~13us)

    The receive end of the link 

    \todo Remove the 7500 offset once compensated for.

    \note The 6063 (97 samples) was not seen in tests, instead a value of 6050
    was consistent. This has been added as an unexplained offset.
    */
#define SFWD_WBS_PROCESSING_OFFSET_US           (6063 + 7500)

#define SFWD_WBS_UNCOMPENSATED_OFFSET_US        (SFWD_WBS_PROCESSING_OFFSET_US - 6063)

/*! Minimum amount of time for packet to get to receiver and still 
    be processed */
#define SFWD_MIN_TRANSIT_TIME_US  (SFWD_PACKET_INTERVAL_US \
                                   + SFWD_WBS_PROCESSING_OFFSET_US \
                                   + SFWD_RX_PROCESSING_TIME_NORMAL_US)


/*! Set a limit on the number of frames we can be behind on
    in the transmit processor. If there appear to be more than 
    this number of frames waiting, then the earliest ones
    will be discarded */
#define SFWD_TX_PACKETISER_MAX_FRAMES_BEHIND    3

    /*! Preferred flush setting.
        Flush should be set lower than the Time to Play delay, as there is
        a processing delay before the packets are queued and after receipt.
        Packets may also contain more than 1 frame so although the first 
        frame in a packet might be late the later ones could 
        still be usable. */
#define SFWD_FLUSH_TARGET_MS    (appConfigScoFwdVoiceTtpMs() \
                                 - (SFWD_RX_PROCESSING_TIME_NORMAL_US/2)/1000 \
                                 - SFWD_PACKET_INTERVAL_US/1000)
#define SFWD_FLUSH_TARGET_SLOTS ((SFWD_FLUSH_TARGET_MS * 1000)/ US_PER_SLOT)

#define SFWD_FLUSH_MIN_MS    (SFWD_FLUSH_TARGET_MS / 2)
#define SFWD_FLUSH_MAX_MS    (SFWD_FLUSH_TARGET_MS * 3 / 2)
#define SFWD_FLUSH_MIN_US    (SFWD_FLUSH_MIN_MS * 1000)
#define SFWD_FLUSH_MAX_US    (SFWD_FLUSH_MAX_MS * 1000)

#define SFWD_LATE_PACKET_OFFSET_TIME_US SFWD_RX_PROCESSING_TIME_NORMAL_US

/*! Messages sent between devices for SCO forwarding */
enum av_headset_scofwd_ota_messages
{
        /*! Forwarded SCO is about to start. Sent to receiving peer. */
    SFWD_OTA_MSG_SETUP = 0x01,
        /*! Forwarded SCO is about to end. Sent to receiving peer.*/
    SFWD_OTA_MSG_TEARDOWN,
        /*! Notify peer that there is an incoming SCO call */
    SFWD_OTA_MSG_INCOMING_CALL,
        /*! Notify peer that incoming SCO call has terminated */
    SFWD_OTA_MSG_INCOMING_ENDED,

        /*! Notify the earbud with the call, that volume up was selected on the peer */
    SFWD_OTA_MSG_VOLUME_UP = 0x41,
        /*! Notify the earbud with the call, that volume down was selected on the peer */
    SFWD_OTA_MSG_VOLUME_DOWN,
        /*! Notify the earbud with the call, that the used requested answer of incoming call */
    SFWD_OTA_MSG_CALL_ANSWER,
        /*! Notify the earbud with the call, that the peer requested reject of incoming call */
    SFWD_OTA_MSG_CALL_REJECT,
        /*! Notify the earbud with the call, that the peer requested hangup of current call */
    SFWD_OTA_MSG_CALL_HANGUP,
};


#else  /* INCLUDE_SCOFWD */

#define SFWD_SEND_CHAIN_BUFFER_SIZE 1024

/*! Value used if we have no SCO forwarding, but want to use AEC to
    synchronise SCO playback between earbuds */
#define SFWD_TTP_DELAY_US           (20000)


#define appScoFwdHasConnection()    (FALSE)
#define appScoFwdIsStreaming()      (FALSE)
#define appScoFwdIsReceiving()      (FALSE)

#define appScoFwdGetSink()          ((Sink)NULL)

#define appScoFwdConnectPeer()      ((void)0)

#define appScoFwdIsCallIncoming()   (FALSE)

#define appScoFwdCallAccept()       ((void)0)
#define appScoFwdCallReject()       ((void)0)
#define appScoFwdCallHangup()       ((void)0)
#define appScoFwdVolumeUp(vol)      ((void)0)
#define appScoFwdVolumeDown(vol)    ((void)0)

#endif /* INCLUDE_SCOFWD */

extern void appScoFwdHandleHfpAudioConnectConfirmation(const HFP_AUDIO_CONNECT_CFM_T *cfm);
extern void appScoFwdHandleHfpAudioDisconnectIndication(const HFP_AUDIO_DISCONNECT_IND_T *ind);

#endif /* _AV_HEADSET_SCO_FWD_H_ */

