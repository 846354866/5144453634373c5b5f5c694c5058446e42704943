/*!
\copyright  Copyright (c) 2005 - 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       av_headset_conn_rules.c
\brief	    Connection Rules Module
*/

#include "av_headset.h"
#include "av_headset_conn_rules.h"
#include "av_headset_device.h"
#include "av_headset_log.h"
#include "av_headset_av.h"
#include "av_headset_phy_state.h"

#include <bdaddr.h>
#include <panic.h>

#pragma unitsuppress Unused

/*! \{
    Macros for diagnostic output that can be suppressed.
    Allows debug of the rules module at two levels. */
#define CONNRULES_LOG(x)       //DEBUG_LOG(x)
#define CONNRULES_LOGF(x, ...) //DEBUG_LOGF(x, __VA_ARGS__)

/*! \{
    Macros for diagnostic output that can be suppressed.
    Allows debug of the rules module at two levels. */
#define CONNRULES_TIMING_LOG(x)       DEBUG_LOG(x)
#define CONNRULES_TIMING_LOGF(x, ...) DEBUG_LOGF(x, __VA_ARGS__)

#define RULE_LOG(x)         DEBUG_LOG(x)
#define RULE_LOGF(x, ...)   DEBUG_LOGF(x, __VA_ARGS__)
/*! \} */

/*! Macro to split a uint64 into 2 uint32 that the debug macro can handle. */
#define PRINT_ULL(x)   ((uint32)(((x) >> 32) & 0xFFFFFFFFUL)),((uint32)((x) & 0xFFFFFFFFUL))

/* Forward declaration for use in RULE_ACTION_RUN_PARAM macro below */
static ruleAction appConnRulesCopyRunParams(void* param, size_t size_param);

/*! \brief Macro used by rules to return RUN action with parameters to return.
    Copies the parameters/data into conn_rules where the rules engine can uses
    it when building the action message.
*/
#define RULE_ACTION_RUN_PARAM(x)   appConnRulesCopyRunParams(&(x), sizeof(x))

/*! \brief Macro to set the status of a rule.
    Guards against setting the status of a rule that carrys the
    RULE_FLAG_ALWAYS_EVALUATE property.
*/
#define SET_RULE_STATUS(r, new_status) \
    { \
        if (r->flags == RULE_FLAG_ALWAYS_EVALUATE) \
        { DEBUG_LOG("Cannot set status of RULE_FLAG_ALWAYS_EVALUATE rule"); Panic(); } \
        else \
        { r->status = new_status; } \
    }

/*! \brief Flags to control rule calling.
*/
typedef enum
{
    RULE_FLAG_NO_FLAGS          = 0x00,

    /*! Always evaluate this rule on any event. */
    RULE_FLAG_ALWAYS_EVALUATE   = 0x01,
} ruleFlags;

/*! \brief Function pointer definition for a rule */
typedef ruleStatus (*ruleFunc)(void);

/*! \brief Definition of a rule entry. */
typedef struct
{
    /*! Events that trigger this rule */
    connRulesEvents events;

    /*! Current state of the rule. */
    ruleStatus status;

    ruleFlags flags;

    /*! Pointer to the function to evaluate the rule. */
    ruleFunc rule;

    /*! Message to send when rule determines action to be run. */
    MessageId message;
} ruleEntry;

/*! Macro to declare a function, based on the name of a rule */
#define DEFINE_RULE(name) \
    static ruleAction name(void)

/*! Macro used to create an entry in the rules table */
#define RULE(event, name, message) \
    { event, RULE_STATUS_NOT_DONE, RULE_FLAG_NO_FLAGS, name, message }

/*! Macro used to create an entry in the rules table */
#define RULE_ALWAYS(event, name, message) \
    { event, RULE_STATUS_NOT_DONE, RULE_FLAG_ALWAYS_EVALUATE, name, message }

/*! \{
    Rule function prototypes, so we can build the rule tables below. */
DEFINE_RULE(rulePeerPair);
DEFINE_RULE(ruleAutoHandsetPair);
DEFINE_RULE(rulePeerSync);
DEFINE_RULE(ruleForwardLinkKeys);
DEFINE_RULE(rulePeerConnectForSco);

DEFINE_RULE(ruleSyncConnectHandset);
DEFINE_RULE(ruleSyncConnectPeerHandset);
DEFINE_RULE(ruleSyncConnectPeer);
DEFINE_RULE(ruleDisconnectPeer);

DEFINE_RULE(ruleUserConnectHandset);
DEFINE_RULE(ruleUserConnectPeerHandset);
DEFINE_RULE(ruleUserConnectPeer);

DEFINE_RULE(ruleOutOfCaseConnectHandset);
DEFINE_RULE(ruleOutOfCaseConnectPeerHandset);
DEFINE_RULE(ruleOutOfCaseConnectPeer);

DEFINE_RULE(ruleUpdateMruHandset);
DEFINE_RULE(ruleSendStatusToHandset);
DEFINE_RULE(ruleOutOfEarA2dpActive);
DEFINE_RULE(ruleOutOfEarScoActive);
DEFINE_RULE(ruleInEarScoTransfer);
DEFINE_RULE(ruleOutOfEarLedsEnable);
DEFINE_RULE(ruleInEarLedsDisable);
DEFINE_RULE(ruleInCaseDisconnectHandset);
DEFINE_RULE(ruleInCaseDisconnectPeer);
DEFINE_RULE(ruleInCaseEnterDfu);
DEFINE_RULE(ruleOutOfCaseAllowHandsetConnect);
DEFINE_RULE(ruleInCaseRejectHandsetConnect);
DEFINE_RULE(ruleDfuAllowHandsetConnect);
DEFINE_RULE(ruleIdleHandsetDisconnect);

DEFINE_RULE(rulePageScanUpdate);

DEFINE_RULE(ruleBatteryLevelChangeUnhandled);
DEFINE_RULE(ruleCheckChargerBatteryState);
/*! \} */

