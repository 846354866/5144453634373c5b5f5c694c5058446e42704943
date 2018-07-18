/*!
\copyright  Copyright (c) 2008 - 2018 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       
\brief      Implementation of SCO forwarding.

            This file covers management of a single L2CAP link to the peer device
            and transmission of data over the link.

            There is some interaction with \ref av_headset_kymera.c, and 
            \ref av_headset_con_manager.c.
*/
#define SFWD_DEBUG

#include <audio.h>
#include <panic.h>
#include <ps.h>
#include <system_clock.h>
#include <rtime.h>
#include <bluestack/hci.h>
#include <bluestack/l2cap_prim.h>
#include <tws_packetiser.h>
#include <util.h>
#include <service.h>
#include "av_headset.h"
#include "av_headset_log.h"
#include "av_headset_sdp.h"

#ifdef INCLUDE_SCOFWD

#define US_TO_MS(us) ((us) / 1000)

static scoFwdState appScoFwdGetState(void);
static void appScoFwdProcessReceivedAirPacket(uint16 avail);

/* SCO TTP Management functions
 The following are used for recording a received TTP, finding the next one
 and identifying any duplicates from late packets / race conditions.
*/
static bool appScoFwdTTPIsExpected(rtime_t new_ttp);
static void set_last_received_ttp(rtime_t ttp_passed_down);
static void clear_last_received_ttp(void);
static bool get_next_expected_ttp(rtime_t *next_ttp);
static bool get_next_ttp_before(rtime_t received_ttp,rtime_t *target_time);
static bool have_no_received_ttp(void);

/* Functions to manage packets arriving late.
   A timer is set for an interval before the expected TTP
   such that we have time to trigger a fake packet.
 */
static void handle_late_packet_timer(void);
static void start_late_packet_timer(rtime_t last_received_TTP);
static void cancel_late_packet_timer(void);

    /* Which leads to faking packets at just the right time */
static void insert_fake_packet_at(rtime_t new_ttp,rtime_t debug_ttp);
void insert_fake_packets_before(rtime_t next_received_ttp,rtime_t debug_ttp);

static void appScoFwdProcessReceivedAirFrame(const uint8 **pSource, uint8 frame_length);
static void appScoFwdKickProcessing(void);
static void appScoFwdSetState(scoFwdState new_state);

#define SHORT_TTP(x) (((x)/1000)%1000)


/*! Macro for creating messages */
#define MAKE_SFWD_MESSAGE(TYPE) \
    SFWD_##TYPE##_T *message = PanicUnlessNew(SFWD_##TYPE##_T);

#define MAKE_SFWD_INTERNAL_MESSAGE(TYPE) \
    SFWD_INTERNAL_##TYPE##_T *message = PanicUnlessNew(SFWD_INTERNAL_##TYPE##_T);

#define assert(x) PanicFalse(x)


/*! \brief Internal message IDs */
enum
{
    SFWD_INTERNAL_BASE,
    SFWD_INTERNAL_LINK_CONNECT_REQ = SFWD_INTERNAL_BASE,
    SFWD_INTERNAL_LINK_DISCONNECT_REQ,
    SFWD_INTERNAL_START_RX_CHAIN,
    SFWD_INTERNAL_STOP_RX_CHAIN,
    SFWD_INTERNAL_KICK_PROCESSING,

    SFWD_TIMER_BASE = SFWD_INTERNAL_BASE + 0x80,
    SFWD_TIMER_LATE_PACKET = SFWD_TIMER_BASE,
};


#define TTP_STATS_RANGE         (appConfigScoFwdVoiceTtpMs())
#define TTP_STATS_NUM_CELLS     20
#define TTP_STATS_CELL_SIZE     (TTP_STATS_RANGE / TTP_STATS_NUM_CELLS)
#define TTP_STATS_MIN_VAL       (  appConfigScoFwdVoiceTtpMs() \
                                 - US_TO_MS(SFWD_RX_PROCESSING_TIME_NORMAL_US) \
                                 - TTP_STATS_NUM_CELLS * TTP_STATS_CELL_SIZE)
#define TTP_STATS_MAX_VAL       (TTP_STATS_MIN_VAL + TTP_STATS_CELL_SIZE * TTP_STATS_NUM_CELLS)

typedef struct {
    uint32 entries;
    uint32 sum;
} TTP_STATS_CELL;
TTP_STATS_CELL ttp_stats[TTP_STATS_NUM_CELLS+2] = {0};


static void ttp_stats_add(unsigned ttp_in_future_ms)
{
    int cell;

    if (ttp_in_future_ms < TTP_STATS_MIN_VAL)
    {
        cell = TTP_STATS_NUM_CELLS;
    }
    else if (ttp_in_future_ms >= TTP_STATS_MAX_VAL)
    {
        cell = TTP_STATS_NUM_CELLS + 1;
    }
    else 
    {
        cell = (ttp_in_future_ms - TTP_STATS_MIN_VAL) / TTP_STATS_CELL_SIZE;
        if (cell > TTP_STATS_NUM_CELLS + 1)
            Panic();
    }

    ttp_stats[cell].entries++;
    ttp_stats[cell].sum += ttp_in_future_ms;
}

static void ttp_stats_print_cell(unsigned minval,unsigned maxval,unsigned entries,unsigned average)
{
    DEBUG_LOGF("\t%4d,%-4d,\t%6d,\t%4d",minval,maxval,entries,average);
}

void ttp_stats_print(void);
void ttp_stats_print(void)
{
    int cell;

    DEBUG_LOG("TTP STATS");
    DEBUG_LOG("\tCELL RANGE,\tNum,\tAverage");

    for (cell = 0;cell < TTP_STATS_NUM_CELLS;cell++)
    {
        unsigned minval = TTP_STATS_MIN_VAL + (cell * TTP_STATS_CELL_SIZE);
        unsigned maxval = minval + TTP_STATS_CELL_SIZE - 1;
        unsigned average = ttp_stats[cell].entries ? ((ttp_stats[cell].sum + ttp_stats[cell].entries/2)/ttp_stats[cell].entries) : 0;
        ttp_stats_print_cell(minval,maxval,ttp_stats[cell].entries,average);
    }
    if (ttp_stats[cell].entries)
    {
        ttp_stats_print_cell(-1000,TTP_STATS_MIN_VAL -1,ttp_stats[cell].entries,(ttp_stats[cell].sum + ttp_stats[cell].entries/2)/ttp_stats[cell].entries);
    }
    cell++;
    if (ttp_stats[cell].entries)
    {
        ttp_stats_print_cell(TTP_STATS_MAX_VAL+1,1000,ttp_stats[cell].entries,(ttp_stats[cell].sum + ttp_stats[cell].entries/2)/ttp_stats[cell].entries);
    }
}

#ifndef INCLUDE_SCOFWD_TEST_MODE
#define appScoFwdDropPacketForTesting() FALSE
#else
static bool appScoFwdDropPacketForTesting(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    static unsigned consecutive_packets_dropped = 0;

    if (theScoFwd->percentage_to_drop)
    {
        unsigned random_percent = UtilRandom() % 100;

        /* First decide about any packet repeat.
           That way, if we are not repeating on purpose - we then
           check against the normal drop percentage.
           Without this the actual %age dropped is noticably off. */

        if (consecutive_packets_dropped)
        {
            if (theScoFwd->drop_multiple_packets < 0)
            {
                if (consecutive_packets_dropped < -theScoFwd->drop_multiple_packets)
                {
                    consecutive_packets_dropped++;
                    if (consecutive_packets_dropped == -theScoFwd->drop_multiple_packets)
                    {
                        consecutive_packets_dropped = 0;
                    }
                    return TRUE;
                }
            }
            else
            {
                if (random_percent < theScoFwd->drop_multiple_packets)
                {
                    return TRUE;
                }
            }
            consecutive_packets_dropped = 0;
        }

        if (random_percent  < theScoFwd->percentage_to_drop)
        {
            consecutive_packets_dropped = 1;
            return TRUE;
        }
    }

    consecutive_packets_dropped = 0;
    return FALSE;
}
#endif /* INCLUDE_SCOFWD_TEST_MODE */

