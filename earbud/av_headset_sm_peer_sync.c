/*!
\copyright  Copyright (c) 2005 - 2018 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       av_headset_sm_peer_sync.c
\brief      Application state machine peer earbud synchronisation.
*/

#include "av_headset.h"
#include "av_headset_log.h"
#include "av_headset_sm_private.h"
#include "av_headset_sm_peer_sync.h"
#include "av_headset_device.h"
#include "av_headset_peer_signalling.h"
#include "av_headset_battery.h"
#include "av_headset_conn_rules.h"

#include <bdaddr.h>

/* Uncomment to enable generation of connected/disconnected events
 * per profile for peer-handset link */
//#define PEER_SYNC_PROFILE_EVENTS

#define PEER_SYNC_STATE_SET_SENT(x)         ((x) |= SM_PEER_SYNC_SENT)
#define PEER_SYNC_STATE_CLEAR_SENT(x)       ((x) &= ~SM_PEER_SYNC_SENT)
#define PEER_SYNC_STATE_SET_RECEIVED(x)     ((x) |= SM_PEER_SYNC_RECEIVED)
#define PEER_SYNC_STATE_CLEAR_RECEIVED(x)   ((x) &= ~SM_PEER_SYNC_RECEIVED)
#define PEER_SYNC_SEQNUM_INCR(x)            (x = ((x+1) % 0xFF))

static void appSmUpdateA2dpConnected(bool connected)
{
    smTaskData *sm = appGetSm();

    /* first profile connected or last profile disconnected then inform rules
     * that peer handset has connected/disconnected */
    if ((sm->peer_a2dp_connected != connected) &&
        (!sm->peer_hfp_connected && !sm->peer_avrcp_connected))
    {
        DEBUG_LOGF("appSmUpdateA2dpConnected Prev %u New %u", sm->peer_a2dp_connected, connected);
        appConnRulesSetEvent(appGetSmTask(),
                             connected ? RULE_EVENT_PEER_HANDSET_CONNECTED
                                       : RULE_EVENT_PEER_HANDSET_DISCONNECTED);
    }
#ifdef PEER_SYNC_PROFILE_EVENTS
    /* generate per profile connected/disconnected events for peer-handset link */
    if (!sm->peer_a2dp_connected && connected)
    {
        appConnRulesSetEvent(appGetSmTask(), RULE_EVENT_PEER_A2DP_CONNECTED);
    }
    else if (sm->peer_a2dp_connected && !connected)
    {
        appConnRulesSetEvent(appGetSmTask(), RULE_EVENT_PEER_A2DP_DISCONNECTED);
    }
#endif
    
    /* update the state with latest from peer */
    sm->peer_a2dp_connected = connected;
}

static void appSmUpdateAvrcpConnected(bool connected)
{
    smTaskData *sm = appGetSm();

    /* first profile connected or last profile disconnected then inform rules
     * that peer handset has connected/disconnected */
    if ((sm->peer_avrcp_connected != connected) &&
        (!sm->peer_hfp_connected && !sm->peer_a2dp_connected))
    {
        DEBUG_LOGF("appSmUpdateAvrcpConnected Prev %u New %u", sm->peer_avrcp_connected, connected);
        appConnRulesSetEvent(appGetSmTask(),
                             connected ? RULE_EVENT_PEER_HANDSET_CONNECTED
                                       : RULE_EVENT_PEER_HANDSET_DISCONNECTED);
    }
#ifdef PEER_SYNC_PROFILE_EVENTS
    /* generate per profile connected/disconnected events for peer-handset link */
    if (!sm->peer_avrcp_connected && connected)
    {
        appConnRulesSetEvent(appGetSmTask(), RULE_EVENT_PEER_AVRCP_CONNECTED);
    }
    else if (sm->peer_avrcp_connected && !connected)
    {
        appConnRulesSetEvent(appGetSmTask(), RULE_EVENT_PEER_AVRCP_DISCONNECTED);
    }
#endif
    sm->peer_avrcp_connected = connected;
}