/*! \brief Set of rules to run on Earbud startup. */
ruleEntry appConnRules[] =
{
    /*! \{
        Rules that should always run on any event */
    RULE_ALWAYS(RULE_EVENT_PAGE_SCAN_UPDATE,    rulePageScanUpdate,         CONN_RULES_PAGE_SCAN_UPDATE),
    /*! \} */
    /*! \{
        Startup (power on) rules */
    RULE(RULE_EVENT_STARTUP,                    rulePeerPair,               CONN_RULES_PEER_PAIR),
    RULE(RULE_EVENT_STARTUP,                    rulePeerSync,               CONN_RULES_SEND_PEER_SYNC),
    /*! \} */

    RULE(RULE_EVENT_PEER_UPDATE_LINKKEYS,       ruleForwardLinkKeys,        CONN_RULES_PEER_SEND_LINK_KEYS),
    RULE(RULE_EVENT_PEER_CONNECTED,             ruleForwardLinkKeys,        CONN_RULES_PEER_SEND_LINK_KEYS),

    /*! \{
        Rules that are run when handset connects */
    RULE(RULE_EVENT_HANDSET_A2DP_CONNECTED,     rulePeerSync,               CONN_RULES_SEND_PEER_SYNC),
    RULE(RULE_EVENT_HANDSET_A2DP_CONNECTED,     ruleSendStatusToHandset,    CONN_RULES_SEND_STATE_TO_HANDSET),
    RULE(RULE_EVENT_HANDSET_A2DP_CONNECTED,     ruleIdleHandsetDisconnect,  CONN_RULES_IDLE_DISCONNECT_HANDSET),
    RULE(RULE_EVENT_HANDSET_AVRCP_CONNECTED,    rulePeerSync,               CONN_RULES_SEND_PEER_SYNC),
    RULE(RULE_EVENT_HANDSET_AVRCP_CONNECTED,    ruleSendStatusToHandset,    CONN_RULES_SEND_STATE_TO_HANDSET),
    RULE(RULE_EVENT_HANDSET_AVRCP_CONNECTED,    ruleIdleHandsetDisconnect,  CONN_RULES_IDLE_DISCONNECT_HANDSET),
    RULE(RULE_EVENT_HANDSET_HFP_CONNECTED,      rulePeerSync,               CONN_RULES_SEND_PEER_SYNC),
    RULE(RULE_EVENT_HANDSET_HFP_CONNECTED,      ruleSendStatusToHandset,    CONN_RULES_SEND_STATE_TO_HANDSET),
    RULE(RULE_EVENT_HANDSET_HFP_CONNECTED,      ruleIdleHandsetDisconnect,  CONN_RULES_IDLE_DISCONNECT_HANDSET),
    RULE(RULE_EVENT_HANDSET_HFP_CONNECTED,      rulePeerConnectForSco,      CONN_RULES_SEND_PEER_SCOFWD_CONNECT),
    /*! \} */

    /*! \{
        Rules that are run when handset disconnects */
    RULE(RULE_EVENT_HANDSET_A2DP_DISCONNECTED,  rulePeerSync,               CONN_RULES_SEND_PEER_SYNC),
    RULE(RULE_EVENT_HANDSET_AVRCP_DISCONNECTED, rulePeerSync,               CONN_RULES_SEND_PEER_SYNC),
    RULE(RULE_EVENT_HANDSET_HFP_DISCONNECTED,   rulePeerSync,               CONN_RULES_SEND_PEER_SYNC),
    RULE(RULE_EVENT_HANDSET_A2DP_DISCONNECTED,  ruleDisconnectPeer,         CONN_RULES_PEER_DISCONNECT),
    RULE(RULE_EVENT_HANDSET_AVRCP_DISCONNECTED, ruleDisconnectPeer,         CONN_RULES_PEER_DISCONNECT),

    /*! \{
        Receive handset link-key from peer */
    RULE(RULE_EVENT_RX_HANDSET_LINKKEY,         rulePeerSync,               CONN_RULES_SEND_PEER_SYNC),
    RULE(RULE_EVENT_RX_HANDSET_LINKKEY,         ruleSyncConnectPeerHandset, CONN_RULES_CONNECT_HANDSET),
    /*! \} */

    /*! \{
        Rules that are run when peer synchronisation is successful */
    RULE(RULE_EVENT_PEER_SYNC_VALID,            ruleSyncConnectPeer,        CONN_RULES_CONNECT_PEER),
    RULE(RULE_EVENT_PEER_SYNC_VALID,            ruleUpdateMruHandset,       CONN_RULES_UPDATE_MRU_PEER_HANDSET),
    /*! \} */

    /*! \{
        Rules that are run when user has request a connect */
    RULE(RULE_EVENT_USER_CONNECT,               ruleUserConnectPeer,        CONN_RULES_CONNECT_PEER),
    RULE(RULE_EVENT_USER_CONNECT,               ruleUserConnectHandset,     CONN_RULES_CONNECT_HANDSET),
    RULE(RULE_EVENT_USER_CONNECT,               ruleUserConnectPeerHandset, CONN_RULES_CONNECT_PEER_HANDSET),
    /*! \} */

    /*! \{
        Rules that are run on physical state changes */
    RULE(RULE_EVENT_OUT_CASE,                   rulePeerSync,                       CONN_RULES_SEND_PEER_SYNC),
    RULE(RULE_EVENT_OUT_CASE,                   ruleOutOfCaseAllowHandsetConnect,   CONN_RULES_ALLOW_HANDSET_CONNECT),
    RULE(RULE_EVENT_OUT_CASE,                   ruleAutoHandsetPair,                CONN_RULES_HANDSET_PAIR),
    RULE(RULE_EVENT_OUT_CASE,                   ruleOutOfCaseConnectPeer,           CONN_RULES_CONNECT_PEER),
    RULE(RULE_EVENT_OUT_CASE,                   ruleOutOfCaseConnectHandset,        CONN_RULES_CONNECT_HANDSET),
    RULE(RULE_EVENT_OUT_CASE,                   ruleIdleHandsetDisconnect,          CONN_RULES_IDLE_DISCONNECT_HANDSET),

    RULE(RULE_EVENT_IN_CASE,                    rulePeerSync,                       CONN_RULES_SEND_PEER_SYNC),
    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseDisconnectHandset,        CONN_RULES_HANDSET_DISCONNECT),
    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseDisconnectPeer,           CONN_RULES_PEER_DISCONNECT),
    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseEnterDfu,                 CONN_RULES_ENTER_DFU),
    RULE(RULE_EVENT_IN_CASE,                    ruleInCaseRejectHandsetConnect,     CONN_RULES_REJECT_HANDSET_CONNECT),

    RULE(RULE_EVENT_OUT_EAR,                    rulePeerSync,                       CONN_RULES_SEND_PEER_SYNC),
    RULE(RULE_EVENT_OUT_EAR,                    ruleOutOfEarA2dpActive,             CONN_RULES_A2DP_TIMEOUT),
    RULE(RULE_EVENT_OUT_EAR,                    ruleOutOfEarScoActive,              CONN_RULES_SCO_TIMEOUT),
    RULE(RULE_EVENT_OUT_EAR,                    ruleOutOfEarLedsEnable,             CONN_RULES_LED_ENABLE),
    RULE(RULE_EVENT_OUT_EAR,                    ruleIdleHandsetDisconnect,          CONN_RULES_IDLE_DISCONNECT_HANDSET),

    RULE(RULE_EVENT_IN_EAR,                     rulePeerSync,                       CONN_RULES_SEND_PEER_SYNC),
    RULE(RULE_EVENT_IN_EAR,                     ruleInEarLedsDisable,               CONN_RULES_LED_DISABLE),
    RULE(RULE_EVENT_IN_EAR,                     ruleInEarScoTransfer,               CONN_RULES_SCO_TRANSFER),
    /*! \} */

    /*! \{
        Rules that are run on peer state changes */
    RULE(RULE_EVENT_PEER_IN_CASE,               ruleSyncConnectHandset,             CONN_RULES_CONNECT_HANDSET),
    RULE(RULE_EVENT_PEER_HANDSET_DISCONNECTED,  ruleSyncConnectHandset,             CONN_RULES_CONNECT_HANDSET),
    /*! \} */

    RULE(RULE_EVENT_DFU_CONNECT,                ruleDfuAllowHandsetConnect,         CONN_RULES_ALLOW_HANDSET_CONNECT),

    /*! \{
        Rules that are run for battery status changes */
    RULE(RULES_EVENT_BATTERY_TOO_LOW,           ruleCheckChargerBatteryState,       CONN_RULES_LOW_POWER_SHUTDOWN),
    RULE(RULES_EVENT_BATTERY_CRITICAL,          ruleBatteryLevelChangeUnhandled,    CONN_RULES_NOP),
    RULE(RULES_EVENT_BATTERY_LOW,               ruleBatteryLevelChangeUnhandled,    CONN_RULES_NOP),
    RULE(RULES_EVENT_BATTERY_OK,                ruleBatteryLevelChangeUnhandled,    CONN_RULES_NOP),
    /*! \} */

    /*! \{
        Rules that are run for battery status changes */
    RULE(RULES_EVENT_CHARGER_CONNECTED,         ruleCheckChargerBatteryState,       CONN_RULES_LOW_POWER_SHUTDOWN),
    RULE(RULES_EVENT_CHARGER_DISCONNECTED,      ruleCheckChargerBatteryState,       CONN_RULES_LOW_POWER_SHUTDOWN),
    /*! \} */


};

/*! \brief Types of event that can cause connect rules to run. */
typedef enum
{
    RULE_CONNECT_USER,          /*!< User initiated connection */
    RULE_CONNECT_STARTUP,       /*!< Connect on startup */
    RULE_CONNECT_PEER_SYNC,     /*!< Peer sync complete initiated connection */
    RULE_CONNECT_OUT_OF_CASE,   /*!< Out of case initiated connection */
} ruleConnectReason;

/*****************************************************************************
 * RULES FUNCTIONS
 *****************************************************************************/

/*! @brief Rule to determine if Earbud should start automatic peer pairing
    This rule determins if automatic peer pairing should start, it is triggered
    by the startup event.
    @startuml

    start
        if (IsPairedWithPeer()) then (no)
            :Start peer pairing;
            end
        else (yes)
            :Already paired;
            stop
    @enduml 
*/
static ruleAction rulePeerPair(void)
{
    if (!appDeviceGetPeerBdAddr(NULL))
    {
        RULE_LOG("ruleStartupPeerPaired, run");
        return RULE_ACTION_RUN;
    }
    else
    {
        RULE_LOG("ruleStartupPeerPaired, done");
        return RULE_ACTION_COMPLETE;
    }
}

/*! @brief Rule to determine if Earbud should start automatic handset pairing
    @startuml

    start
        if (IsInCase()) then (yes)
            :Earbud is in case, do nothing;
            end
        endif
        if (IsPairedWithHandset()) then (yes)
            :Already paired with handset, do nothing;
            end
        endif
        if (IsPeerSyncComplete()) then (no)
            :Not sync'ed with peer, defer;
            end
        endif
        if (IsPeerPairing()) then (yes)
            :Peer is already pairing, do nothing;
            end
        endif
        if (IsPeerPairWithHandset()) then (yes)
            :Peer is already paired with handset, do nothing;
            end
        endif
        if (IsPeerInCase()) then (yes)
            :Start pairing, peer is in case;
            stop
        endif

        :Both Earbuds are out of case;
        if (IsPeerLeftEarbud) then (yes)
            stop
        else (no)
            end
        endif
    @enduml 
*/
static ruleAction ruleAutoHandsetPair(void)
{
    /* NOTE: Ordering of these checks is important */

    if (appSmIsInCase())
    {
        RULE_LOG("ruleAutoHandsetPair, ignore, we're in the case");
        return RULE_ACTION_IGNORE;
    }

    if (appDeviceGetHandsetBdAddr(NULL))
    {
        RULE_LOG("ruleAutoHandsetPair, complete, already paired with handset");
        return RULE_ACTION_COMPLETE;
    }

    if (!appSmIsPeerSyncComplete())
    {
        RULE_LOG("ruleAutoHandsetPair, defer, not synced with peer");
        return RULE_ACTION_DEFER;
    }

    if (appSmIsPeerPairing())
    {
        RULE_LOG("ruleAutoHandsetPair, defer, peer is already in pairing mode");
        return RULE_ACTION_DEFER;
    }

    if (appSmIsPairing())
    {
        RULE_LOG("ruleAutoHandsetPair, ignore, already in pairing mode");
        return RULE_ACTION_IGNORE;
    }

    if (appSmHasPeerHandsetPairing())
    {
        RULE_LOG("ruleAutoHandsetPair, complete, peer is already paired with handset");
        return RULE_ACTION_COMPLETE;
    }

    if (appSmIsPeerInCase())
    {
        RULE_LOG("ruleAutoHandsetPair, run, no paired handset, we're out of case, peer is in case");
        return RULE_ACTION_RUN;
    }
    else
    {
        /* Both out of case, neither pairing or paired.  Left wins, right loses */
        if (appConfigIsLeft())
        {
            RULE_LOG("ruleAutoHandsetPair, run, no paired handset, we're out of case, peer is out of case, we're left earbud");
            return RULE_ACTION_RUN;
        }
        else
        {
            RULE_LOG("ruleAutoHandsetPair, ignore, no paired handset, we're out of case, peer is out of case, but we're right earbud");
            return RULE_ACTION_IGNORE;
        }
    }
}