/* Track good/bad packets */
static void updatePacketStats(bool good_packet)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    theScoFwd->lost_packets -= ((theScoFwd->packet_history & 0x80000000u) == 0x80000000u);
    theScoFwd->lost_packets += (good_packet == FALSE);
    theScoFwd->packet_history |= (good_packet == FALSE);
}

static uint8 *sfwd_tx_help_write_ttp(uint8* buffer,rtime_t ttp)
{
    *buffer++ = (ttp >> 16) & 0xff;
    *buffer++ = (ttp >> 8) & 0xff;
    *buffer++ = ttp & 0xff;
    return buffer;
}


static const uint8 *sfwd_rx_help_read_ttp(const uint8* buffer,rtime_t *ttp)
{
    rtime_t result;
    result = *buffer++ & 0xFF;
    result = (result << 8) + (*buffer++ & 0xFF);
    result = (result << 8) + (*buffer++ & 0xFF);
    *ttp = result;
    return buffer;
}


/* Convert a microsend time to a perfectly aligned BT clock */
static uint32 rtime_to_btclock(rtime_t time_us)
{
    uint32 clock = (time_us / US_PER_SLOT) * 2;
    uint16 spare_us = time_us % US_PER_SLOT;
    clock += (spare_us + HALF_SLOT_US/2)/HALF_SLOT_US;
    return clock & ((1ul << 28)-1);   /* Clocks are 28 bits */
}


static uint8* uint16Write(uint8 *dest, uint16 val)
{
    dest[0] = val & 0xff;
    dest[1] = (val >> 8) & 0xff;
    return dest + 2;
}


/*! The Async WBS Decoder expects to have time information about the
    packet, which is information normally supplied by the SCO endpoint.
    Populate this here, converting the supplied time to play (TTP) 
    into the BTCLOCK expected. */
static uint8 *ScoMetadataSet(uint8 *buffer,rtime_t ttp)
{
    buffer = uint16Write(buffer, 0x5c5c);
    buffer = uint16Write(buffer, 5);
    buffer = uint16Write(buffer, SFWD_AUDIO_FRAME_OCTETS);   
    buffer = uint16Write(buffer, 0);    // 0 == OK
    return uint16Write(buffer, (uint16)rtime_to_btclock(ttp));
}


static void start_late_packet_timer(rtime_t ttp)
{
    rtime_t target_callback_time = rtime_sub(ttp,SFWD_LATE_PACKET_OFFSET_TIME_US);
    rtime_t now = SystemClockGetTimerTime();
    int32 ms_delay = US_TO_MS(rtime_sub(target_callback_time, now) + 999);

    cancel_late_packet_timer();

    /* If the delay is out of range... take no action for now.
       When the packetiser is working, we should never receive a packet
       to process that meets that criteria.
     */
    if ((-1 <= ms_delay) && (ms_delay < appConfigScoFwdVoiceTtpMs()))
    {
        MessageSendLater(appGetScoFwdTask(), SFWD_TIMER_LATE_PACKET, NULL, ms_delay);
    }
}

static void cancel_late_packet_timer(void)
{
    uint16 cancel_count = MessageCancelAll(appGetScoFwdTask(), SFWD_TIMER_LATE_PACKET);

    if (cancel_count  > 1)
    {
        DEBUG_LOGF("cancel_late_packet_timer. More than one time cancelled - %d ???",cancel_count);
    }
}


static void handle_late_packet_timer(void)
{
    rtime_t next_ttp;

    if (get_next_expected_ttp(&next_ttp))
    {
        insert_fake_packet_at(next_ttp,0);
    }

    // The misconceived(?) plan was to insert SCO_METADATA into the buffer saying 
    // NOTHING_RECEIVED and get something to happen.
    //
    // it very much looks like the code doesn't actually deal with this though...
    // and as it happens, adding a few words of 0000 to the buffer causes magic to occur.
    // MAGIC:  The normal WBS, proper SCO, works on a tESCO tick (7.5ms) generated
    //         elsewhere in the system. The asynchronous WBS is only activated
    //         by data - so we can kick the WBS by adding data to the buffer 
}

void insert_fake_packet_at(rtime_t new_ttp,rtime_t debug_ttp)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    uint8 hdr[AUDIO_FRAME_METADATA_LENGTH];
    uint16 offset;

    start_late_packet_timer(new_ttp);

    if (appScoFwdTTPIsExpected(new_ttp))
    {
        updatePacketStats(FALSE);
        if (debug_ttp)
        {
            DEBUG_LOGF("FAKE-CATCHUP @ %d 0x%06x",SHORT_TTP(new_ttp),debug_ttp);
        }
        else
        {
            DEBUG_LOGF("FAKE-TIMEOUT @ %d",SHORT_TTP(new_ttp));
        }
        if ((offset = SinkClaim(theScoFwd->sink,SFWD_WBS_DEC_KICK_SIZE)) != 0xFFFF)
        {
            uint8* snk = SinkMap(theScoFwd->sink) + offset;
            audio_frame_metadata_t md = {0,0,0};
            memset(snk,0,SFWD_WBS_DEC_KICK_SIZE);
            md.ttp = new_ttp;

            PacketiserHelperAudioFrameMetadataSet(&md, hdr);

            SinkFlushHeader(theScoFwd->sink,
                    SFWD_WBS_DEC_KICK_SIZE,hdr,AUDIO_FRAME_METADATA_LENGTH);

            set_last_received_ttp(new_ttp);
        }
    }
    else
    {
        if (debug_ttp)
        {
            DEBUG_LOGF("NO-CATCHUP @ %d %d",SHORT_TTP(new_ttp),debug_ttp);
        }
        else
        {
            DEBUG_LOGF("NO-TIMEOUT @ %d",SHORT_TTP(new_ttp));
        }
    }
}


static bool appScoFwdTTPIsExpected(rtime_t new_ttp)
{
    rtime_t expected_ttp;

    if (!get_next_expected_ttp(&expected_ttp))
    {
        return TRUE;
    }
    if (rtime_lt(new_ttp,rtime_sub(expected_ttp,SFWD_PACKET_INTERVAL_MARGIN_US)))
    {
        return FALSE;
    }
    return TRUE;
}

static void set_last_received_ttp(rtime_t ttp_passed_down)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    /* Make sure the low bit of TTP is set so it's distinct from 0 */
    theScoFwd->ttp_of_last_received_packet = ttp_passed_down | 1; 
}

static void clear_last_received_ttp(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    theScoFwd->ttp_of_last_received_packet = 0;
}

static bool have_no_received_ttp(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    return (theScoFwd->ttp_of_last_received_packet == 0);
}

static bool get_next_expected_ttp(rtime_t *next_ttp)
{
    if (next_ttp)
    {
        *next_ttp = 0;

        if (!have_no_received_ttp())
        {
            scoFwdTaskData *theScoFwd = appGetScoFwd();

            *next_ttp = rtime_add(theScoFwd->ttp_of_last_received_packet, SFWD_PACKET_INTERVAL_US);
            return TRUE;
        }
    }
    return FALSE;
}


static bool get_next_ttp_before(rtime_t received_ttp,rtime_t *target_time)
{
    rtime_t next_expected_ttp;

    if (target_time)
    {
        *target_time = 0;

        if (get_next_expected_ttp(&next_expected_ttp))
        {
            if (rtime_gt(received_ttp,rtime_add(next_expected_ttp,SFWD_PACKET_INTERVAL_MARGIN_US)))
            {
                *target_time = next_expected_ttp;
                return TRUE;
            }
        }
    }

    return FALSE;
}

/** Insert any fake packets needed, on the assumption that the supplied
    TTP is the time for the packet we have just received.
 */
