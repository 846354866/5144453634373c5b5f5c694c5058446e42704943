/*!
\copyright  Copyright (c) 2008 - 2017 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       av_headset_power.c
\brief      Power Management
*/

#include <panic.h>
#include <connection.h>
#include <ps.h>
#include <boot.h>
#include <dormant.h>
#include <system_clock.h>
#include <rtime.h>

#include "av_headset.h"
#include "av_headset_log.h"
#include "av_headset_sm.h"
#include "av_headset_power.h"
#include "av_headset_ui.h"
#include "av_headset_charger.h"

/*! Internal messages */
enum power_internal_messages
{
                                            /*!  Power off timeout has expired */
    APP_POWER_INTERNAL_POWER_OFF_TIMEOUT = INTERNAL_MESSAGE_BASE, 
    APP_POWER_INTERNAL_POWER_OFF,           /*!< Power off now */ 
    APP_POWER_INTERNAL_AUTO_POWER_OFF,      /*!< Idle auto power off timer */
};

void appPowerOn(void)
{
    DEBUG_LOG("appPowerOn");
    
    /* Check if we are in limbo state (i.e logically powered off) */
    if (appSmIsSleepy())
    {
        /* Work our way back to operation */
        appSetState(APP_STATE_STARTUP);
    }

    /* Display power on LED pattern */
    appUiPowerOn();
}

void appPowerReboot(void)
{
    /* Reboot now */
    BootSetMode(BootGetMode());

    /* BootSetMode returns control on some devices, although should reboot 
       Wait here for 1 second and then Panic() */
    rtime_t start = SystemClockGetTimerTime();

    while (1)
    {
        rtime_t elapsed = rtime_sub(SystemClockGetTimerTime(),start);
        if (rtime_gt(elapsed,D_SEC(1)))
        {
            Panic();
        }
    }
}

void appPowerPerformanceProfileRequest(void)
{
    powerTaskData *thePower = appGetPowerControl();

    if (0 == thePower->performance_req_count)
    {
        VmRequestRunTimeProfile(VM_PERFORMANCE);
        DEBUG_LOG("appPowerPerformanceProfileRequest VM_PERFORMANCE");
    }
    thePower->performance_req_count++;
    /* Unsigned overflowed request count */
    PanicZero(thePower->performance_req_count);
}

void appPowerPerformanceProfileRelinquish(void)
{
    powerTaskData *thePower = appGetPowerControl();

    /* Unsigned underflow request count */
    PanicZero(thePower->performance_req_count);
    thePower->performance_req_count--;
    if (0 == thePower->performance_req_count)
    {
        VmRequestRunTimeProfile(VM_BALANCED);
        DEBUG_LOG("appPowerPerformanceProfileRelinquish VM_BALANCED");
    }
}


#ifdef INCLUDE_POWER_CONTROL

/*! \brief Query if power is going to dormant mode */
#define appPowerGoingToDormant() (appSmIsSleepy() && \
                                  APP_POWER_LOW_POWER_MODE_DORMANT \
                                      == appGetPowerControl()->powerdown_type_wanted)

void appPowerEnterSoporific(bool links_to_be_disconnected)
{
    if (links_to_be_disconnected)
    {
        /* waiting for link disconnect completion,
         * start a timer to force reset if we fail to complete for some reason */
        MessageSendLater(appGetPowerTask(), APP_POWER_INTERNAL_POWER_OFF_TIMEOUT, 
                            NULL, APP_POWER_OFF_FORCE_MS);
    }
    else
    {
        /* No links being disconnected, so shorter delay  */
        MessageSendLater(appGetPowerTask(), APP_POWER_INTERNAL_POWER_OFF_TIMEOUT, 
                            NULL, APP_POWER_OFF_WAIT_MS);
    }
}

void appPowerLinkStateUpdated(void)
{
    if (!appConManagerAnyLinkConnected() && appHfpIsDisconnected() && appAvIsDisconnected())
    {
        MessageSend(appGetPowerTask(), APP_POWER_INTERNAL_POWER_OFF, NULL);
    }
}

void appPowerEnterAsleep(void)
{
     MessageSendLater(appGetPowerTask(), APP_POWER_INTERNAL_POWER_OFF_TIMEOUT, NULL, APP_POWER_OFF_DORMANT_RETRY_MS);
}