/*! @brief Rule to determine if Earbud should attempt to synchronize with peer Earbud
    @startuml

    start
        if (IsPairedWithPeer() and !IsInCaseDfu()) then (yes)
            :Start peer sync;
            stop
        else (no)
            :Already paired;
            end
    @enduml 
*/
static ruleAction rulePeerSync(void)
{
    if (appDeviceGetPeerBdAddr(NULL) && appGetState() != APP_STATE_IN_CASE_DFU)
    {
        RULE_LOGF("rulePeerSync, run (state x%x)",appGetState());
        return RULE_ACTION_RUN;
    }
    else
    {
        RULE_LOG("rulePeerSync, ignore as there's no peer - or in DFU");
        return RULE_ACTION_IGNORE;
    }
}

static ruleAction rulePeerConnectForSco(void)
{
    if (!appDeviceGetPeerBdAddr(NULL))
    {
        RULE_LOG("rulePeerConnectForSco, ignore (no peer)");
        return RULE_ACTION_IGNORE;
    }
    if (appScoFwdHasConnection())
    {
        RULE_LOG("rulePeerConnectForSco, complete - link is up");
        return RULE_ACTION_COMPLETE;
    }
    return RULE_ACTION_RUN;
}



/*! @brief Rule to determine if Earbud should attempt to forward handset link-key to peer
    @startuml

    start
        if (IsPairedWithPeer()) then (yes)
            :Forward any link-keys to peer;
            stop
        else (no)
            :Not paired;
            end
    @enduml 
*/
static ruleAction ruleForwardLinkKeys(void)
{
    if (appDeviceGetPeerBdAddr(NULL))
    {
        RULE_LOG("ruleForwardLinkKeys, run");
        return RULE_ACTION_RUN;
    }
    else
    {
        RULE_LOG("ruleForwardLinkKeys, ignore as there's no peer");
        return RULE_ACTION_IGNORE;
    }
}

/*! @brief Sub-rule to determine if Earbud should connect based on battery level.

    @startuml

    start
        if (Battery Voltage > Peer Battery Voltage) then (yes)
            :Our battery voltage is higher, connect;
            end
        else
            if (Battery Voltage = Peer Battery Voltage) then (yes)
                if (IsLeftEarbud()) then (yes)
                    :Left takes preference;
                    end
                else (no)
                    :Right doesn't connect;
                    stop
                endif
            else
                :Our battery voltage is lower, don't connect;
                stop
        endif
    endif
    @enduml 
*/
static ruleAction ruleConnectBatteryVoltage(ruleConnectReason reason)
{
    uint16 battery_level, peer_battery_level;
    UNUSED(reason);

    appSmGetPeerBatteryLevel(&battery_level, &peer_battery_level);
    RULE_LOGF("ruleConnectBatteryVoltage, battery %u, peer battery %u", battery_level, peer_battery_level);

    if (battery_level > peer_battery_level)
    {
        RULE_LOG("ruleConnectBatteryVoltage, run as our battery is higher");
        return RULE_ACTION_RUN;
    }
    else if (battery_level == peer_battery_level)
    {
        if (appConfigIsLeft())
        {
            RULE_LOG("ruleConnectBatteryVoltage, equal, run as left earbud");
            return RULE_ACTION_RUN;
        }
        else
        {
            RULE_LOG("ruleConnectBatteryVoltage, equal, ignore as right earbud");
            return RULE_ACTION_IGNORE;
        }
    }
    else
    {
            RULE_LOG("ruleConnectBatteryVoltage, ignore as our battery is lower");
            return RULE_ACTION_IGNORE;
    }
}

/*! @brief Sub-rule to determine if Earbud should connect to standard handset
*/
static ruleAction ruleConnectHandsetStandard(ruleConnectReason reason)
{
    if (reason == RULE_CONNECT_USER)
    {     
        RULE_LOG("ruleConnectHandsetStandard, run as standard handset and user requested connection");
        return RULE_ACTION_RUN;
    }
    else if ((reason == RULE_CONNECT_PEER_SYNC) || (reason == RULE_CONNECT_OUT_OF_CASE))
    {
        /* Check if out of case */
        if (appSmIsOutOfCase())
        {
            /* Check if peer is in case */
            if (appSmIsPeerInCase())
            {
                RULE_LOG("ruleConnectHandsetStandard, run as standard handset and not in case but peer is in case");
                return RULE_ACTION_RUN;
            }
            else
            {
                /* Both out of case */
                if (reason == RULE_CONNECT_OUT_OF_CASE)
                {
                    RULE_LOG("ruleConnectHandsetStandard, calling ruleConnectBatteryVoltage() as standard handset and both out of case but peer not connected");
                    return ruleConnectBatteryVoltage(reason);
                }
                else
                {
                    RULE_LOG("ruleConnectHandsetStandard, ignore as standard handset and both out of case");
                    return RULE_ACTION_IGNORE;                    
                }
            }
        }
        else
        {
            RULE_LOG("ruleConnectHandsetStandard, ignore as in case");
            return RULE_ACTION_IGNORE;                                
        }
    }
    else
    {
        return ruleConnectBatteryVoltage(reason);
    }
}