void insert_fake_packets_before(rtime_t next_received_ttp,rtime_t debug_ttp)
{
    rtime_t target_time;

    while (get_next_ttp_before(next_received_ttp,&target_time))
    {
        insert_fake_packet_at(target_time,debug_ttp);
    }
}

/*! Check the contents of a WBS frame about to be sent to the air.
    We substitute a similar header on the receiving side, so this
    acts as a sanity check for unexpected behaviour 

    \todo Consider removing panic before full release
    */
static void check_valid_WBS_frame_header(const uint8 *pSource)
{
    if (    pSource[0] != 0x01
         || (   pSource[1] != 0x08
             && pSource[1] != 0x38
             && pSource[1] != 0xC8
             && pSource[1] != 0xF8)
         || pSource[2] != 0xAD
         || pSource[3] != 0x00
         || pSource[4] != 0x00)
    {
         DEBUG_LOGF("Unexpected WBS frame to air. Header %02X %02X %02X %02X",pSource[0],pSource[1],pSource[2],pSource[3]);
         Panic();
    }
}


static void sfwd_tx_queue_next_packet(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    audio_frame_metadata_t md;
    Source audio_source = theScoFwd->source;
    Sink air_sink = theScoFwd->link_sink;
    bool frame_sent = FALSE;
    uint16 packet_size = 0;
    int32 future_ms=0;
    uint16 total_available_data = SourceSize(audio_source);

    if (total_available_data  < SFWD_AUDIO_FRAME_OCTETS)
    {
        return;
    }

    uint16 estimated_frames = total_available_data / SFWD_AUDIO_FRAME_OCTETS;
    if (estimated_frames > SFWD_TX_PACKETISER_MAX_FRAMES_BEHIND)
    {
        uint16  boundary;
        DEBUG_LOGF("TOO MANY FRAMES. %d",estimated_frames);

        while (   estimated_frames > SFWD_TX_PACKETISER_MAX_FRAMES_BEHIND
               && 0 != (boundary = SourceBoundary(audio_source)))
        {
            estimated_frames--;
            SourceDrop(audio_source,boundary);
        }
    }

    /* We only send one packet here, but may also discard some - so use a loop */
    while (   PacketiserHelperAudioFrameMetadataGetFromSource(audio_source,&md) 
           && (!frame_sent))
    {
        rtime_t ttp_in = md.ttp;
        uint16 avail = SourceBoundary(audio_source);
        int32 diff = rtime_sub(ttp_in,SystemClockGetTimerTime());

        if (diff < SFWD_MIN_TRANSIT_TIME_US)
        {
            DEBUG_LOGF("DISCARD (%dms). Now %8d TTP %8d",US_TO_MS(diff),
                                    SystemClockGetTimerTime(),ttp_in);
            SourceDrop(audio_source,avail);
            continue;
        }

        if (appScoFwdDropPacketForTesting())
        {
            SourceDrop(audio_source,avail);
            continue;
        }

        packet_size =   avail
                      + SFWD_TX_PACKETISER_FRAME_HDR_SIZE
                      - SFWD_STRIPPED_HEADER_SIZE;

        uint16 offset = SinkClaim(air_sink, packet_size);
        if (offset == 0xFFFF)
        {
            /* No space for this packet, so exit loop as 
               wont be any space for more packets */
            break;
        }
        uint8 *base = SinkMap(air_sink);
        
        rtime_t ttp_out;
        RtimeLocalToWallClock(&theScoFwd->wallclock,ttp_in,&ttp_out);

        future_ms = US_TO_MS(diff);
        ttp_stats_add(future_ms);

        uint8 *framebase = base + offset;
        uint8 *writeptr = framebase;

        writeptr = sfwd_tx_help_write_ttp(writeptr,ttp_out);
        const uint8 *pSource = SourceMap(audio_source);

        /* Copy audio data into buffer to the air, removing the header */
        memcpy(writeptr,&pSource[SFWD_STRIPPED_HEADER_SIZE],avail - SFWD_STRIPPED_HEADER_SIZE);

        check_valid_WBS_frame_header(pSource);

        SourceDrop(audio_source,avail);
        SinkFlush(air_sink, packet_size);
        frame_sent = TRUE;

        DEBUG_LOGF("TX 1 frame [%3d octets]. TTP in future by %dms",packet_size,future_ms);
    }

}

static void SendOTAControlMessage(uint8 ota_msg_id)
{
    bdaddr peer;
    if (appDeviceGetPeerBdAddr(&peer))
    {
        DEBUG_LOGF("SendOTAControlMessage. OTA CMD 0x%02x requested",ota_msg_id);
        appPeerSigMsgChannelTxRequest(appGetScoFwdTask(),
                                      &peer,
                                      PEER_SIG_MSG_CHANNEL_SCOFWD,
                                      &ota_msg_id,sizeof(ota_msg_id));
    }
    else
    {
        DEBUG_LOGF("SendOTAControlMessage. OTA CMD 0x%02x discarded. NO PEER?",ota_msg_id);
    }
}


static void appScoFwdProcessForwardedSco(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    uint16 avail;

    while ((avail = SourceBoundary(theScoFwd->link_source)) != 0)
    {
        appScoFwdProcessReceivedAirPacket(avail);
    }
}

static void ProcessOTAControlMessage(uint8 ota_msg_id)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    DEBUG_LOGF("ProcessOTAControlMessage. OTA message ID 0x%02X",ota_msg_id);

    switch (ota_msg_id)
    {
        case SFWD_OTA_MSG_SETUP:
            MessageSend(appGetScoFwdTask(), SFWD_INTERNAL_START_RX_CHAIN, NULL);
            break;

        case SFWD_OTA_MSG_TEARDOWN:
            MessageSend(appGetScoFwdTask(), SFWD_INTERNAL_STOP_RX_CHAIN, NULL);
            break;

        case SFWD_OTA_MSG_INCOMING_CALL:
            DEBUG_LOG("SCO Forwarding notified of incoming call");
            theScoFwd->peer_incoming_call = TRUE;
            break;

        case SFWD_OTA_MSG_INCOMING_ENDED:
            DEBUG_LOG("SCO Forwarding notified of incoming call END");
            theScoFwd->peer_incoming_call = FALSE;
            break;

        case SFWD_OTA_MSG_CALL_ANSWER:
            DEBUG_LOG("SCO Forwarding PEER ANSWERING call");
            appHfpCallAccept();
            break;

        case SFWD_OTA_MSG_CALL_REJECT:
            DEBUG_LOG("SCO Forwarding PEER REJECTING call");
            appHfpCallReject();
            break;

        case SFWD_OTA_MSG_CALL_HANGUP:
            DEBUG_LOG("SCO Forwarding PEER ending call");
            appHfpCallHangup();
            break;

        case SFWD_OTA_MSG_VOLUME_UP:
            DEBUG_LOG("SCO Forwarding PEER sent volume up");
            if (appHfpIsScoActive() || appHfpIsConnected())
            {
                appHfpVolumeStart(appConfigGetHfpVolumeStep());
                appHfpVolumeStop(appConfigGetHfpVolumeStep());
            }
            break;

        case SFWD_OTA_MSG_VOLUME_DOWN:
            DEBUG_LOG("SCO Forwarding PEER sent volume down");
            if (appHfpIsScoActive() || appHfpIsConnected())
            {
                appHfpVolumeStart(-appConfigGetHfpVolumeStep());
                appHfpVolumeStop(-appConfigGetHfpVolumeStep());
            }
            break;

        default:
            DEBUG_LOG("Unhandled OTA");
            Panic();
            break;
    }
}


/* This function does basic analysis on a packet received over the
   air. This can be a command, or include multiple SCO frames 

   Guaranteed to consume 'avail' octets from the source.
   */