void appPowerOffTimerRestart(void)
{
    powerTaskData *thePower = appGetPowerControl();

    DEBUG_LOGF("appPowerOffTimerRestart. Flags 0x%X",thePower->power_events_mask);

    MessageCancelFirst(appGetPowerTask(), APP_POWER_INTERNAL_AUTO_POWER_OFF);

    /* Check if any events are active */
    if (APP_POWER_EVENT_NONE == thePower->power_events_mask)
    {
        /* (Re)start timer if all logged activity ceases */
        MessageSendLater(appGetPowerTask(), APP_POWER_INTERNAL_AUTO_POWER_OFF, 0, D_SEC(APP_AUTO_POWER_OFF_TIMEOUT));
    }
}

void appPowerOffTimerDisable(appPowerEventMask event_mask)
{
    powerTaskData *thePower = appGetPowerControl();

    thePower->power_events_mask |= event_mask;

    appPowerOffTimerRestart();
}

void appPowerOffTimerEnable(appPowerEventMask event_mask)
{
    powerTaskData *thePower = appGetPowerControl();

    thePower->power_events_mask &= ~event_mask;

    appPowerOffTimerRestart();
}


/*! \brief Start power down operations

    This function is called to enter the shutdown states and notify the
    UI.
*/
static void appPowerControlStartPowerDownMode(appPowerPowerdownType mode)
{
    powerTaskData *thePower = appGetPowerControl();

    thePower->powerdown_type_wanted = mode;
    thePower->cancel_dormant = FALSE;

    /* Display power off LED pattern */
    appUiPowerOff();

    /* Initiate shutdown through the state machine */
    appSmPowerDownDisconnect();


}

void appPowerOff(void)
{
    DEBUG_LOG("appPowerOff");

    /* Only attempt to power off if we can */
    if (appChargerCanPowerOff())
    {
        appPowerControlStartPowerDownMode(APP_POWER_LOW_POWER_MODE_OFF);
    }
    else
    {
        DEBUG_LOG("Charger connected, blocking power off");
        appUiError();
    }
}

void appPowerHandleIdleTimer(void)
{
    powerTaskData *thePower = appGetPowerControl();

    DEBUG_LOG("appPowerHandleIdleTimer");

    /* Only attempt to power off if we are actually powered on (ie active) */
    if (!appSmIsActive())
    {
        DEBUG_LOGF("Idle PowerDown aborted as the application is not in an active state (state %d)",appGetState());
        return;
    }

    if (APP_POWER_LOW_POWER_MODE_NONE != thePower->powerdown_type_wanted)
    {
        /* We shouldn't be trying to enter dormant if something else
         * has requested a powerdown. */
        Panic();
    }

    if (   thePower->allow_powerdown_fn
        && !(*thePower->allow_powerdown_fn)())
    {
        DEBUG_LOGF("Idle PowerDown aborted as not allowed (state:%d, phystate:%d)",appGetState(),appPhyStateGetState());
        appPowerOffTimerRestart();
        return;
    }

    /* Although a simpler check, make sure it is last so tests have the 
       chance to check other behaviours */
    if (!thePower->allow_dormant)
    {
        DEBUG_LOG("DORMANT blocked by test command");
        return;
    }

    appPowerControlStartPowerDownMode(APP_POWER_LOW_POWER_MODE_DORMANT);
}

/*! \brief Enter dormant mode

    This function is called internally to enter dormant.

    \param extended_wakeup_events Allow accelerometer to wake from dormant 
*/
static void appPowerControlEnterDormantMode(bool extended_wakeup_events)
{
    DEBUG_LOG("appPowerControlEnterDormantMode");
    
#ifdef INCLUDE_ACCELEROMETER
    if (extended_wakeup_events)
    {
        powerTaskData *thePower = appGetPowerControl();
        dormant_config_key key;
        uint32 value;
        /* Register to ensure accelerometer is active */
        appAccelerometerClientRegister(&thePower->task);
        if (appAccelerometerGetDormantConfigureKeyValue(&key, &value))
        {
            /* Since there is only one dormant wake source (not including sys_ctrl),
               just apply the accelerometer's key and value */
            PanicFalse(DormantConfigure(key, value));
        }
        else
        {
            appAccelerometerClientUnregister(&thePower->task);
        }
    }
#else
    UNUSED(extended_wakeup_events);
#endif /* INCLUDE_ACCELEROMETER */

    appPhyStatePrepareToEnterDormant();

    /* An active charge module blocks dormant, regardless of whether
       it has power */
    appChargerForceDisable();

    /* Make sure dormant will ignore any wake up time */
    PanicFalse(DormantConfigure(DEADLINE_VALID,FALSE));

    /* Enter dormant */
    PanicFalse(DormantConfigure(DORMANT_ENABLE,TRUE));

    DEBUG_LOG("appPowerControlEnterDormantMode FAILED");

    /* If we happen to get here then Dormant didn't work,
     * so make sure the charger is running again (if needed)
     * so we could continue. */
    appChargerRestoreState();

    Panic();
}