/*! @brief Rule to determine if Earbud should connect to Handset
    @startuml

    start
    if (IsInCase()) then (yes)
        :Never connect when in case;
        end
    endif

    if (IsPeerSyncComplete()) then (yes)
        if (Not IsPairedWithHandset()) then (yes)
            :Not paired with handset, don't connect;
            end
        endif        
        if (IsHandsetA2dpConnected() and IsHandsetAvrcpConnected() and IsHandsetHfpConnected()) then (yes)
            :Already connected;
            end
        endif

        if (IsTwsPlusHandset()) then (yes)
            :Handset is a TWS+ device;
            if (WasConnected() or Reason is 'User', 'Start-up' or 'Out of Case') then (yes)
                if (not just paired) then (yes)
                    :Connect to handset;
                    end
                else
                    :Just paired, handset will connect to us;
                    stop
                endif
            else (no)
                :Wasn't connected before;
                stop
            endif
        else (no)
            if (IsPeerConnectedA2dp() or IsPeerConnectedAvrcp() or IsPeerConnectedHfp()) then (yes)
                :Peer already has profile(s) connected, don't connect;
                stop
            else (no)
                if (WasConnected() or Reason is 'User', 'Start-up' or 'Out of Case') then (yes)
                    :run RuleConnectHandsetStandard();
                    end
                else (no)
                    :Wasn't connected before;
                    stop
                endif
            endif
        endif
    else (no)
        :Not sync'ed with peer;
        if (IsPairedWithHandset() and IsHandsetTwsPlus() and WasConnected()) then (yes)
            :Connect to handset, it is TWS+ handset;
            stop
        else (no)
            :Don't connect, not TWS+ handset;
            end
        endif
    endif

    @enduml 
*/
static ruleAction ruleConnectHandset(ruleConnectReason reason)
{
    bdaddr handset_addr;

    RULE_LOGF("ruleConnectHandset, reason %u", reason);

    /* Don't attempt to connect if we're in the case */
    if (appSmIsInCase())
    {
        RULE_LOG("ruleConnectHandset, ignore as in case");
        return RULE_ACTION_IGNORE;
    }

    /* Check we have sync'ed with peer */
    if (appSmIsPeerSyncComplete())
    {
        /* If we're not paired with handset then don't connect */
        if (!appDeviceGetHandsetBdAddr(&handset_addr))
        {
            RULE_LOG("ruleConnectHandset, ignore as not paired with handset");
            return RULE_ACTION_IGNORE;
        }

        /* If we're already connected to handset then don't connect */
        if (appDeviceIsHandsetA2dpConnected() && appDeviceIsHandsetAvrcpConnected() && appDeviceIsHandsetHfpConnected())
        {
            RULE_LOG("ruleConnectHandset, ignore as already connected to handset");
            return RULE_ACTION_IGNORE;
        }

        /* Peer is not connected to handset, so we should connect to our handset if it's a TWS+ handset or
           it's a standard handset and our battery level is higer */

        /* Check if TWS+ handset, if so just connect, otherwise compare battery levels
         * if we have higher battery level connect to handset */
        if (appDeviceIsTwsPlusHandset(&handset_addr))
        {
            /* this call will read persistent store, so just called once and re-use
             * results */
            uint8 profiles = appDeviceWasConnectedProfiles(&handset_addr);

            /* Always attempt to connect HFP and A2DP if user initiated connect, or out-of-case connect */
            if ((reason == RULE_CONNECT_OUT_OF_CASE) || (reason == RULE_CONNECT_USER))
                profiles |= DEVICE_PROFILE_HFP | DEVICE_PROFILE_A2DP;

            /* Check if device was connected before, or we connecting due to user request */
            if (profiles != 0 ||
                (reason == RULE_CONNECT_USER) ||
                (reason == RULE_CONNECT_STARTUP) ||
                (reason == RULE_CONNECT_OUT_OF_CASE))
            {
                if (!appDeviceHasJustPaired(&handset_addr))
                {
                    RULE_LOGF("ruleConnectHandset, run as TWS+ handset for profiles:%u", profiles);
                    return RULE_ACTION_RUN_PARAM(profiles);
                }
                else
                {
                    RULE_LOG("ruleConnectHandset, ignore as just paired with TWS+ handset");
                    return RULE_ACTION_IGNORE;
                }
            }
            else
            {
                RULE_LOG("ruleConnectHandset, ignore as TWS+ handset but wasn't connected before");
                return RULE_ACTION_IGNORE;
            }
        }
        else
        {
            /* If peer is connected to handset then we shouldn't connect using this rule, use ruleConnectPeerHandset. */
            if (appSmIsPeerHandsetA2dpConnected() || appSmIsPeerHandsetHfpConnected())
            {
                RULE_LOG("ruleConnectHandset, ignore as peer has already connected");
                return RULE_ACTION_IGNORE;
            }

            /* Check if device was connected before, or we connecting due to user request or startup */
            if (appDeviceWasConnected(&handset_addr) ||
                (reason == RULE_CONNECT_USER) ||
                (reason == RULE_CONNECT_STARTUP) ||
                (reason == RULE_CONNECT_OUT_OF_CASE))
            {
                uint8 profiles = appDeviceWasConnectedProfiles(&handset_addr);

                /* Always attempt to connect HFP and A2DP if user initiated connect, or out-of-case connect */
                if ((reason == RULE_CONNECT_OUT_OF_CASE) || (reason == RULE_CONNECT_USER))
                    profiles |= DEVICE_PROFILE_HFP | DEVICE_PROFILE_A2DP;

                RULE_LOG("ruleConnectHandset, calling ruleConnectHandsetStandard()");
                if (ruleConnectHandsetStandard(reason) == RULE_ACTION_RUN)
                {
                    RULE_LOG("ruleConnectHandset, run as standard handset we were connected to before");
                    return RULE_ACTION_RUN_PARAM(profiles);
                }
                else
                {
                    RULE_LOG("ruleConnectHandset, ignore, standard handset but not connected before");
                    return RULE_ACTION_IGNORE;
                }
            }
            else
            {
                RULE_LOG("ruleConnectHandset, ignore as standard handset but wasn't connected before");
                return RULE_ACTION_IGNORE;
            }
        }
    }
    else
    {
        uint8 profiles = 0;
        if (appDeviceGetHandsetBdAddr(&handset_addr) &&
            appDeviceIsTwsPlusHandset(&handset_addr) &&
            ((profiles = appDeviceWasConnectedProfiles(&handset_addr)) != 0))
        {
            RULE_LOG("ruleConnectHandset, run as TWS+ handset, as connected before, despite peer sync fail");
            return RULE_ACTION_RUN_PARAM(profiles);
        }
        else
        {
            RULE_LOG("ruleConnectHandset, defer as not sync'ed with peer");
            return RULE_ACTION_DEFER;
        }
    }
}

/*! @brief Wrapper around ruleConnectHandset() that calls it with 'Peer sync' connect reason
*/
static ruleAction ruleSyncConnectHandset(void)
{
    return ruleConnectHandset(RULE_CONNECT_PEER_SYNC);
}

/*! @brief Wrapper around ruleConnectHandset() that calls it with 'User' connect reason
*/
static ruleAction ruleUserConnectHandset(void)
{
    return ruleConnectHandset(RULE_CONNECT_USER);
}

/*! @brief Wrapper around ruleConnectHandset() that calls it with 'Out of case' connect reason
*/
static ruleAction ruleOutOfCaseConnectHandset(void)
{
    RULE_LOG("ruleOutOfCaseConnectHandset");
    return ruleConnectHandset(RULE_CONNECT_OUT_OF_CASE);
}

/*! @brief Rule to determine if Earbud should connect to Handset even when synchronisation with peer Earbud failed
    @startuml

    start
    if (IsInCase()) then (yes)
        :Never connect when in case;
        stop
    endif

    if (Not IsPeerSyncComplete()) then (yes)
        :Not sync'ed with peer;
        if (IsPairedWithHandset() and Not IsHandsetConnected()) then (yes)
            if (IsTwsPlusHandset()) then (yes)
                :Connect to handset, it is TWS+ handset;
                stop
            else (no)
                :Don't connect, not TWS+ handset;
                end
            endif
        else (no)
            :Don't connected, not paired or already connected;
            end
        endif
    else
        :Do nothing as not sync'ed with peer Earbud;
        end
    endif
    @enduml
*/
static ruleAction ruleNoSyncConnectHandset(void)
{
    bdaddr handset_addr;

    /* Don't attempt to connect if we're in the case */
    if (appSmIsInCase())
    {
        RULE_LOG("ruleConnectHandset, ignore as nin case");
        return RULE_ACTION_IGNORE;
    }

    /* Check we haven't sync'ed with peer */
    if (!appSmIsPeerSyncComplete())
    {
        /* Not sync'ed with peer, so connect to handset if it's a TWS+ handset */

        /* Check we're paired with handset and not already connected */
        if (appDeviceGetHandsetBdAddr(&handset_addr) && !appDeviceIsHandsetConnected())
        {
            /* Check if TWS+ handset, if so just connect */
            if (appDeviceIsTwsPlusHandset(&handset_addr))
            {
                RULE_LOG("ruleConnectHandsetNoSync, run as not sync'ed but TWS+ handset");
                return RULE_ACTION_RUN;
            }
            else
            {
                RULE_LOG("ruleConnectHandsetNoSync, ignore as not sync'ed but standard handset");
                return RULE_ACTION_IGNORE;
            }
        }
        else
        {
            RULE_LOG("ruleConnectHandsetNoSync, ignore as not paired or already connected to handset");
            return RULE_ACTION_IGNORE;
        }
    }
    else
    {
        RULE_LOG("ruleConnectHandsetNoSync, ignore as sync'ed with peer");
        return RULE_ACTION_IGNORE;
    }
}

/*! @brief Rule to determine if Earbud should connect to peer's Handset
    @startuml

    start
    if (IsInCase()) then (yes)
        :Never connect when in case;
        stop
    endif

    if (Not IsPeerSyncComplete()) then (yes)
        :Not sync'ed with peer;
        end
    endif

    if (IsPeerHandsetA2dpConnected() or IsPeerHandsetAvrcpConnected() or IsPeerHandsetHfpConnected()) then (yes)
        if (IsPeerHandsetTws()) then (yes)
            if (IsPairedWithHandset())) then (yes)
                if (Not JustPaired()) then (yes)
                    if (Reason is 'User' or 'Start-up' or 'Out of case') then (yes)
                        :Connect to peer's handset;
                        stop
                    else (no)
                        :Don't connect to peer's handset;
                        end
                    endif
                else (no)
                    :Don't connect as just paired;
                    end
                endif
            else (no)
                :Not paired with peer's handset;
                end
            endif
        else (no)
            :Peer is connected to standard handset;
            end
        endif
    else (no)
        :Don't connect as peer is not connected to handset;
        end
    endif
    @enduml 
*/
static ruleAction ruleConnectPeerHandset(ruleConnectReason reason)
{
    connRulesTaskData *conn_rules = appGetConnRules();

    /* Don't attempt to connect if we're in the case */
    if (appSmIsInCase())
    {
        RULE_LOG("ruleConnectHandset, ignore as nin case");
        return RULE_ACTION_IGNORE;
    }

    /* Don't run rule if we haven't sync'ed with peer */
    if (!appSmIsPeerSyncComplete())
    {
        RULE_LOG("ruleConnectPeerHandset, defer as not sync'ed with peer");
        return RULE_ACTION_DEFER;
    }

    /* If peer is connected to handset then we should also connect to this handset if it's TWS+ */
    if (appSmIsPeerHandsetA2dpConnected() || appSmIsPeerHandsetHfpConnected())
    {
        /*  Check peer's handset is TWS+ */
        if (appSmIsPeerHandsetTws())
        {
            bdaddr handset_addr;
            appSmGetPeerHandsetAddr(&handset_addr);

            /* Check we paired with this handset */
            if (appDeviceIsHandset(&handset_addr))
            {
                if (!conn_rules->allow_connect_after_pairing &&
                    !appDeviceHasJustPaired(&handset_addr))
                {
                    if ((reason == RULE_CONNECT_USER) ||
                        (reason == RULE_CONNECT_STARTUP) ||
                        (reason == RULE_CONNECT_OUT_OF_CASE)
                        || (conn_rules->allow_connect_after_pairing && 
                            appDeviceHasJustPaired(&handset_addr))
                        )
                    {
                        uint8 profiles = 0;

                        if (appSmIsPeerHandsetA2dpConnected())
                            profiles |= DEVICE_PROFILE_A2DP;
                        if (appSmIsPeerHandsetHfpConnected())
                            profiles |= DEVICE_PROFILE_HFP;
                        if (appSmIsPeerHandsetAvrcpConnected())
                            profiles |= DEVICE_PROFILE_AVRCP;

                        RULE_LOGF("ruleConnectPeerHandset, run as peer is connected to TWS+ handset, profiles:%u", profiles);

                        return RULE_ACTION_RUN_PARAM(profiles);
                    }
                    else
                    {
                        RULE_LOG("ruleConnectPeerHandset, ignore as peer is connected to TWS+ handset but not user or startup connect and not just paired (or allow_connect_after_pairing disabled)");
                        return RULE_ACTION_IGNORE;
                    }
                }
                else
                {
                    RULE_LOG("ruleConnectPeerHandset, ignore as just paired with peer's TWS+ handset or allow_connect_after_pairing disabled");
                    return RULE_ACTION_IGNORE;
                }
            }
            else
            {
                RULE_LOG("ruleConnectPeerHandset, ignore as peer is connected to TWS+ handset but we're not paired with it");
                return RULE_ACTION_IGNORE;
            }
        }
        else
        {
            RULE_LOG("ruleConnectPeerHandset, ignore as peer is connected to standard handset");
            return RULE_ACTION_IGNORE;
        }
    }
    else
    {
        /* Peer is not connected to handset, don't connect as ruleConnectHandset handles this case */
        RULE_LOG("ruleConnectPeerHandset, done as peer is not connected");
        return RULE_ACTION_COMPLETE;
    }
}