static void appScoFwdProcessReceivedAirPacket(uint16 avail)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    Source air_source = theScoFwd->link_source;
    const uint8 *pSource = SourceMap(air_source);

    if (!theScoFwd->sink)
    {
        DEBUG_LOG("No sink at present");
    }
    else if (avail < SFWD_STRIPPED_AUDIO_FRAME_OCTETS)
    {
        DEBUG_LOGF("Too little data for a packet %d < %d",avail,SFWD_STRIPPED_AUDIO_FRAME_OCTETS);
        Panic();
    }
    else
    {
        appScoFwdProcessReceivedAirFrame(&pSource, SFWD_STRIPPED_AUDIO_FRAME_OCTETS);
    }

    SourceDrop(air_source,avail);
}

/* This function processes a single SCO frame received over the air */
static void appScoFwdProcessReceivedAirFrame(const uint8 **ppSource,uint8 frame_length)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    bool    setup_late_packet_timer = FALSE;
    rtime_t frame_ttp = 0;
    rtime_t ttp_ota;
    uint8 hdr[AUDIO_FRAME_METADATA_LENGTH];

    *ppSource = sfwd_rx_help_read_ttp(*ppSource,&ttp_ota);
    RtimeWallClock24ToLocal(&theScoFwd->wallclock, ttp_ota, &frame_ttp);

    /* The WBS encoder should subtract a magic number from the TTP to deal with 
       the way encode/decode works. i.e. The first sample fed in comes *out* 
       after some analysis magic, so the first sample out is actually earlier. 
     */
    frame_ttp = rtime_sub(frame_ttp,SFWD_WBS_UNCOMPENSATED_OFFSET_US);
    int32 diff = rtime_sub(frame_ttp,SystemClockGetTimerTime());

    /* Only bother to process the frame if we think we can get to it
        in time. */
    if (diff >= SFWD_RX_PROCESSING_TIME_NORMAL_US)
    {
        DEBUG_LOGF("[%d] fut  %dms",frame_length,US_TO_MS(diff));
        ttp_stats_add(US_TO_MS(diff));

        setup_late_packet_timer = TRUE;

        insert_fake_packets_before(frame_ttp,ttp_ota);

        /* There is a chance that we have already processed this TTP
          or it is sufficiently far out we're just a bit confused.
          Check this and just process good packets. */
        if (appScoFwdTTPIsExpected(frame_ttp))
        {
            uint16 offset;
            uint16 audio_bfr_len = frame_length + SFWD_STRIPPED_HEADER_SIZE + SFWD_SCO_METADATA_SIZE;

            updatePacketStats(TRUE);

            DEBUG_LOGF("REAL @ %d 0x%06x",SHORT_TTP(frame_ttp),ttp_ota);

            if ((offset = SinkClaim(theScoFwd->sink,audio_bfr_len)) != 0xFFFF)
            {
                uint8* snk = SinkMap(theScoFwd->sink) + offset;
                audio_frame_metadata_t md = {0,0,0};

                /* We only set this if we can push the data. Fake audio is smaller
                 * so may be able to enter the buffer */
                set_last_received_ttp(frame_ttp);

                snk = ScoMetadataSet(snk,frame_ttp);    /* Set the TTP into the SCO metadata */
                md.ttp = frame_ttp;

                /* SCO frames start with some metadata that is
                   fixed / not used. We remove this when forwarding SCO
                   so reinsert values here */
                *snk++ = 0x1;
                *snk++ = 0x18;  /* 18 is not a typical value for this field, 
                                   but works and chosen in preference to 0,
                                   which doesn't */
                *snk++ = 0xAD;  /* msbc Syncword */
                *snk++ = 0;
                *snk++ = 0;

                memcpy(snk,*ppSource,frame_length);

                PacketiserHelperAudioFrameMetadataSet(&md, hdr);

                SinkFlushHeader(theScoFwd->sink,audio_bfr_len,hdr,AUDIO_FRAME_METADATA_LENGTH);
            }
            else
            {
                DEBUG_LOGF("Stalled - no space for %d ?",frame_length + SFWD_SCO_METADATA_SIZE);
            }
        }
        else
        {
            DEBUG_LOGF("NOREAL @ %d 0x%06x",SHORT_TTP(frame_ttp),ttp_ota);
        }
    }
    else
    {
        DEBUG_LOGF("No way we can process this. Now %d TTP %d",SystemClockGetTimerTime(),frame_ttp);
    }

    if (setup_late_packet_timer)
    {
        start_late_packet_timer(frame_ttp);
    }

    *ppSource += frame_length;
}


static bool appScoFwdStateCanConnect(void)
{
    return appScoFwdGetState() == SFWD_STATE_IDLE;
}

bool appScoFwdIsStreaming(void)
{
    return (   appScoFwdGetState() == SFWD_STATE_CONNECTED_ACTIVE_SEND
            || appScoFwdGetState() == SFWD_STATE_CONNECTED_ACTIVE_RECEIVE);
}

bool appScoFwdIsReceiving(void)
{
    return (appScoFwdGetState() == SFWD_STATE_CONNECTED_ACTIVE_RECEIVE);
}

bool appScoFwdHasConnection(void)
{
    return appScoFwdGetSink() != NULL;
}


static void appScoFwdEnterIdle(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    DEBUG_LOG("appScoFwdEnterIdle");

    theScoFwd->link_sink = (Sink)NULL;
    theScoFwd->link_source = (Source)NULL;

    theScoFwd->link_retries = 0;
}

static void appScoFwdEnterSdpSearch(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    bdaddr peer_bd_addr;

    DEBUG_LOG("appScoFwdEnterSdpSearch");
    PanicFalse(appDeviceGetPeerBdAddr(&peer_bd_addr));

    /* Perform SDP search */
    ConnectionSdpServiceSearchAttributeRequest(&theScoFwd->task, &peer_bd_addr, 0x32,
                                               appSdpGetScoFwdServiceSearchRequestSize(), appSdpGetScoFwdServiceSearchRequest(),
                                               appSdpGetScoFwdAttributeSearchRequestSize(), appSdpGetScoFwdAttributeSearchRequest());
}

static void appScoFwdEnterConnectingMaster(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    bdaddr peer_bd_addr;

    static const uint16 l2cap_conftab[] =
    {
        /* Configuration Table must start with a separator. */
            L2CAP_AUTOPT_SEPARATOR,
        /* Flow & Error Control Mode. */
            L2CAP_AUTOPT_FLOW_MODE,
        /* Set to Basic mode with no fallback mode */
                BKV_16_FLOW_MODE( FLOW_MODE_BASIC, 0 ),
        /* Local MTU exact value (incoming). */
            L2CAP_AUTOPT_MTU_IN,
        /*  Exact MTU for this L2CAP connection - 672. */
                672,
        /* Remote MTU Minumum value (outgoing). */
            L2CAP_AUTOPT_MTU_OUT,
        /*  Minimum MTU accepted from the Remote device. */
                48,
         /* Local Flush Timeout  - Accept Non-default Timeout*/
            L2CAP_AUTOPT_FLUSH_OUT,
                BKV_UINT32R(SFWD_FLUSH_MIN_US,SFWD_FLUSH_MAX_US),
            L2CAP_AUTOPT_FLUSH_IN,
                BKV_UINT32R(SFWD_FLUSH_MIN_US,SFWD_FLUSH_MAX_US),

        /* Configuration Table must end with a terminator. */
            L2CAP_AUTOPT_TERMINATOR
    };

    DEBUG_LOG("appScoFwdEnterConnectingMaster");
    PanicFalse(appDeviceGetPeerBdAddr(&peer_bd_addr));

    ConnectionL2capConnectRequest(appGetScoFwdTask(),
                                  &peer_bd_addr,
                                  theScoFwd->local_psm, theScoFwd->remote_psm,
                                  CONFTAB_LEN(l2cap_conftab),
                                  l2cap_conftab);
}

static void appScoFwdEnterConnected(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    DEBUG_LOG("appScoFwdEnterConnected");

    appLinkPolicyUpdateRoleFromSink(theScoFwd->link_sink);
}