static void appSmUpdateHfpConnected(bool connected)
{
    smTaskData *sm = appGetSm();

    /* first profile connected or last profile disconnected then inform rules
     * that peer handset has connected/disconnected */
    if ((sm->peer_hfp_connected != connected) &&
        (!sm->peer_a2dp_connected && !sm->peer_avrcp_connected))
    {
        DEBUG_LOGF("appSmUpdateHfpConnected Prev %u New %u", sm->peer_hfp_connected, connected);
        appConnRulesSetEvent(appGetSmTask(),
                             connected ? RULE_EVENT_PEER_HANDSET_CONNECTED
                                       : RULE_EVENT_PEER_HANDSET_DISCONNECTED);
    }
#ifdef PEER_SYNC_PROFILE_EVENTS
    /* generate per profile connected/disconnected events for peer-handset link */
    if (!sm->peer_hfp_connected && connected)
    {
        appConnRulesSetEvent(appGetSmTask(), RULE_EVENT_PEER_HFP_CONNECTED);
    }
    else if (sm->peer_hfp_connected && !connected)
    {
        appConnRulesSetEvent(appGetSmTask(), RULE_EVENT_PEER_HFP_DISCONNECTED);
    }
#endif
    sm->peer_hfp_connected = connected;
}

static void appSmUpdatePeerInCase(bool peer_in_case)
{
    smTaskData *sm = appGetSm();

    if (!sm->peer_in_case && peer_in_case)
    {
        DEBUG_LOGF("appSmUpdatePeerInCase Prev %u New %u", sm->peer_in_case, peer_in_case);
        appConnRulesSetEvent(appGetSmTask(), RULE_EVENT_PEER_IN_CASE);
    }
    else if (sm->peer_in_case && !peer_in_case)
    {
        DEBUG_LOGF("appSmUpdatePeerInCase Prev %u New %u", sm->peer_in_case, peer_in_case);
        appConnRulesSetEvent(appGetSmTask(), RULE_EVENT_PEER_OUT_CASE);
    }
    sm->peer_in_case = peer_in_case;
}

static void appSmUpdatePeerInEar(bool peer_in_ear)
{
    smTaskData *sm = appGetSm();

    if (!sm->peer_in_ear && peer_in_ear)
    {
        DEBUG_LOGF("appSmUpdatePeerInEar Prev %u New %u", sm->peer_in_ear, peer_in_ear);
        appConnRulesSetEvent(&sm->task, RULE_EVENT_PEER_IN_EAR);
    }
    else if (sm->peer_in_ear && !peer_in_ear)
    {
        DEBUG_LOGF("appSmUpdatePeerInEar Prev %u New %u", sm->peer_in_ear, peer_in_ear);
        appConnRulesSetEvent(&sm->task, RULE_EVENT_PEER_OUT_EAR);
    }
    sm->peer_in_ear = peer_in_ear;
}

/*! \brief Send a peer sync to peer earbud.
 */
void appSmSendPeerSync(bool response)
{
    bdaddr peer_addr;
    bdaddr handset_addr;
    uint16 tws_version = DEVICE_TWS_UNKNOWN;
    uint16 battery_level = appBatteryGetVoltage();
    smTaskData *sm = appGetSm();
    peerSigSyncReqData sync_data;

    DEBUG_LOGF("appSmSendPeerSync response %u", response);

    /* Can only send this if we have a peer earbud */
    if (appDeviceGetPeerBdAddr(&peer_addr))
    {
        DEBUG_LOG("appSmSendPeerSync sending");

        /* Try and find last connected handset address, may not exist */
        if (!appDeviceGetHandsetBdAddr(&handset_addr))
            BdaddrSetZero(&handset_addr);
        else
            tws_version = appDeviceTwsVersion(&handset_addr);

        /* Store battery level we sent, so we can compare with peer */
        sm->sync_battery_level = battery_level;

        /* mark sent peer sync as invalid, until we get a confirmation of
         * delivery of the peer sync TX */
        PEER_SYNC_STATE_CLEAR_SENT(sm->peer_sync_state);

        if (!response)
        {
            /* if this isn't a response sync, also mark received as invalid,
             * so that peer sync isn't complete until we get back the response
             * sync from the peer. Prevents rules firing that require up to
             * date peer sync information from peer after local state has
             * changed */
            PEER_SYNC_STATE_CLEAR_RECEIVED(sm->peer_sync_state);
            
            /* not a response sync, increment our TX seqnum */
            PEER_SYNC_SEQNUM_INCR(sm->peer_sync_tx_seqnum);
        }

        sync_data.battery_level = sm->sync_battery_level;
        sync_data.handset_addr = handset_addr;
        sync_data.handset_tws_version = tws_version;
        sync_data.a2dp_connected = appDeviceIsHandsetA2dpConnected();
        sync_data.a2dp_streaming = appDeviceIsHandsetA2dpStreaming();
        sync_data.avrcp_connected = appDeviceIsHandsetAvrcpConnected();
        sync_data.hfp_connected = appDeviceIsHandsetHfpConnected();
        sync_data.is_startup = (appGetState() == APP_STATE_STARTUP);
        sync_data.in_case = appSmIsInCase();
        sync_data.in_ear = appSmIsInEar();
        sync_data.is_pairing = (appGetState() == APP_STATE_HANDSET_PAIRING);
        sync_data.peer_rules_in_progress = appConnRulesInProgress();
        sync_data.have_handset_pairing = !BdaddrIsZero(&handset_addr);
        sync_data.tx_seqnum = sm->peer_sync_tx_seqnum;
        sync_data.rx_seqnum = sm->peer_sync_rx_seqnum;

        /* Attempt to send sync message to peer */
        appPeerSigSyncRequest(&sm->task, &peer_addr, &sync_data);

        /* reset the event marking peer sync as valid, we'll set it
         * again once peer sync is completed */
        appConnRulesResetEvent(RULE_EVENT_PEER_SYNC_VALID);
    }
}