/*! @brief Wrapper around ruleSyncConnectPeerHandset() that calls it with 'Peer sync' connect reason
*/
static ruleAction ruleSyncConnectPeerHandset(void)
{
    return ruleConnectPeerHandset(RULE_CONNECT_PEER_SYNC);
}

/*! @brief Wrapper around ruleSyncConnectPeerHandset() that calls it with 'User' connect reason
*/
static ruleAction ruleUserConnectPeerHandset(void)
{
    return ruleConnectPeerHandset(RULE_CONNECT_USER);
}

/*! @brief Wrapper around ruleSyncConnectPeerHandset() that calls it with 'Out of case' connect reason
*/
static ruleAction ruleOutOfCaseConnectPeerHandset(void)
{
    RULE_LOG("ruleOutOfCaseConnectPeerHandset");
    return ruleConnectPeerHandset(RULE_CONNECT_OUT_OF_CASE);
}

/*! @brief Rule to determine if Earbud should connect A2DP & AVRCP to peer Earbud
    @startuml

    start
    if (IsPeerA2dpConnected()) then (yes)
        :Already connected;
        end
    endif

    if (IsPeerSyncComplete()) then (yes)
        if (IsPeerInCase()) then (yes)
            :Peer is in case, so don't connect to it;
            end
        endif

        if (IsPeerHandsetA2dpConnected() or IsPeerHandsetHfpConnected()) then (yes)
            if (IsPeerHandsetTws()) then (yes)
                :Don't need to connect to peer, as peer is connected to TWS+ handset;
                end
            else (no)
                :Don't need to connect, peer will connect to us;
                end
            endif
        else (no)    
            :Peer is not connected to handset yet;
            if (IsPairedWithHandset()) then (yes)
                if (Not IsTwsHandset()) then (yes)
                    if (IsHandsetA2dpConnected() or IsHandsetHfpConnected()) then (yes)
                        :Connect to peer as  connected to standard handset, peer won't be connected;
                        stop
                    else (no)
                        :Run RuleConnectHandsetStandard() to determine if we're going to connect to handset;
                        if (RuleConnectHandsetStandard()) then (yes)
                            :Will connect to handset, so should also connect to peer;
                            stop
                        else (no)
                            :Won't connect to handset, so don't connect to peer;
                            end
                        endif
                    endif
                else (no)
                    :Don't connect to peer, as connected to TWS+ handset;
                    end
                endif
            else (no)
                :Don't connect to peer, as not paired with handset;
                end
            endif
        endif
    else (no)
        :Not sync'ed with peer;
        end
    endif

    @enduml 
*/
static ruleAction ruleConnectPeer(ruleConnectReason reason)
{
    bdaddr handset_addr;

    /* Don't run rule if we're connected to peer */
    if (appDeviceIsPeerA2dpConnected())
    {
        RULE_LOG("ruleConnectPeer, ignore as already connected to peer");
        return RULE_ACTION_IGNORE;
    }

    /* Check we have sync'ed with peer */
    if (appSmIsPeerSyncComplete())
    {
        /* Check if peer is in case */
        if (appSmIsPeerInCase())
        {
            RULE_LOG("ruleConnectPeer, ignore as peer is in case");
            return RULE_ACTION_IGNORE;
        }

        /* Check if peer is connected to handset */
        if (appSmIsPeerHandsetA2dpConnected() || appSmIsPeerHandsetHfpConnected())
        {
            /* Don't connect to peer if handset is TWS+ */
            if (appSmIsPeerHandsetTws())
            {
                RULE_LOG("ruleConnectPeer, ignore as peer is connected to TWS+ handset");
                return RULE_ACTION_IGNORE;
            }
            else
            {
                RULE_LOG("ruleConnectPeer, ignore as peer is connected to standard handset and peer will connect to us");
                return RULE_ACTION_IGNORE;
            }
        }
        else
        {
            /* Peer is not connected to handset yet */
            /* Get handset address */
            if (appDeviceGetHandsetBdAddr(&handset_addr))
            {
                /* Check if the handset we would connect to is a standard handset */
                if (!appDeviceIsTwsPlusHandset(&handset_addr))
                {
                    /* Check if we're already connected to handset */
                    if (appDeviceIsHandsetA2dpConnected() || appDeviceIsHandsetHfpConnected())
                    {
                        RULE_LOG("ruleConnectPeer, run as connected to standard handset, peer won't be connected");
                        return RULE_ACTION_RUN;
                    }
                    else
                    {
                        /* Not connected to handset, if we are going to connect to standard handset, we should also connect to peer */
                        RULE_LOG("ruleConnectPeer, calling ruleConnectHandsetStandard() to determine if we're going to connect to handset");
                        if (ruleConnectHandsetStandard(reason) == RULE_ACTION_RUN)
                        {
                            RULE_LOG("ruleConnectPeer, run as connected/ing to standard handset");
                            return RULE_ACTION_RUN;
                        }
                        else
                        {
                            RULE_LOG("ruleConnectPeer, ignore as not connected/ing to standard handset");
                            return RULE_ACTION_IGNORE;
                        }
                    }
                }
                else
                {
                    RULE_LOG("ruleConnectPeer, ignore as connected/ing to TWS+ handset");
                    return RULE_ACTION_IGNORE;
                }
            }
            else
            {
                RULE_LOG("ruleConnectPeer, ignore as no handset, so no need to connect to peer");
                return RULE_ACTION_IGNORE;
            }
        }
    }
    else
    {
        /* Peer sync is not complete */
        RULE_LOG("ruleConnectPeer, defer as peer sync not complete");
        return RULE_ACTION_DEFER;
    }
}

/*! @brief Wrapper around ruleSyncConnectPeer() that calls it with 'Peer sync' connect reason
*/
static ruleAction ruleSyncConnectPeer(void)
{
    return ruleConnectPeer(RULE_CONNECT_PEER_SYNC);
}

/*! @brief Wrapper around ruleSyncConnectPeer() that calls it with 'User' connect reason
*/
static ruleAction ruleUserConnectPeer(void)
{
    return ruleConnectPeer(RULE_CONNECT_USER);
}

/*! @brief Wrapper around ruleSyncConnectPeer() that calls it with 'Out of case' connect reason
*/
static ruleAction ruleOutOfCaseConnectPeer(void)
{
    RULE_LOG("ruleOutOfCaseConnectPeer");
    return ruleConnectPeer(RULE_CONNECT_OUT_OF_CASE);
}

/*! @brief Rule to determine if most recently used handset should be updated
    @startuml

    start
    if (Not IsPeerSyncComplete()) then (yes)
        :Peer sync not completed;
        end 
    endif

    if (IsPeerHandsetA2dpConnected() or IsPeerHandsetHfpConnected()) then (yes)
        if (IsPairedPeerHandset()) then (yes)
            :Update MRU handzset as peer is connected to handset;
            stop
        else (no)
            :Do nothing as not paired to peer's handset;
            end
        endif
    else
        :Do nothing as peer is not connected to handset;
        end
    endif
    @enduml
*/
static ruleAction ruleUpdateMruHandset(void)
{
    /* Don't run rule if we haven't sync'ed with peer */
    if (!appSmIsPeerSyncComplete())
    {
        RULE_LOG("ruleUpdateMruHandset, defer as not sync'ed with peer");
        return RULE_ACTION_DEFER;
    }

    /* If peer is connected to handset then we should mark this handset as most recently used */
    if (appSmIsPeerHandsetA2dpConnected() || appSmIsPeerHandsetHfpConnected())
    {
        /* Check we paired with this handset */
        bdaddr handset_addr;
        appSmGetPeerHandsetAddr(&handset_addr);
        if (appDeviceIsHandset(&handset_addr))
        {
            RULE_LOG("ruleUpdateMruHandset, run as peer is connected to handset");
            return RULE_ACTION_RUN;
        }
        else
        {
            RULE_LOG("ruleUpdateMruHandset, ignore as not paired with peer's handset");
            return RULE_ACTION_IGNORE;
        }
    }
    else
    {
        /* Peer is not connected to handset */
        RULE_LOG("ruleUpdateMruHandset, ignore as peer is not connected");
        return RULE_ACTION_IGNORE;
    }

}