static void appScoFwdExitConnected(void)
{
    DEBUG_LOG("appScoFwdExitConnected");
}


static void appScoFwdEnterConnectedActiveSend(void)
{
    DEBUG_LOG("appScoFwdEnterActiveSend");

    /* Set flush timeout on ACL as a workaround until B-265037 is fixed */
    ConnectionWriteFlushTimeout(appScoFwdGetSink(), SFWD_FLUSH_TARGET_SLOTS);
}


static void appScoFwdExitConnectedActiveSend(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    DEBUG_LOG("appScoFwdExitActiveSend");

    /* Reset flush timeout on ACL as a workaround until B-265037 is fixed */
    ConnectionWriteFlushTimeout(appScoFwdGetSink(), HCI_MAX_FLUSH_TIMEOUT);

    appKymeraScoStopForwarding();

    PanicFalse(SourceUnmap(theScoFwd->source));
    theScoFwd->source = NULL;
}

static void appScoFwdEnterConnectedActiveReceive(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    DEBUG_LOG("appScoFwdEnterConnectedActiveReceive");

    appAvStreamingSuspend(AV_SUSPEND_REASON_SCOFWD);

    /* Start Kymera receive chain */
    appKymeraScoFwdStartReceive(theScoFwd->link_source, appGetHfp()->volume);

    clear_last_received_ttp();
}

static void appScoFwdExitConnectedActiveReceive(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    DEBUG_LOG("appScoFwdEnterConnectedActiveReceive");

    appKymeraScoFwdStopReceive();

    /* Unmap the timestamped endpoint, as the audio/apps0 won't */
    PanicFalse(SinkUnmap(theScoFwd->sink));
    theScoFwd->sink = NULL;

    appAvStreamingResume(AV_SUSPEND_REASON_SCOFWD);
}

static void appScoFwdEnterDisconnecting(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    DEBUG_LOG("appScoFwdEnterDisconnecting");

    ConnectionL2capDisconnectRequest(appGetScoFwdTask(),
                                     theScoFwd->link_sink);
}


static void appScoFwdExitInitialising(void)
{
    MessageSend(appGetAppTask(), SFWD_INIT_CFM, NULL);
}


/*! \brief Set the SCO forwarding FSM state

    Called to change state.  Handles calling the state entry and exit
    functions for the new and old states.
*/
static void appScoFwdSetState(scoFwdState new_state)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    bdaddr peer;
    scoFwdState old_state = theScoFwd->state;

    DEBUG_LOGF("appScoFwdSetState(%d) from %d", new_state, old_state);

    switch (old_state)
    {
        case SFWD_STATE_NULL:
        case SFWD_STATE_IDLE:
        case SFWD_STATE_SDP_SEARCH:
        case SFWD_STATE_CONNECTING_MASTER:
        case SFWD_STATE_CONNECTING_SLAVE:
        case SFWD_STATE_DISCONNECTING:
            break;

        case SFWD_STATE_CONNECTED_ACTIVE_RECEIVE:
            appScoFwdExitConnectedActiveReceive();
            break;

        case SFWD_STATE_CONNECTED_ACTIVE_SEND:
            appScoFwdExitConnectedActiveSend();
            break;

        case SFWD_STATE_INITIALISING:
            appScoFwdExitInitialising();
            break;

        default:
            break;
    }


    if ((old_state>= SFWD_STATE_CONNECTED) &&
        (new_state < SFWD_STATE_CONNECTED))
        appScoFwdExitConnected();

    /* Set new state */
    theScoFwd->state = new_state;
    theScoFwd->lock = new_state & SFWD_STATE_LOCK_MASK;

    if ((new_state>= SFWD_STATE_CONNECTED) &&
        (old_state < SFWD_STATE_CONNECTED))
        appScoFwdEnterConnected();

    switch (new_state)
    {
        case SFWD_STATE_IDLE:
            appScoFwdEnterIdle();
            break;

        case SFWD_STATE_NULL:
            DEBUG_LOG("appScoFwdSetState, null");
            break;

        case SFWD_STATE_INITIALISING:
            DEBUG_LOG("appScoFwdSetState, initialising");
            break;

        case SFWD_STATE_SDP_SEARCH:
            appScoFwdEnterSdpSearch();
            break;

        case SFWD_STATE_CONNECTING_MASTER:
            appScoFwdEnterConnectingMaster();
            break;

        case SFWD_STATE_CONNECTING_SLAVE:
            DEBUG_LOG("appScoFwdSetState, connecting slave");
            break;

        case SFWD_STATE_CONNECTED_ACTIVE_SEND:
            appScoFwdEnterConnectedActiveSend();
            break;

        case SFWD_STATE_CONNECTED_ACTIVE_RECEIVE:
            appScoFwdEnterConnectedActiveReceive();
            break;

        case SFWD_STATE_DISCONNECTING:
            appScoFwdEnterDisconnecting();
            break;

        default:
            break;
    }

    if (appDeviceGetPeerBdAddr(&peer))
    {
        appLinkPolicyUpdatePowerTable(&peer);
    }
}

/*! \brief Get the SCO forwarding FSM state

    Returns current state of the SCO forwarding FSM.
*/
scoFwdState appScoFwdGetState(void)
{
    return appGetScoFwd()->state;
}



void appScoFwdInitPacketising(Source audio_source)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    theScoFwd->source = audio_source;

    PanicNull(theScoFwd->link_sink);
    PanicNull(theScoFwd->source);

    MessageStreamTaskFromSink(theScoFwd->link_sink, appGetScoFwdTask());
    MessageStreamTaskFromSource(theScoFwd->source, appGetScoFwdTask());

    RtimeWallClockEnable(&theScoFwd->wallclock, theScoFwd->link_sink);

    PanicFalse(SourceMapInit(theScoFwd->source, STREAM_TIMESTAMPED, AUDIO_FRAME_METADATA_LENGTH));
    PanicFalse(SourceConfigure(theScoFwd->source, VM_SOURCE_MESSAGES, VM_MESSAGES_ALL));
    PanicFalse(SinkConfigure(theScoFwd->link_sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL));

    appScoFwdSetState(SFWD_STATE_CONNECTED_ACTIVE_SEND);

    appScoFwdKickProcessing();
}


void appScoFwdNotifyIncomingSink(Sink sco_sink)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    theScoFwd->sink = sco_sink;

    PanicFalse(SinkConfigure(theScoFwd->sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL));
    PanicFalse(SourceConfigure(theScoFwd->link_source, VM_SOURCE_MESSAGES, VM_MESSAGES_ALL));

    appScoFwdKickProcessing();
}

/*! When a link and chain are initialised there are occasions where
    the buffer messages (MESSAGE_MORE_DATA/MESSAGE_MORE_SPACE) can be
    lost or mishandled, leaving a situation where the buffers have data
    and space, but we will never process it.

    This function can be used to kick the processing when the chains
    are definitely ready for use.
 */
static void appScoFwdKickProcessing(void)
{
    MessageSend(appGetScoFwdTask(),SFWD_INTERNAL_KICK_PROCESSING,NULL);
}


/* We have a request to make sure that the L2CAP link to our peer
   is established.

   If the link is *not* already up, take steps to connect it */
static void appScoFwdHandleLinkConnectReq(void)
{
    if (appScoFwdStateCanConnect())
        appScoFwdSetState(SFWD_STATE_SDP_SEARCH);
}

static void appScoFwdHandleLinkDisconnectReq(void)
{
    switch (appScoFwdGetState())
    {
        case SFWD_STATE_CONNECTED_ACTIVE_SEND:
        case SFWD_STATE_CONNECTED_ACTIVE_RECEIVE:
        case SFWD_STATE_CONNECTED:
            appScoFwdSetState(SFWD_STATE_DISCONNECTING);
            break;

        default:
            break;
    }
}