/*! \brief Handle confirmation of peer sync transmission.
 */
void appSmHandlePeerSigSyncConfirm(PEER_SIG_SYNC_CFM_T *cfm)
{
    smTaskData *sm = appGetSm();

    if (cfm->status == peerSigStatusSuccess)
    {
        DEBUG_LOG("appSmHandlePeerSigSyncConfirm, success");

        /* Update peer sync state */
        PEER_SYNC_STATE_SET_SENT(sm->peer_sync_state);

        /* have we successfully received and sent peer sync messages? */
        if (appSmIsPeerSyncComplete())
        {
            /* if we're in the startup state and have completed peer sync,
             * then set the initial core state machine state */
            if (appGetState() == APP_STATE_STARTUP)
            {
                DEBUG_LOG("appSmHandlePeerSigSyncConfirm, startup sync complete, set initial state");
                appSmSetInitialCoreState();
            }

            /* Generate peer sync valid event, may cause previously deferred
             * rules to be evaluated again  */
            appConnRulesSetEvent(appGetSmTask(), RULE_EVENT_PEER_SYNC_VALID);
        }
    }
    else
    {
        DEBUG_LOGF("appSmHandlePeerSigStartupSyncConfirm, failed, status %u", cfm->status);

        /* if we're in the startup state, set the initial core state machine
         * state, despite failing to send a peer sync. Ensures we don't get
         * stuck in the startup state if the other earbud is not available */
        if (appGetState() == APP_STATE_STARTUP)
        {
            DEBUG_LOG("appSmHandlePeerSigStartupSyncConfirm, startup sync failed, set initial state");
            appSmSetInitialCoreState();
        }
    }
}

/*! \brief Handle indication of incoming peer sync from peer.
 */