/*! @brief Rule to determine if Earbud should send status to handset over HFP and/or AVRCP
    @startuml

    start
    if (IsPairedHandset() and IsTwsPlusHandset()) then (yes)
        if (IsHandsetHfpConnected() or IsHandsetAvrcpConnected()) then (yes)
            :HFP and/or AVRCP connected, send status update;
            stop
        endif
    endif

    :Not connected with AVRCP or HFP to handset;
    end
    @enduml
*/
static ruleAction ruleSendStatusToHandset(void)
{
    bdaddr handset_addr;

    if (appDeviceGetHandsetBdAddr(&handset_addr) && appDeviceIsTwsPlusHandset(&handset_addr))
    {
        if (appDeviceIsHandsetHfpConnected() || appDeviceIsHandsetAvrcpConnected())
        {
            RULE_LOG("ruleSendStatusToHandset, run as TWS+ handset");
            return RULE_ACTION_RUN;
        }
    }

    RULE_LOG("ruleSendStatusToHandset, ignore as not connected to TWS+ handset");
    return RULE_ACTION_IGNORE;
}

/*! @brief Rule to determine if A2DP streaming when out of ear
    Rule is triggered by the 'out of ear' event
    @startuml

    start
    if (IsAvStreaming()) then (yes)
        :Run rule, as out of ear with A2DP streaming;
        stop
    endif
    end
    @enduml
*/
static ruleAction ruleOutOfEarA2dpActive(void)
{
    if (appAvIsStreaming())
    {
        RULE_LOG("ruleOutOfEarA2dpActive, run as A2DP is active and earbud out of ear");
        return RULE_ACTION_RUN;
    }

    RULE_LOG("ruleOutOfEarA2dpActive, ignore as A2DP not active out of ear");
    return RULE_ACTION_IGNORE;
}

/*! @brief Rule to determine if SCO active when out of ear
    Rule is triggered by the 'out of ear' event
    @startuml

    start
    if (IsScoActive()) then (yes)
        :Run rule, as out of ear with SCO active;
        stop
    endif
    end
    @enduml
*/
static ruleAction ruleOutOfEarScoActive(void)
{
    /* Don't run rule if we haven't sync'ed with peer */
    if (!appSmIsPeerSyncComplete())
    {
        RULE_LOG("ruleOutOfEarScoActive, defer as not sync'ed with peer");
        return RULE_ACTION_DEFER;
    }

    if (appHfpIsScoActive())
    {
        RULE_LOG("ruleOutOfEarScoActive, run as SCO is active and earbud out of ear");
        return RULE_ACTION_RUN;
    }

    RULE_LOG("ruleOutOfEarScoActive, ignore as SCO not active out of ear");
    return RULE_ACTION_IGNORE;
}


static ruleAction ruleInEarScoTransfer(void)
{
    /* Don't run rule if we haven't sync'ed with peer */
    if (!appSmIsPeerSyncComplete())
    {
        RULE_LOG("ruleInEarScoTransfer, defer as not sync'ed with peer");
        return RULE_ACTION_DEFER;
    }

    if (appHfpIsCallActive())
    {
        RULE_LOG("ruleInEarScoTransfer, run as call is active and earbud in ear");
        return RULE_ACTION_RUN;
    }

    RULE_LOG("ruleInEarScoTransfer, ignore as SCO not active out of ear");
    return RULE_ACTION_IGNORE;
}

/*! @brief Rule to determine if LED should be enabled when out of ear
    Rule is triggered by the 'out of ear' event
    @startuml

    start
    if (Not IsLedsInEarEnabled()) then (yes)
        :Run rule, as out of ear and LEDs were disabled in ear;
        stop
    endif
    end
    @enduml
*/
static ruleAction ruleOutOfEarLedsEnable(void)
{
    if (!appConfigInEarLedsEnabled())
    {
        RULE_LOG("ruleOutOfEarLedsEnable, run as out of ear");
        return RULE_ACTION_RUN;
    }
    else
    {
        RULE_LOG("ruleOutOfEarLedsEnable, ignore as out of ear but in ear LEDs enabled");
        return RULE_ACTION_IGNORE;
    }
}

/*! @brief Rule to determine if LED should be disabled when in ear
    Rule is triggered by the 'in ear' event
    @startuml

    start
    if (Not IsLedsInEarEnabled()) then (yes)
        :Run rule, as in ear and LEDs are disabled in ear;
        stop
    endif
    end
    @enduml
*/
static ruleAction ruleInEarLedsDisable(void)
{
    if (!appConfigInEarLedsEnabled())
    {
        RULE_LOG("ruleInEarLedsDisable, run as in ear");
        return RULE_ACTION_RUN;
    }
    else
    {
        RULE_LOG("ruleInEarLedsDisable, ignore as in ear but in ear LEDs enabled");
        return RULE_ACTION_IGNORE;
    }
}

/*! @brief Rule to determine if Earbud should disconnect from handset when put in case
    Rule is triggered by the 'in case' event
    @startuml

    start
    if (IsInCase() and IsHandsetConnected() and Not DfuUpgradePending()) then (yes)
        :Disconnect from handset as now in case;
        stop
    endif
    end
    @enduml
*/
static ruleAction ruleInCaseDisconnectHandset(void)
{
    if (   appSmIsInCase() 
        && appDeviceIsHandsetConnected() 
        && !appUpgradeDfuPending())
    {
        RULE_LOG("ruleInCaseDisconnectHandset, run as in case and handset connected");
        return RULE_ACTION_RUN;
    }
    else
    {
        RULE_LOG("ruleInCaseDisconnectHandset, ignore as not in case or handset not connected");
        return RULE_ACTION_IGNORE;
    }
}

/*! @brief Rule to determine if Earbud should disconnect A2DP/AVRCP from peer when put in case
    Rule is triggered by the 'in case' event
    @startuml

    start
    if (IsInCase() and IsPeerA2dpConnected() and IsPeerAvrcpConnectedForAv()) then (yes)
        :Disconnect from peer as now in case;
        stop
    endif
    end
    @enduml
*/
static ruleAction ruleInCaseDisconnectPeer(void)
{
    if (appSmIsInCase() && (appDeviceIsPeerA2dpConnected() || appDeviceIsPeerAvrcpConnectedForAv()))
    {
        RULE_LOG("ruleInCaseDisconnectPeer, run as in case and peer connected");
        return RULE_ACTION_RUN;
    }
    else
    {
        RULE_LOG("ruleInCaseDisconnectPeer, ignore as not in case or peer not connected");
        return RULE_ACTION_IGNORE;
    }
}

/*! @brief Rule to determine if Earbud should start DFU  when put in case
    Rule is triggered by the 'in case' event
    @startuml

    start
    if (IsInCase() and DfuUpgradePending()) then (yes)
        :DFU upgrade can start as it was pending and now in case;
        stop
    endif
    end
    @enduml
*/
static ruleAction ruleInCaseEnterDfu(void)
{
#ifdef INCLUDE_DFU
    if (appSmIsInCase() && appUpgradeDfuPending())
    {
        RULE_LOG("ruleInCaseCheckDfu, run as still in case & DFU pending/active");
        return RULE_ACTION_RUN;
    }
    else
    {
        RULE_LOG("ruleInCaseCheckDfu, ignore as not in case or no DFU pending");
        return RULE_ACTION_IGNORE;
    }
#else
    return RULE_ACTION_IGNORE;
#endif
}


static ruleAction ruleDfuAllowHandsetConnect(void)
{
#ifdef INCLUDE_DFU
    bdaddr handset_addr;

    /* If we're already connected to handset then don't connect */
    if (appDeviceIsHandsetConnected())
    {
        RULE_LOG("ruleDfuAllowHandsetConnect, ignore as already connected to handset");
        return RULE_ACTION_IGNORE;
    }

    RULE_LOG("ruleDfuAllowHandsetConnect - just run it");
    return RULE_ACTION_RUN;

#else
    return RULE_ACTION_IGNORE;
#endif
}