/*! \brief Enter power off

    This function is called internally to enter the power off mode.
*/
static void appPowerControlDoPowerOff(void)
{
    /* No need to disable charger for power down, but if a charger is connected
       we will fail. This status should have been checked when the power off 
       command was received */
    PsuConfigure(PSU_ALL, PSU_ENABLE, FALSE);

    DEBUG_LOG("Turning off power supplies was ineffective?");

    /* Fall back to Dormant */
    appPowerControlEnterDormantMode(FALSE);

    Panic();
}


/*! \brief Power-off timeout expired

    This function is called when the power-off timeout has expired.
    It is responsible for calling appropriate functions to power off
    by either rebooting (DFU), entering dormant, or powering down.

    There is a small window during which something could have happened that
    has the effect of cancelling a transition to dormant. An example would 
    be movement of the earbud. This is handled here, in the timeout, to avoid
    any race conditions elsewhere.
*/
static void appPowerHandleInternalPowerOffTimeout(void)
{
    powerTaskData *thePower = appGetPowerControl();

    DEBUG_LOG("appPowerHandleInternalPowerOffTimeout");

    if (!appSmIsSleepy())
    {
        DEBUG_LOGF("appPowerHandleInternalPowerOffTimeout called when not in a sleep state. State:%d",appGetState());
        Panic();
    }

    if (APP_POWER_LOW_POWER_MODE_DORMANT == thePower->powerdown_type_wanted)
    {
        if (!thePower->cancel_dormant)
        {
            appPowerControlEnterDormantMode(TRUE);
        }
        else
        {
            appPowerCancelInProgressDormant();
            appSetState(APP_STATE_STARTUP);
        }
    }
    else if (APP_POWER_LOW_POWER_MODE_OFF == thePower->powerdown_type_wanted)
    {
        appPowerControlDoPowerOff();
    }
}

void appPowerRegisterPowerDownCheck(allowPowerDownFunc pd_func)
{
    powerTaskData *thePower = appGetPowerControl();

    thePower->allow_powerdown_fn = pd_func;
}

void appPowerCancelInProgressDormant(void)
{
    powerTaskData *thePower = appGetPowerControl();

    MessageCancelFirst(appGetPowerTask(), APP_POWER_INTERNAL_POWER_OFF_TIMEOUT);
    thePower->powerdown_type_wanted = APP_POWER_LOW_POWER_MODE_NONE;
    thePower->cancel_dormant = FALSE;

    appPowerOffTimerRestart();
}


/*! \brief Handle a change in the earbuds physcal state 

    We only need to action this if we were in the process of going to 
    sleep (dormant) and the state indicates that we should awake.

    In other cases we treat this as evidence that the earbud was active
    and just restart the power off timer.
*/
static void appPowerHandlePhyStateChanged(const PHY_STATE_CHANGED_IND_T *message)
{
    if (appPowerGoingToDormant())
    {
        if (   PHY_STATE_OUT_OF_EAR == message->new_state
            || PHY_STATE_IN_EAR == message->new_state)
        {
            appGetPowerControl()->cancel_dormant = TRUE;
        }
    }
    /* The OUT_OF_EAR_AT_REST state indicates that movement has stopped. This is
       normally just the end of transition from ear to table, so nothing new 
       requiring idle timer to be restarted. */
    else if (PHY_STATE_OUT_OF_EAR_AT_REST != message->new_state)
    {
        appPowerOffTimerRestart();
    }
}