static void appScoFwdRetryConnect(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    /* Only retry if we're in the SDP search or connecting master state */
    switch (appScoFwdGetState())
    {
        case SFWD_STATE_SDP_SEARCH:
        case SFWD_STATE_CONNECTING_MASTER:
        {
            theScoFwd->link_retries += 1;
            DEBUG_LOGF("appScoFwdRetryConnect, retry %u", theScoFwd->link_retries);
            if (theScoFwd->link_retries < SFWD_L2CAP_MAX_ATTEMPTS)
            {
                /* Re-enter current state, this will kick off another connection attempt */
                appScoFwdSetState(appScoFwdGetState());
            }
            else
            {
                /* Retry limit hit, go back to idle */
                appScoFwdSetState(SFWD_STATE_IDLE);
            }
        }
        break;

        default:
        {
            /* Unexpected state */
            Panic();
        }
        break;
    }
}


static void appScoFwdHandleL2capConnectCfm(const CL_L2CAP_CONNECT_CFM_T *cfm)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    DEBUG_LOGF("appScoFwdHandleL2capConnectCfm, status %u", cfm->status);

    switch (appScoFwdGetState())
    {
        case SFWD_STATE_CONNECTING_MASTER:
        case SFWD_STATE_CONNECTING_SLAVE:
        {
            if (l2cap_connect_pending == cfm->status)
            {
                DEBUG_LOG("appScoFwdHandleL2capConnectCfm, connect pending, wait");
            }
            else if (l2cap_connect_success == cfm->status)
            {
                DEBUG_LOGF("appScoFwdHandleL2capConnectCfm, connected, conn ID %u, flush remote %u", cfm->connection_id, cfm->flush_timeout_remote);

                PanicNull(cfm->sink);
                theScoFwd->link_sink = cfm->sink;
                theScoFwd->link_source = StreamSourceFromSink(cfm->sink);

                if (RtimeWallClockEnable(&appGetScoFwd()->wallclock, cfm->sink))
                {
                    DEBUG_LOGF("appScoFwdHandleL2capConnectCfm, wallclock enabled offset %d", appGetScoFwd()->wallclock.offset);
                }
                else
                {
                    DEBUG_LOG("appScoFwdHandleL2capConnectCfm, wallclock not enabled");
                }

                appScoFwdSetState(SFWD_STATE_CONNECTED);

                /* If we first connect with an incoming call active, then we
                   need to inform our peer - otherwise the UI won't work. */
                if (appHfpIsCallIncoming())
                {
                    SendOTAControlMessage(SFWD_OTA_MSG_INCOMING_CALL);
                }
            }
            else
            {
                if (appScoFwdGetState() == SFWD_STATE_CONNECTING_MASTER)
                {
                    DEBUG_LOG("appScoFwdHandleL2capConnectCfm, failed, retrying connection");
                    appScoFwdRetryConnect();
                }
                else
                {
                    DEBUG_LOG("appScoFwdHandleL2capConnectCfm, failed");
                    appScoFwdSetState(SFWD_STATE_IDLE);
                }
            }
        }
        break;

        default:
            Panic();
            break;

    }
}

static void appScoFwdHandleL2capRegisterCfm(const CL_L2CAP_REGISTER_CFM_T *cfm)
{
    DEBUG_LOGF("appScoFwdHandleL2capRegisterCfm, status %u, psm %u", cfm->status, cfm->psm);
    PanicFalse(appScoFwdGetState() == SFWD_STATE_INITIALISING);

    /* We have registered the PSM used for SCO forwarding links with
       connection manager, now need to wait for requests to process 
       an incoming connection or make an outgoing connection. */
    if (success == cfm->status)
    {
        scoFwdTaskData *theScoFwd = appGetScoFwd();

        /* Keep a copy of the registered L2CAP PSM, maybe useful later */
        theScoFwd->local_psm = cfm->psm;

        /* Copy and update SDP record */
        uint8 *record = PanicUnlessMalloc(appSdpGetScoFwdServiceRecordSize());
        memcpy(record, appSdpGetScoFwdServiceRecord(), appSdpGetScoFwdServiceRecordSize());

        /* Write L2CAP PSM into service record */
        appSdpSetScoFwdPsm(record, cfm->psm);

        /* Register service record */
        ConnectionRegisterServiceRecord(appGetScoFwdTask(), appSdpGetScoFwdServiceRecordSize(), record);
    }
    else
    {
        DEBUG_LOG("appScoFwdHandleL2capRegisterCfm, failed to register L2CAP PSM");
        Panic();
    }
}

static void appScoFwdHandleClSdpRegisterCfm(const CL_SDP_REGISTER_CFM_T *cfm)
{
    DEBUG_LOGF("appScoFwdHandleClSdpRegisterCfm, status %d", cfm->status);
    PanicFalse(appScoFwdGetState() == SFWD_STATE_INITIALISING);

    if (cfm->status == sds_status_success)
    {
        /* Move to 'idle' state */
        appScoFwdSetState(SFWD_STATE_IDLE);
    }
    else
        Panic();
}

static bool appScoFwdGetL2capPSM(const uint8 *begin, const uint8 *end, uint16 *psm, uint16 id)
{
    ServiceDataType type;
    Region record, protocols, protocol, value;
    record.begin = begin;
    record.end   = end;

    while (ServiceFindAttribute(&record, id, &type, &protocols))
        if (type == sdtSequence)
            while (ServiceGetValue(&protocols, &type, &protocol))
            if (type == sdtSequence
               && ServiceGetValue(&protocol, &type, &value)
               && type == sdtUUID
               && RegionMatchesUUID32(&value, (uint32)0x0100)
               && ServiceGetValue(&protocol, &type, &value)
               && type == sdtUnsignedInteger)
            {
                *psm = (uint16)RegionReadUnsigned(&value);
                return TRUE;
            }

    return FALSE;
}

static void appScoFwdHandleClSdpServiceSearchAttributeCfm(const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *cfm)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    DEBUG_LOGF("appScoFwdHandleClSdpServiceSearchAttributeCfm, status %d", cfm->status);

    switch (appScoFwdGetState())
    {
        case SFWD_STATE_SDP_SEARCH:
        {
            /* Find the PSM in the returned attributes */
            if (cfm->status == sdp_response_success)
            {
                if (appScoFwdGetL2capPSM(cfm->attributes, cfm->attributes+cfm->size_attributes,
                                         &theScoFwd->remote_psm, saProtocolDescriptorList))
                {
                    DEBUG_LOGF("appHandleClSdpServiceSearchAttributeCfm, peer psm %u", theScoFwd->remote_psm);

                    appScoFwdSetState(SFWD_STATE_CONNECTING_MASTER);
                }
                else
                {
                    /* No PSM found, malformed SDP record on peer? */
                    appScoFwdSetState(SFWD_STATE_IDLE);
                }
            }
            else if (cfm->status == sdp_no_response_data)
            {
                /* Peer Earbud doesn't support SCO forwarding service */
                appScoFwdSetState(SFWD_STATE_IDLE);
            }
            else
            {
                /* SDP seach failed, retry? */
                appScoFwdRetryConnect();
            }
        }
        break;

        default:
        {
            /* Silently ignore, not the end of the world */
        }
        break;
    }
}