/*! @brief Rule to determine if Earbud should disconnect A2DP/AVRCP from peer Earbud
    @startuml

    start
    if (Not IsPeerA2dpConnected() and Not IsPeerAvrcoConnectedForAv()) then (yes)
        :Not connected, do nothing;
        stop
    endif

    if (Not IsHandsetPaired()) then (yes)
        :Not paired with handset, disconnect from peer;
        stop
    endif

    if (IsHandsetA2dpConnected()) then (yes)
        if (IsTwsPlusHandset()) then (yes)
            :Connected to TWS+ handset, no need for A2DP/AVCRP to peer;
            stop
        else
            :Connected to standard handset, still require A2DP/AVRCP to peer;
            end
        endif
    else
        :Not connected with A2DP to handset;
        end
    endif    
    @enduml
*/
static ruleAction ruleDisconnectPeer(void)
{
    bdaddr handset_addr;

    /* Don't run rule if we're not connected to peer */
    if (!appDeviceIsPeerA2dpConnected() && !appDeviceIsPeerAvrcpConnectedForAv())
    {
        RULE_LOG("ruleSyncDisconnectPeer, ignore as not connected to peer");
        return RULE_ACTION_IGNORE;
    }

    /* If we're not paired with handset then disconnect */
    if (!appDeviceGetHandsetBdAddr(&handset_addr))
    {
        RULE_LOG("ruleSyncDisconnectPeer, run and not paired with handset");
        return RULE_ACTION_IGNORE;
    }

    /* If we're connected to a handset, but it's a TWS+ handset then we don't need connction to peer */
    if (appDeviceIsHandsetA2dpConnected())
    {
        if (appDeviceIsTwsPlusHandset(&handset_addr))
        {
            RULE_LOG("ruleSyncDisconnectPeer, run as connected to TWS+ handset");
            return RULE_ACTION_RUN;
        }
        else
        {
            RULE_LOG("ruleSyncDisconnectPeer, ignore as connected to standard handset");
            return RULE_ACTION_IGNORE;
        }
    }
    else
    {
        RULE_LOG("ruleSyncDisconnectPeer, run as not connected handset");
        return RULE_ACTION_RUN;
    }
}

static ruleAction ruleOutOfCaseAllowHandsetConnect(void)
{
    RULE_LOG("ruleOutOfCaseAllowHandsetConnect, run as out of case");
    return RULE_ACTION_RUN;
}

static ruleAction ruleInCaseRejectHandsetConnect(void)
{
#ifdef INCLUDE_DFU
    if (appUpgradeDfuPending())
    {
        RULE_LOG("ruleInCaseRejectHandsetConnect, ignored as DFU pending");
        return RULE_ACTION_IGNORE;
    }
#endif

    RULE_LOG("ruleInCaseRejectHandsetConnect, run as in case and no DFU");
    return RULE_ACTION_RUN;
}

/*! @brief Rule to determine if Earbud should start timer to disconnect from
           handset due to being out of the case and ear, and no audio active.

    @startuml
    start
        if (IsInCase() or IsInEar()) then (yes)
            :Earbud is not out of case and ear, do nothing;
            end
        endif
        if (IsHandsetConnected()) then (yes)
            :Earbud is not connected to handset, do nothing;
            end
        endif
        if (IsAvStreaming()) then (yes)
            :Earbud is still streaming A2DP, do nothing;
            end
        endif
        if (IsScoActive()) then (yes)
            :Earbud is still in active call, do nothing;
            end
        else (no)
            :Earbud out of case and ear, and idle, start handset disconnect timer;
            stop
        endif
    @enduml
*/
static ruleAction ruleIdleHandsetDisconnect(void)
{
    if (appSmIsInCase())
    {
        RULE_LOG("ruleIdleHandsetDisconnect, ignore earbud is in the case");
        return RULE_ACTION_IGNORE;
    }
    if (appSmIsInEar())
    {
        RULE_LOG("ruleIdleHandsetDisconnect, ignore earbud is in the ear");
        return RULE_ACTION_IGNORE;
    }
    if (!appDeviceIsHandsetA2dpConnected())
    {
        RULE_LOG("ruleIdleHandsetDisconnect, ignore earbud is not connected to handset");
        return RULE_ACTION_IGNORE;
    }
    if (appAvIsStreaming() || appHfpIsScoActive())
    {
        RULE_LOG("ruleIdleHandsetDisconnect, ignore earbud has active audio");
        return RULE_ACTION_IGNORE;
    }

    RULE_LOG("ruleIdleHandsetDisconnect, run as out of case and ear and idle");
    return RULE_ACTION_RUN;
}

/*! @brief Rule to determine if page scan settings should be changed.

    @startuml
        (handset1)
        (handset2)
        (earbud1)
        (earbud2)
        earbud1 <-> earbud2 : A
        earbud1 <--> handset1 : B
        earbud2 <--> handset2 : C
    @enduml
    A = link between earbuds
    B = link from earbud1 to handset1
    C = link from earbud2 to handset2
    D = earbud1 handset is TWS+
    E = earbud2 handset is TWS+
    Links B and C are mutually exclusive.

    Page scan is controlled as defined in the following truth table (X=Don't care).
    Viewed from the perspective of Earbud1.

    A | B | C | D | E | Page Scan On
    - | - | - | - | - | ------------
    0 | X | X | X | X | 1
    1 | 0 | 0 | X | X | 1
    1 | 0 | 1 | X | 1 | 1
    1 | 0 | 1 | X | 0 | 0
    1 | 1 | X | X | X | 0
*/
static ruleAction rulePageScanUpdate(void)
{
    bool connected_peer = appDeviceIsPeerConnected();
    bool connected_handset = appDeviceIsHandsetHfpConnected() ||
                             appDeviceIsHandsetA2dpConnected() ||
                             appDeviceIsHandsetAvrcpConnected();
    bool peer_connected_handset = appSmIsPeerHandsetA2dpConnected() ||
                                  appSmIsPeerHandsetAvrcpConnected() ||
                                  appSmIsPeerHandsetHfpConnected();

    /* Logic derived from the truth table above */
    bool ps_on = !connected_peer ||
                (!connected_handset && !peer_connected_handset) ||
                (!connected_handset && peer_connected_handset && appSmIsPeerHandsetTws());

    RULE_LOGF("rulePageScanUpdate, Peer=%u Handset=%u PeerHandset=%u, PS=%u",
               connected_peer, connected_handset, peer_connected_handset, ps_on);

    if (ps_on && !appScanManagerIsPageScanEnabledForUser(SCAN_MAN_USER_SM))
    {
        /* need to enable page scan and it is not already enabled for the SM user */
        RULE_LOG("rulePageScanUpdate, run, need to enable page scan");

        /* using CONN_RULES_PAGE_SCAN_UPDATE message which take a bool parameter,
         * use RULE_ACTION_RUN_PARAM macro to prime the message data and indicate
         * to the rules engine it should return it in the message to the client task */
        return RULE_ACTION_RUN_PARAM(ps_on);
    }
    else if (!ps_on && appScanManagerIsPageScanEnabledForUser(SCAN_MAN_USER_SM))
    {
        /* need to disable page scan and it is currently enabled for SM user */
        RULE_LOG("rulePageScanUpdate, run, need to disable page scan");
        return RULE_ACTION_RUN_PARAM(ps_on);
    }

    return RULE_ACTION_IGNORE;
}


static ruleAction ruleBatteryLevelChangeUnhandled(void)
{
    RULE_LOG("ruleBatteryLevelChangeUnhandled, ignore after battery state change");
    return RULE_ACTION_IGNORE;
}

static ruleAction ruleCheckChargerBatteryState(void)
{
    if (   battery_level_too_low == appBatteryGetState()
        && !appChargerIsConnected())
    {
        RULE_LOG("ruleBatteryLevelChangeShutdown, run, battery state low - no charger");
        return RULE_ACTION_RUN;
    }
    RULE_LOG("ruleBatteryLevelChangeShutdown, ignored");
    return RULE_ACTION_IGNORE;
}


/*****************************************************************************
 * END RULES FUNCTIONS
 *****************************************************************************/

static void appConRulesSetRuleStatus(MessageId message, ruleStatus status, ruleStatus new_status, connRulesEvents event)
{
    int rule_index;
    int num_rules = sizeof(appConnRules) / sizeof(ruleEntry);
    connRulesEvents event_mask = 0;

    for (rule_index = 0; rule_index < num_rules; rule_index++)
    {
        ruleEntry *rule = &appConnRules[rule_index];
        if ((rule->message == message) && (rule->status == status) && (rule->events & event))
        {
            CONNRULES_LOGF("appConnRulesSetStatus, rule %d, status %d", rule_index, new_status);
            SET_RULE_STATUS(rule, new_status);

            /* Build up set of events where rules are complete */
            event_mask |= rule->events;
        }
    }

    /* Check if all rules for an event are now complete, if so clear event */
    for (rule_index = 0; rule_index < num_rules; rule_index++)
    {
        ruleEntry *rule = &appConnRules[rule_index];
        if (rule->events & event)
        {
            /* Clear event if this rule is not complete */
            if (rule->status != RULE_STATUS_COMPLETE)
                event_mask &= ~rule->events;
        }
    }

    if (event_mask)
    {
        CONNRULES_LOGF("appConnRulesSetStatus, event %08lx%08lx complete", PRINT_ULL(event_mask));
        appConnRulesResetEvent(event_mask);
    }
}