void appSmHandlePeerSigSyncIndication(PEER_SIG_SYNC_IND_T *ind)
{
    smTaskData *sm = appGetSm();

    DEBUG_LOGF("appSmHandlePeerSigSyncIndication txseq %u rxseq %u", ind->tx_seqnum, ind->rx_seqnum);
    DEBUG_LOGF("appSmHandlePeerSigSyncIndication, battery %u, bdaddr %04x,%02x,%06lx, version %u.%02u, startup %u",
               ind->battery_level, ind->handset_addr.nap, ind->handset_addr.uap, ind->handset_addr.lap,
               ind->tws_version >> 8, ind->tws_version & 0xFF, ind->startup);
    DEBUG_LOGF("appSmHandlePeerSigSyncIndication, peer_a2dp_connected %u, peer_avrcp_connected %u, peer_hfp_conected %u, peer_a2dp_streaming %u",
               ind->peer_a2dp_connected, ind->peer_avrcp_connected, ind->peer_hfp_connected,
               ind->peer_a2dp_streaming);
    DEBUG_LOGF("appSmHandlePeerSigSyncIndication, peer_in_case %u, peer_in_ear %u, peer_is_pairing %u, peer_has_handset_pairing %u RIP %u",
               ind->peer_in_case, ind->peer_in_ear, ind->peer_is_pairing, ind->peer_has_handset_pairing, ind->peer_rules_in_progress);

    /* Store peer's info */
    /* \todo This should probably go in the device manager database */
    sm->peer_battery_level = ind->battery_level;
    sm->peer_handset_addr = ind->handset_addr;
    sm->peer_handset_tws = ind->tws_version;
    sm->peer_a2dp_streaming = ind->peer_a2dp_streaming;
    sm->peer_is_pairing = ind->peer_is_pairing;
    sm->peer_has_handset_pairing = ind->peer_has_handset_pairing;
    sm->peer_rules_in_progress = ind->peer_rules_in_progress;

    /* update state that may generate events to the rules engine */
    appSmUpdatePeerInCase(ind->peer_in_case);
    appSmUpdatePeerInEar(ind->peer_in_ear);
    appSmUpdateA2dpConnected(ind->peer_a2dp_connected);
    appSmUpdateAvrcpConnected(ind->peer_avrcp_connected);
    appSmUpdateHfpConnected(ind->peer_hfp_connected);

    /* RX sequence number from peer indicates it has seen our peer sync TX 
     * this is a response sync matching the latest TX we have sent */
    if (ind->rx_seqnum == sm->peer_sync_tx_seqnum)
    {
        /* Update peer sync RX state */
        PEER_SYNC_STATE_SET_RECEIVED(sm->peer_sync_state);
    }

    /* For a TWS Standard headset, if the peer is connected then this Earbud should
     * also consider itself 'was connected' for those profiles */
    if (!appDeviceIsTwsPlusHandset(&sm->peer_handset_addr))
    {
        if (sm->peer_a2dp_connected)
            appDeviceSetA2dpWasConnected(&sm->peer_handset_addr, sm->peer_a2dp_connected);
        if (sm->peer_hfp_connected)
            appDeviceSetHfpWasConnected(&sm->peer_handset_addr, sm->peer_hfp_connected);
    }

    /* For a TWS+ and TWS Standard headset, if the peer is connected then this Earbud
     * should consider the headset supports those profiles */
    if (sm->peer_a2dp_connected)
        appDeviceSetA2dpIsSupported(&sm->peer_handset_addr);
    if (sm->peer_avrcp_connected)
        appDeviceSetAvrcpIsSupported(&sm->peer_handset_addr);
    if (sm->peer_hfp_connected)
        appDeviceSetHfpIsSupported(&sm->peer_handset_addr, hfp_handsfree_107_profile);  // TODO: Get HFP profile version from peer

    /* TX seqnum has changed from the last we saw, so this is a new peer sync, which
     * requires a response peer sync from us */
     /* Also ensures we send a sync to a startup sync to get a rebooted peer 
     * earbud back to a completed sync state */
    if (ind->tx_seqnum != sm->peer_sync_rx_seqnum)
    {
        sm->peer_sync_rx_seqnum = ind->tx_seqnum;
        DEBUG_LOG("appSmHandlePeerSigSyncIndication new peer sync, send response sync back");
        appSmSendPeerSync(TRUE);
    }

    /* Set peer sync valid event if we've received and successfully send peer sync messages */
    if (appSmIsPeerSyncComplete())
    {
        DEBUG_LOG("appSmHandlePeerSigSyncIndication, peer sync complete");

        /* Check if we're in the startup state */
        if (appGetState() == APP_STATE_STARTUP)
        {
            DEBUG_LOG("appSmHandlePeerSigSyncIndication, startup, set initial state");

            /* Move to core state state */
            appSmSetInitialCoreState();
        }

        /* Run peer sync valid rules */
        appConnRulesSetEvent(appGetSmTask(), RULE_EVENT_PEER_SYNC_VALID);
    }
}

/*! \brief Determine if peer sync is complete.
 */
bool appSmIsPeerSyncComplete(void)
{
    smTaskData *sm = appGetSm();
    return sm->peer_sync_state == SM_PEER_SYNC_COMPLETE;
}