static void appScoFwdHandleL2capConnectInd(const CL_L2CAP_CONNECT_IND_T *ind)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    DEBUG_LOGF("appScoFwdHandleL2capConnectInd, psm %u",ind->psm);
    PanicFalse(ind->psm == theScoFwd->local_psm);

    static const uint16 l2cap_conftab[] =
    {
        /* Configuration Table must start with a separator. */
        L2CAP_AUTOPT_SEPARATOR, 
        /* Local Flush Timeout  - Accept Non-default Timeout*/
        L2CAP_AUTOPT_FLUSH_OUT,
            BKV_UINT32R(SFWD_FLUSH_MIN_US,SFWD_FLUSH_MAX_US),
        L2CAP_AUTOPT_FLUSH_IN,
            BKV_UINT32R(SFWD_FLUSH_MIN_US,SFWD_FLUSH_MAX_US),
        L2CAP_AUTOPT_TERMINATOR
    };    

    /* Only accept connection if it's from the peer and we're in the idle state */
    if (appDeviceIsPeer(&ind->bd_addr) && (appScoFwdGetState() == SFWD_STATE_IDLE))
    {
        /* Send a response accepting the connection. */
        ConnectionL2capConnectResponse(appGetScoFwdTask(),     /* The client task. */
                                       TRUE,                   /* Accept the connection. */
                                       ind->psm,               /* The local PSM. */
                                       ind->connection_id,     /* The L2CAP connection ID.*/
                                       ind->identifier,        /* The L2CAP signal identifier. */
                                       CONFTAB_LEN(l2cap_conftab),
                                       l2cap_conftab);          /* The configuration table. */

        appScoFwdSetState(SFWD_STATE_CONNECTING_SLAVE);
    }
    else
    {
        /* Send a response rejecting the connection. */
        ConnectionL2capConnectResponse(appGetScoFwdTask(),     /* The client task. */
                                       FALSE,                  /* Reject the connection. */
                                       ind->psm,               /* The local PSM. */
                                       ind->connection_id,     /* The L2CAP connection ID.*/
                                       ind->identifier,        /* The L2CAP signal identifier. */
                                       CONFTAB_LEN(l2cap_conftab),
                                       l2cap_conftab);          /* The configuration table. */
    }
}

static void appScoFwdHandleL2capDisconnectInd(const CL_L2CAP_DISCONNECT_IND_T *ind)
{
    DEBUG_LOGF("appScoFwdHandleL2capDisconnectInd, status %u", ind->status);

    ConnectionL2capDisconnectResponse(ind->identifier, ind->sink);

    appScoFwdSetState(SFWD_STATE_IDLE);
}


static void appScoFwdHandleL2capDisconnectCfm(const CL_L2CAP_DISCONNECT_CFM_T *cfm)
{
    DEBUG_LOGF("appScoFwdHandleL2capDisconnectCfm, status %u", cfm->status);

    appScoFwdSetState(SFWD_STATE_IDLE);
}


static void appScoFwdHandleMMD(const MessageMoreData *mmd)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    if (mmd->source == theScoFwd->source)
    {
        sfwd_tx_queue_next_packet();
    }
    else if (mmd->source == theScoFwd->link_source)
    {
        appScoFwdProcessForwardedSco();
    }
}

/*! \brief Handle messages about the interface between the outside world
           (L2CAP) and the audio chain.
           
     We treat space in the target buffer in the same way as more data in the
     send buffer. May need to assess if we need this, but neccessary for 
     making sure stall situations recover (if possible).
 */
static void appScoFwdHandleMMS(const MessageMoreSpace *mms)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    if (mms->sink == theScoFwd->link_sink)
    {
        sfwd_tx_queue_next_packet();
    }
    else if (mms->sink == theScoFwd->sink)
    {
        appScoFwdProcessForwardedSco();
    }
}

static void appScoFwdHandleStartReceiveChain(void)
{
    if (!appScoFwdIsStreaming() && appScoFwdHasConnection())
    {
        /* Move to receive state */
        appScoFwdSetState(SFWD_STATE_CONNECTED_ACTIVE_RECEIVE);
    }
    else
    {
        DEBUG_LOG("appScoFwdHandleStartChain Asked to start when already active");
    }
}

static void appScoFwdHandleStopReceiveChain(void)
{
    if (appScoFwdIsStreaming())
    {
        appScoFwdSetState(SFWD_STATE_CONNECTED);
    }
    else
    {
        DEBUG_LOGF("appScoFwdHandleStopChain Asked to stop when not active. State %d",appScoFwdGetState());
    }
}

static void appScoFwdHandleKickProcessing(void)
{
    scoFwdState state = appScoFwdGetState();
    if (SFWD_STATE_CONNECTED_ACTIVE_RECEIVE == state)
    {
        appScoFwdProcessForwardedSco();
    }
    else if (SFWD_STATE_CONNECTED_ACTIVE_SEND == state)
    {
        sfwd_tx_queue_next_packet();
    }
}

static void appScoFwdHandleHfpScoIncomingRingInd(void)
{
    SendOTAControlMessage(SFWD_OTA_MSG_INCOMING_CALL);
}

static void appScoFwdHandleHfpScoIncomingCallEndedInd(void)
{
    SendOTAControlMessage(SFWD_OTA_MSG_INCOMING_ENDED);
}

static void appScoFwdHandlePeerSignallingMessage(const PEER_SIG_MSG_CHANNEL_RX_IND_T *ind)
{
    /* Note that at least initially all signalling messages are just 1 byte */
    DEBUG_LOGF("appScoFwdHandlePeerSignallingMessage. Channel 0x%x, len %d, content %x",ind->channel,ind->msg_size,ind->msg[0]);

    ProcessOTAControlMessage(ind->msg[0]);
}

/* Handle a confirm message.

    This is only used for debug purposes, but may need to deal with any error code
    in future. */
static void appScoFwdHandlePeerSignallingMessageTxConfirm(const PEER_SIG_MSG_CHANNEL_TX_CFM_T *cfm)
{
    peerSigStatus status = cfm->status;

    DEBUG_LOGF("appScoFwdHandlePeerSignallingMessageTxConfirm. Channel 0x%x", cfm->channel);

    if (peerSigStatusSuccess != status)
    {
        DEBUG_LOGF("appScoFwdHandlePeerSignallingMessageTxConfirm reports failure code 0x%x(%d)",status,status);
    }
}

static void appScoFwdHandlePeerSignallingConnectionInd(const PEER_SIG_CONNECTION_IND_T *ind)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    DEBUG_LOGF("appScoFwdHandlePeerSignallingConnectionInd, status %u", ind->status);

    /* Peer signalling has disconnected, therefore we don't know if peer has
     * incoming call or not */
    if (ind->status == peerSigStatusDisconnected)
        theScoFwd->peer_incoming_call = FALSE;
}

/*! \brief Message Handler

    This function is the main message handler for SCO forwarding, every
    message is handled in it's own seperate handler function.  

    The different groups of messages are separated in the switch statement
    by a comment like this ----
*/
static void appScoFwdHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

#ifdef SCOFWD_EXTRA_DEBUG
    if (   id != MESSAGE_MORE_DATA
        && id != MESSAGE_MORE_SPACE)
    {
        DEBUG_LOGF("**** appScoFwdHandleMessage: 0x%x (%d)",id,id);
    }