/*! \brief Handle the charger state changing

    We only need to action this if we were in the process of going to 
    sleep (dormant) as we should now postpone.

    In other cases restart the power off timer as, with a charger, we
    won't be sleeping for a while.
*/
static void appPowerHandleChargerStateChanged(void)
{
    connRulesEvents event;

    DEBUG_LOG("appPowerHandleChargerStateChanged");

    if (appChargerIsConnected())
    {
        event = RULES_EVENT_CHARGER_CONNECTED;

        if (appPowerGoingToDormant())
        {
            appGetPowerControl()->cancel_dormant = TRUE;
        }
        else
        {
            appPowerOffTimerRestart();
        }
    }
    else
    {
        event = RULES_EVENT_CHARGER_DISCONNECTED;
    }

    appConnRulesSetEvent(appGetPowerTask(), event);

}

static void appPowerHandleBatteryLevelUpdate(const MESSAGE_BATTERY_LEVEL_UPDATE_STATE_T *battery_state)
{
    DEBUG_LOGF("appSmHandleBatteryLevelUpdate. State %d",battery_state->state);

    connRulesEvents event;

    switch (battery_state->state)
    {
        case battery_level_too_low:
            event = RULES_EVENT_BATTERY_TOO_LOW;
            break;

        case battery_level_critical:
            event = RULES_EVENT_BATTERY_CRITICAL;
            break;

        case battery_level_low:
            event = RULES_EVENT_BATTERY_LOW;
            break;

        case battery_level_ok:
            event = RULES_EVENT_BATTERY_OK;
            break;

        default:
            return;
    }

    appConnRulesSetEvent(appGetPowerTask(), event);
}

/*! \brief Battery level has dropped. Make sure all activity is 
    stopped and turn off. 

    If the charger is connected, then we can't power off.
 */
static void appPowerHandleConnRulesBatteryForcedShutdown(void)
{
    DEBUG_LOG("appPowerHandleConnRulesBatteryForcedShutdown");

    if (appChargerCanPowerOff())
    {
        appPowerControlStartPowerDownMode(APP_POWER_LOW_POWER_MODE_OFF);
    }

    appConnRulesSetRuleComplete(CONN_RULES_LOW_POWER_SHUTDOWN);
}


/*! @brief Power control message handler
 */
static void appPowerHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
        /* ---- Internal messages ---- */
        case APP_POWER_INTERNAL_POWER_OFF_TIMEOUT:
        case APP_POWER_INTERNAL_POWER_OFF:
            appPowerHandleInternalPowerOffTimeout();
            return;
            
        case APP_POWER_INTERNAL_AUTO_POWER_OFF:
            appPowerHandleIdleTimer();
            return;

        /* ---- Physical State messages ---- */
        case PHY_STATE_CHANGED_IND:
            appPowerHandlePhyStateChanged((const PHY_STATE_CHANGED_IND_T *)message);
            break;

        /* ---- Charger messages ---- */
        case CHARGER_MESSAGE_ATTACHED:
            appPowerHandleChargerStateChanged();
            break;

        case CHARGER_MESSAGE_DETACHED:
            appPowerHandleChargerStateChanged();
            break;

        /* ---- Battery messages ---- */
        case MESSAGE_BATTERY_LEVEL_UPDATE_STATE:
            appPowerHandleBatteryLevelUpdate((const MESSAGE_BATTERY_LEVEL_UPDATE_STATE_T *)message);
            break;

        /* ---- Connection rule messages ---- */
        case CONN_RULES_LOW_POWER_SHUTDOWN:
            appPowerHandleConnRulesBatteryForcedShutdown();
            break;
    }
}

void appPowerControlInit(void)
{
    batteryRegistrationForm batteryMonitoringForm;
    powerTaskData *thePower = appGetPowerControl();

    memset(thePower, 0, sizeof(*thePower));

    thePower->task.handler = appPowerHandleMessage;
    thePower->allow_dormant = TRUE;
    thePower->cancel_dormant = FALSE;
    thePower->performance_req_count = 0;
    VmRequestRunTimeProfile(VM_BALANCED);

    appPhyStateRegisterClient(appGetPowerTask());

    appChargerClientRegister(appGetPowerTask());

    batteryMonitoringForm.task = appGetPowerTask();
    batteryMonitoringForm.representation = battery_level_repres_state;
    batteryMonitoringForm.hysteresis = appConfigSmBatteryHysteresisMargin();
    appBatteryRegister(&batteryMonitoringForm);


}
#endif /* INCLUDE_POWER_CONTROL */