static void appConnRulesCheck(void)
{
    connRulesTaskData *conn_rules = appGetConnRules();
    int rule_index;
    int num_rules = sizeof(appConnRules) / sizeof(ruleEntry);
    connRulesEvents events = conn_rules->events;
    uint32 rules_total_start_time = VmGetClock();
    uint32 rules_total_end_time = 0;

    CONNRULES_LOGF("appConnRulesCheck, starting events %08lx%08lx", PRINT_ULL(events));

    for (rule_index = 0; rule_index < num_rules; rule_index++)
    {
        ruleEntry *rule = &appConnRules[rule_index];
        ruleAction action;
        uint32 rule_start_time, rule_end_time = 0;

        /* On check rules that match event */
        if ((rule->events & events) == rule->events ||
             rule->flags == RULE_FLAG_ALWAYS_EVALUATE)
        {
            /* Skip rules that are now complete */
            if (rule->status == RULE_STATUS_COMPLETE)
                continue;

            /* Stop checking rules for this event if rule is in progress */
            if (rule->status == RULE_STATUS_IN_PROGRESS)
            {
                events &= ~rule->events;
                CONNRULES_LOGF("appConnRulesCheck, in progress, filtered events %08lx%08lx", PRINT_ULL(events));
                continue;
            }

            /* Call the rule */
            CONNRULES_LOGF("appConnRulesCheck, running rule %d, status %d, events %08lx%08lx",
                                                    rule_index, rule->status, PRINT_ULL(events));
            rule_start_time = VmGetClock();
            action = rule->rule();
            rule_end_time = VmGetClock();
            /* ignoring clock wrap at the moment */
            CONNRULES_TIMING_LOGF("appConnRulesCheck timing rule %d took %u ms",
                                   rule_index, rule_end_time-rule_start_time);

            /* handle result of the rule */
            if ((action == RULE_ACTION_RUN) ||
                (action == RULE_ACTION_RUN_WITH_PARAM))
            {
                TaskList *rule_tasks = appTaskListInit();
                TaskListData data = {0};
                Task task = 0;

                CONNRULES_LOG("appConnRulesCheck, rule in progress");

                /* mark rule as in progress, but not if this is an always
                 * evaluate rule */
                if (rule->flags != RULE_FLAG_ALWAYS_EVALUATE)
                {
                    SET_RULE_STATUS(rule, RULE_STATUS_IN_PROGRESS);
                }

                /*  Build list of tasks to send message to */
                while (appTaskListIterateWithData(conn_rules->event_tasks, &task, &data))
                {
                    if ((data.u64 & rule->events) == rule->events)
                    {
                        appTaskListAddTask(rule_tasks, task);
                    }
                }

                /* Send rule message to tasks in list. */
                if (action == RULE_ACTION_RUN)
                {
                    PanicFalse(conn_rules->size_rule_message_data == 0);
                    PanicFalse(conn_rules->rule_message_data == NULL);

                    /* for no parameters just send the message with id */
                    appTaskListMessageSendId(rule_tasks, rule->message);
                }
                else if (action == RULE_ACTION_RUN_WITH_PARAM)
                {
                    PanicFalse(conn_rules->size_rule_message_data != 0);
                    PanicFalse(conn_rules->rule_message_data != NULL);

                    /* for rules with parameters, use the message data that
                     * the rule will have placed in conn_rules already */
                    appTaskListMessageSendWithSize(rule_tasks, rule->message,
                                                   conn_rules->rule_message_data,
                                                   conn_rules->size_rule_message_data);

                    /* do not need to free rule_message_data, it has been
                     * used in the message system and will be freed once
                     * automatically once delivered, just clean up local
                     * references */
                    conn_rules->rule_message_data = NULL;
                    conn_rules->size_rule_message_data = 0;
                }
                appTaskListDestroy(rule_tasks);

                /* Stop checking rules for this event
                 * we only want to continue processing rules for this event after
                 * it has been marked as completed */
                if (rule->flags != RULE_FLAG_ALWAYS_EVALUATE)
                    events &= ~rule->events;
                continue;
            }
            else if (action == RULE_ACTION_COMPLETE)
            {
                CONNRULES_LOG("appConnRulesCheck, rule complete");
                if (rule->flags != RULE_FLAG_ALWAYS_EVALUATE)
                    appConRulesSetRuleStatus(rule->message, rule->status, RULE_STATUS_COMPLETE, rule->events);
            }
            else if (action == RULE_ACTION_IGNORE)
            {
                CONNRULES_LOG("appConnRulesCheck, rule ignored");
                if (rule->flags != RULE_FLAG_ALWAYS_EVALUATE)
                    appConRulesSetRuleStatus(rule->message, rule->status, RULE_STATUS_COMPLETE, rule->events);
            }
            else if (action == RULE_ACTION_DEFER)
            {
                CONNRULES_LOG("appConnRulesCheck, rule deferred");
                SET_RULE_STATUS(rule, RULE_STATUS_DEFERRED);
            }
        }
    }

    /* ignoring clock wrap for this simple debug tool, exceptions will be obvious */
    rules_total_end_time = VmGetClock();
    CONNRULES_TIMING_LOGF("appConnRulesCheck timing total run time %u",
                          rules_total_end_time - rules_total_start_time);
}



/*! \brief Initialise the connection rules module. */
void appConnRulesInit(void)
{
    connRulesTaskData *conn_rules = appGetConnRules();
    conn_rules->events = 0;
    conn_rules->event_tasks = appTaskListWithDataInit();

#ifdef ALLOW_CONNECT_AFTER_PAIRING
    conn_rules->allow_connect_after_pairing = TRUE;
#else
    conn_rules->allow_connect_after_pairing = FALSE;
#endif
}

void appConnRulesSetEvent(Task client_task, connRulesEvents event_mask)
{
    connRulesTaskData *conn_rules = appGetConnRules();
    TaskListData data = {0};

    conn_rules->events |= event_mask;
    CONNRULES_LOGF("appConnRulesSetEvent, new event %08lx%08lx, events %08lx%08lx", PRINT_ULL(event_mask), PRINT_ULL(conn_rules->events));

    if (appTaskListGetDataForTask(conn_rules->event_tasks, client_task, &data))
    {
        data.u64 |= event_mask;
        appTaskListSetDataForTask(conn_rules->event_tasks, client_task, &data);
    }
    else
    {
        data.u64 |= event_mask;
        appTaskListAddTaskWithData(conn_rules->event_tasks, client_task, &data);
    }

    appConnRulesCheck();
}

void appConnRulesResetEvent(connRulesEvents event)
{
    int rule_index;
    int num_rules = sizeof(appConnRules) / sizeof(ruleEntry);
    connRulesTaskData *conn_rules = appGetConnRules();
    TaskListData data = {0};
    Task iter_task = 0;

    conn_rules->events &= ~event;
    //CONNRULES_LOGF("appConnRulesResetEvent, new event %08lx%08lx, events %08lx%08lx", PRINT_ULL(event), PRINT_ULL(conn_rules->events));

    /* Walk through matching rules resetting the status */
    for (rule_index = 0; rule_index < num_rules; rule_index++)
    {
        ruleEntry *rule = &appConnRules[rule_index];

        if (rule->events & event)
        {
            //CONNRULES_LOGF("appConnRulesResetEvent, resetting rule %d", rule_index);
            SET_RULE_STATUS(rule, RULE_STATUS_NOT_DONE);
        }
    }

    /* delete the event from any tasks on the event_tasks list that is registered
     * for it. If a task has no remaining events, delete it from the list */
    while (appTaskListIterateWithData(conn_rules->event_tasks, &iter_task, &data))
    {
        if ((data.u64 & event) == event)
        {
            CONNRULES_LOGF("appConnRulesResetEvent, clearing event %08lx%08lx", PRINT_ULL(event));
            data.u64 &= ~event;
            if (data.u64)
            {
                appTaskListSetDataForTask(conn_rules->event_tasks, iter_task, &data);
            }
            else
            {
                appTaskListRemoveTask(conn_rules->event_tasks, iter_task);
            }
        }
    }
}

extern connRulesEvents appConnRulesGetEvents(void)
{
    connRulesTaskData *conn_rules = appGetConnRules();
    return conn_rules->events;
}

void appConnRulesSetRuleComplete(MessageId message)
{
    appConRulesSetRuleStatus(message, RULE_STATUS_IN_PROGRESS, RULE_STATUS_COMPLETE, RULE_EVENT_ALL_EVENTS_MASK);
    appConnRulesCheck();
}

void appConnRulesSetRuleWithEventComplete(MessageId message, connRulesEvents event)
{
    appConRulesSetRuleStatus(message, RULE_STATUS_IN_PROGRESS, RULE_STATUS_COMPLETE, event);
    appConnRulesCheck();
}

/*! \brief Copy rule param data for the engine to put into action messages.
    \param param Pointer to data to copy.
    \param size_param Size of the data in bytes.
    \return RULE_ACTION_RUN_WITH_PARAM to indicate the rule action message needs parameters.
 */
ruleAction appConnRulesCopyRunParams(void* param, size_t size_param)
{
    connRulesTaskData *conn_rules = appGetConnRules();
    conn_rules->rule_message_data = PanicUnlessMalloc(size_param);
    conn_rules->size_rule_message_data = size_param;
    memcpy(conn_rules->rule_message_data, param, size_param);
    return RULE_ACTION_RUN_WITH_PARAM;
}

/*! \brief Determine if there are still rules in progress. */
bool appConnRulesInProgress(void)
{
    int rule_index;
    int num_rules = sizeof(appConnRules) / sizeof(ruleEntry);
    bool rc = FALSE;

    for (rule_index = 0; rule_index < num_rules; rule_index++)
    {
        ruleEntry *rule = &appConnRules[rule_index];
        if ((rule->flags != RULE_FLAG_ALWAYS_EVALUATE) &&
            (rule->status == RULE_STATUS_IN_PROGRESS))
        {
            CONNRULES_LOGF("appConnRulesInProgress rule %u in progress", rule_index);
            rc = TRUE;
        }
    }
    return rc;
}