#endif

    switch (id)
    {
        case PEER_SIG_MSG_CHANNEL_RX_IND:
            appScoFwdHandlePeerSignallingMessage((const PEER_SIG_MSG_CHANNEL_RX_IND_T *)message);
            break;

        case PEER_SIG_MSG_CHANNEL_TX_CFM:
            appScoFwdHandlePeerSignallingMessageTxConfirm((const PEER_SIG_MSG_CHANNEL_TX_CFM_T *)message);
            break;

        case PEER_SIG_CONNECTION_IND:
            appScoFwdHandlePeerSignallingConnectionInd((const PEER_SIG_CONNECTION_IND_T *)message);
            break;

        /*----*/

        case CL_L2CAP_REGISTER_CFM:
            appScoFwdHandleL2capRegisterCfm((const CL_L2CAP_REGISTER_CFM_T *)message);
            break;

        case CL_SDP_REGISTER_CFM:
            appScoFwdHandleClSdpRegisterCfm((const CL_SDP_REGISTER_CFM_T *)message);
            break;

        case CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM:
            appScoFwdHandleClSdpServiceSearchAttributeCfm((const CL_SDP_SERVICE_SEARCH_ATTRIBUTE_CFM_T *)message);
            return;

        case CL_L2CAP_CONNECT_IND:
            appScoFwdHandleL2capConnectInd((const CL_L2CAP_CONNECT_IND_T *)message);
            break;

        case CL_L2CAP_CONNECT_CFM:
            appScoFwdHandleL2capConnectCfm((const CL_L2CAP_CONNECT_CFM_T *)message);
            break;

        case CL_L2CAP_DISCONNECT_IND:
            appScoFwdHandleL2capDisconnectInd((const CL_L2CAP_DISCONNECT_IND_T *)message);
            break;

        case CL_L2CAP_DISCONNECT_CFM:
            appScoFwdHandleL2capDisconnectCfm((const CL_L2CAP_DISCONNECT_CFM_T *)message);
            break;

        /*----*/

        case APP_HFP_SCO_INCOMING_RING_IND:
            appScoFwdHandleHfpScoIncomingRingInd();
            break;

        case APP_HFP_SCO_INCOMING_ENDED_IND:
            appScoFwdHandleHfpScoIncomingCallEndedInd();
            break;

        /*----*/

        case MESSAGE_MORE_DATA:
            appScoFwdHandleMMD((const MessageMoreData*)message);
            break;

        case MESSAGE_MORE_SPACE:
            appScoFwdHandleMMS((const MessageMoreSpace*)message);
            break;

        case MESSAGE_SOURCE_EMPTY:
            break;

        /*----*/

        case SFWD_INTERNAL_LINK_CONNECT_REQ:
            appScoFwdHandleLinkConnectReq();
            break;

        case SFWD_INTERNAL_LINK_DISCONNECT_REQ:
            appScoFwdHandleLinkDisconnectReq();
            break;

        case SFWD_INTERNAL_START_RX_CHAIN:
            appScoFwdHandleStartReceiveChain();
            break;

        case SFWD_INTERNAL_STOP_RX_CHAIN:
            appScoFwdHandleStopReceiveChain();
            break;

        case SFWD_INTERNAL_KICK_PROCESSING:
            appScoFwdHandleKickProcessing();
            break;

        case SFWD_TIMER_LATE_PACKET:
            handle_late_packet_timer();
            break;

        /*----*/

        default:
            DEBUG_LOGF("appScoFwdHandleMessage. UNHANDLED Message id=x%x (%d). State %d", id, id, appScoFwdGetState());
            break;
    }
}

/* Set-up a link between the devices for forwarding SCO audio. */
void appScoFwdConnectPeer(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    MessageSendConditionally(appGetScoFwdTask(), SFWD_INTERNAL_LINK_CONNECT_REQ, NULL, &theScoFwd->lock);
}

/*! \brief Inform the Peer earbud that we're done with HFP */
void appScoFwdDisconnectPeer(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();
    MessageSendConditionally(appGetScoFwdTask(), SFWD_INTERNAL_LINK_DISCONNECT_REQ, NULL, &theScoFwd->lock);
}


bool appScoFwdIsCallIncoming(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    DEBUG_LOGF("appScoFwdIsCallIncoming: Checking for incoming call (%d)",theScoFwd->peer_incoming_call);

    return theScoFwd->peer_incoming_call;
}

void appScoFwdCallAccept(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    SendOTAControlMessage(SFWD_OTA_MSG_CALL_ANSWER);
    theScoFwd->peer_incoming_call = FALSE;
}

void appScoFwdCallReject(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    SendOTAControlMessage(SFWD_OTA_MSG_CALL_REJECT);
    theScoFwd->peer_incoming_call = FALSE;
}

void appScoFwdCallHangup(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    SendOTAControlMessage(SFWD_OTA_MSG_CALL_HANGUP);
    theScoFwd->peer_incoming_call = FALSE;
}

void appScoFwdVolumeUp(int step)
{
    UNUSED(step);

    SendOTAControlMessage(SFWD_OTA_MSG_VOLUME_UP);
}

void appScoFwdVolumeDown(int step)
{
    UNUSED(step);

    SendOTAControlMessage(SFWD_OTA_MSG_VOLUME_DOWN);
}


/*! \brief Initialise SCO Forwarding task

    Called at start up to initialise the SCO forwarding task
*/
void appScoFwdInit(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    /* Set up task handler */
    theScoFwd->task.handler = appScoFwdHandleMessage;

    /* Register a Protocol/Service Multiplexor (PSM) that will be 
       used for this application. The same PSM is used at both
       ends. */
    ConnectionL2capRegisterRequest(appGetScoFwdTask(), L2CA_PSM_INVALID, 0);

    /* Initialise state */
    theScoFwd->state = SFWD_STATE_NULL;
    appScoFwdSetState(SFWD_STATE_INITIALISING);

    /* Want to know about HFP calls */
    appHfpStatusClientRegister(appGetScoFwdTask());

    /* Register a channel for peer signalling */
    appPeerSigMsgChannelTaskRegister(appGetScoFwdTask(), PEER_SIG_MSG_CHANNEL_SCOFWD);

    /* Register for peer signaling notifications */
    appPeerSigClientRegister(appGetScoFwdTask());
}

#endif

/*  This is the number of message sends from scofwd calling
    SendOTAControlMessage(SFWD_OTA_MSG_SETUP);
    to the AVRCP flushing the message to the sink. It comprises:
        SendOTAControlMessage()
            appPeerSigMsgChannelTxRequest() -> PEER_SIG_INTERNAL_MSG_CHANNEL_TX_REQ
        appPeerSigHandleInternalMsgChannelTxRequest()
            appPeerSigVendorPassthroughRequest()
                appAvrcpVendorPassthroughRequest() -> AV_INTERNAL_AVRCP_VENDOR_PASSTHROUGH_REQ
        appAvrcpHandleInternalAvrcpVendorPassthroughRequest()
            AvrcpPassthroughRequest() -> AVRCP_INTERNAL_PASSTHROUGH_REQ
        avrcpHandleInternalPassThroughReq()
            avrcpAvctpSendMessage()
                SinkFlush()
    For SCO forwarding, the Kymera SCO start is delayed by this number of message
    sends to allow the OTA control message to be flushed before incurring the long,
    blocking kymera start calls.
*/
#define SFWD_SCO_START_MSG_DELAY 3

/*! \brief SCO forwarding handling of HFP_AUDIO_CONNECT_CFM.
    \param cfm The confirmation message.
    \note Without ScoFwd, just start kymera SCO.
*/
void appScoFwdHandleHfpAudioConnectConfirmation(const HFP_AUDIO_CONNECT_CFM_T *cfm)
{
    uint8 delay = 0;
#ifdef INCLUDE_SCOFWD
    if (appScoFwdHasConnection())
    {
        tp_bdaddr sink_bdaddr;

        SinkGetBdAddr(cfm->audio_sink, &sink_bdaddr);
        if (!appDeviceIsTwsPlusHandset(&sink_bdaddr.taddr.addr))
        {
            SendOTAControlMessage(SFWD_OTA_MSG_SETUP);
            delay = SFWD_SCO_START_MSG_DELAY - 1;
        }
    }
#endif
    appKymeraScoStart(cfm->audio_sink, cfm->codec, cfm->wesco, appGetHfp()->volume, delay);
    if (delay)
    {
        appKymeraScoStartForwarding(appScoFwdGetSink());
    }
}

/*! \brief SCO forwarding handling of HFP_AUDIO_DISCONNECT_IND.
    \param ind The indication message.
    \note Without ScoFwd, just stop kymera SCO.
*/
void appScoFwdHandleHfpAudioDisconnectIndication(const HFP_AUDIO_DISCONNECT_IND_T *ind)
{
    UNUSED(ind);

#ifdef INCLUDE_SCOFWD

    if (appScoFwdIsStreaming())
    {
        /* Send message to stop chain on peer */
        SendOTAControlMessage(SFWD_OTA_MSG_TEARDOWN);

        /* Move to connected state */
        appScoFwdSetState(SFWD_STATE_CONNECTED);
    }
#endif

    appKymeraScoStop();
}

