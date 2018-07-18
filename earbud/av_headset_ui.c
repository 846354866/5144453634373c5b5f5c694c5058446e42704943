/*!
\copyright  Copyright (c) 2008 - 2017 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       av_headset_ui.c
\brief      Application User Interface
*/

#include <panic.h>
#include <ps.h>
#include <audio.h>
#include <boot.h>
#include <input_event_manager.h>

#include "av_headset.h"
#include "av_headset_ui.h"
#include "av_headset_sm.h"
#include "av_headset_hfp.h"
#include "av_headset_power.h"
#include "av_headset_log.h"

/*! Include the correct button header based on the number of buttons available to the UI */
#if defined(HAVE_9_BUTTONS)
#include "9_buttons.h"
#elif defined(HAVE_6_BUTTONS)
#include "6_buttons.h"
#elif defined(HAVE_1_BUTTON)
#include "1_button.h"
#else
#error "No buttons define found"
#endif


/*! At the end of every tone, add a short rest to make sure tone mxing in the DSP doens't truncate the tone */
#define RINGTONE_STOP  RINGTONE_NOTE(REST, HEMIDEMISEMIQUAVER), RINGTONE_END

/*!@{ \name Definition of LEDs, and basic colour combinations

    The basic handling for LEDs is similar, whether there are
    3 separate LEDs, a tri-color LED, or just a single LED.
 */

#if (appConfigNumberOfLeds() == 3)
#define LED_0_STATE  (1 << 0)
#define LED_1_STATE  (1 << 1)
#define LED_2_STATE  (1 << 2)
#elif (appConfigNumberOfLeds() == 2)
/* We only have 2 LED so map all control to the same LED */
#define LED_0_STATE  (1 << 0)
#define LED_1_STATE  (1 << 1)
#define LED_2_STATE  (1 << 1)
#else
/* We only have 1 LED so map all control to the same LED */
#define LED_0_STATE  (1 << 0)
#define LED_1_STATE  (1 << 0)
#define LED_2_STATE  (1 << 0)
#endif

#define LED_RED     (LED_0_STATE)
#define LED_GREEN   (LED_1_STATE)
#define LED_BLUE    (LED_2_STATE)
#define LED_WHITE   (LED_0_STATE | LED_1_STATE | LED_2_STATE)
#define LED_YELLOW  (LED_RED | LED_GREEN)

/*!@} */

#ifdef APP_TWS_T08
    #define APP_BUTTON_FLAG                 1015
	static volatile uint8 app_button_flag = 0;
#endif

/*! \brief An LED filter used for battery low

    \param led_state    State of LEDs prior to filter

    \returns The new, filtered, state
*/
uint16 app_led_filter_battery_low(uint16 led_state)
{
    return (led_state) ? LED_RED : 0;
}

/*! \brief An LED filter used for low charging level

    \param led_state    State of LEDs prior to filter

    \returns The new, filtered, state
*/
uint16 app_led_filter_charging_low(uint16 led_state)
{
    UNUSED(led_state);
    return LED_RED;
}

/*! \brief An LED filter used for charging level OK

    \param led_state    State of LEDs prior to filter

    \returns The new, filtered, state
*/
uint16 app_led_filter_charging_ok(uint16 led_state)
{
    UNUSED(led_state);
    return LED_YELLOW;
}

/*! \brief An LED filter used for charging complete 

    \param led_state    State of LEDs prior to filter

    \returns The new, filtered, state
*/
uint16 app_led_filter_charging_complete(uint16 led_state)
{
    UNUSED(led_state);
    return LED_GREEN;
}

/*! \cond led_patterns_well_named
    No need to document these. The public interface is
    from public functions such as appUiPowerOn()
 */

const ledPattern app_led_pattern_power_on[] = 
{
    LED_LOCK,
    LED_ON(LED_RED),    LED_WAIT(100),
    LED_ON(LED_GREEN),  LED_WAIT(100),
    LED_ON(LED_BLUE),   LED_WAIT(100),
    LED_OFF(LED_RED),   LED_WAIT(100),
    LED_OFF(LED_GREEN), LED_WAIT(100),
    LED_OFF(LED_BLUE),  LED_WAIT(100),
    LED_UNLOCK,
    LED_END
};

const ledPattern app_led_pattern_power_off[] = 
{
    LED_LOCK,
    LED_ON(LED_WHITE), LED_WAIT(100), LED_OFF(LED_WHITE), LED_WAIT(100),
    LED_REPEAT(1, 2),
    LED_UNLOCK,
    LED_END
};

const ledPattern app_led_pattern_error[] = 
{
    LED_LOCK,
    LED_ON(LED_RED), LED_WAIT(100), LED_OFF(LED_RED), LED_WAIT(100),
    LED_REPEAT(1, 2),
    LED_UNLOCK,
    LED_END
};

const ledPattern app_led_pattern_idle[] = 
{
    LED_SYNC(2000),
    LED_LOCK,
    LED_ON(LED_GREEN), LED_WAIT(100), LED_OFF(LED_GREEN),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
};

const ledPattern app_led_pattern_idle_connected[] = 
{
    LED_SYNC(1000),
    LED_LOCK,
    LED_ON(LED_GREEN), LED_WAIT(100), LED_OFF(LED_GREEN),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
};

const ledPattern app_led_pattern_pairing[] = 
{
    LED_LOCK,
    LED_ON(LED_BLUE), LED_WAIT(100), LED_OFF(LED_BLUE), LED_WAIT(100),
    LED_UNLOCK,
    LED_REPEAT(0, 0)
};

const ledPattern app_led_pattern_pairing_deleted[] = 
{
    LED_LOCK,
    LED_ON(LED_YELLOW), LED_WAIT(100), LED_OFF(LED_YELLOW), LED_WAIT(100),
    LED_REPEAT(1, 2),
    LED_UNLOCK,
    LED_END
};

const ledPattern app_led_pattern_peer_pairing[] =
{
    LED_LOCK,
    LED_ON(LED_BLUE), LED_WAIT(50), LED_OFF(LED_BLUE), LED_WAIT(50),
    LED_UNLOCK,
    LED_REPEAT(0, 0)
};

#ifdef INCLUDE_DFU
const ledPattern app_led_pattern_dfu[] = 
{
    LED_LOCK,
    LED_ON(LED_RED), LED_WAIT(100), LED_OFF(LED_RED), LED_WAIT(100),
    LED_REPEAT(1, 2),
    LED_WAIT(400),
    LED_UNLOCK,
    LED_REPEAT(0, 0)
};
#endif

#ifdef INCLUDE_AV
const ledPattern app_led_pattern_streaming[] =
{
    LED_SYNC(2000),
    LED_LOCK,
    LED_ON(LED_GREEN), LED_WAIT(50), LED_OFF(LED_GREEN), LED_WAIT(50),
    LED_REPEAT(2, 2),
    LED_WAIT(500),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
};
#endif

#ifdef INCLUDE_AV
const ledPattern app_led_pattern_streaming_aptx[] =
{
    LED_SYNC(2000),
    LED_LOCK,
    LED_ON(LED_BLUE), LED_WAIT(50), LED_OFF(LED_BLUE), LED_WAIT(50),
    LED_REPEAT(2, 2),
    LED_WAIT(500),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
};
#endif

const ledPattern app_led_pattern_sco[] = 
{
    LED_SYNC(2000),
    LED_LOCK,
    LED_ON(LED_GREEN), LED_WAIT(50), LED_OFF(LED_GREEN), LED_WAIT(50),
    LED_REPEAT(2, 1),
    LED_WAIT(500),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
};

const ledPattern app_led_pattern_call_incoming[] = 
{
    LED_LOCK,
    LED_SYNC(1000),
    LED_ON(LED_WHITE), LED_WAIT(50), LED_OFF(LED_WHITE), LED_WAIT(50),
    LED_REPEAT(2, 1),
    LED_UNLOCK,
    LED_REPEAT(0, 0),
};

const ledPattern app_led_pattern_battery_empty[] = 
{
    LED_LOCK,
    LED_ON(LED_RED),
    LED_REPEAT(1, 2),
    LED_UNLOCK,
    LED_END
};
/*! \endcond led_patterns_well_named
 */


/*! \cond constant_well_named_tones 
    No Need to document these tones. Their access through functions such as
    appUiIdleActive() is the public interface.
 */
 
const ringtone_note app_tone_button[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_button_2[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_button_3[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_button_4[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

#ifdef INCLUDE_DFU
const ringtone_note app_tone_button_dfu[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(A7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};
#endif

const ringtone_note app_tone_button_factory_reset[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(A7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(C7, SEMIQUAVER),
    RINGTONE_NOTE(B7, SEMIQUAVER),
    RINGTONE_STOP
};

#ifdef INCLUDE_AV
const ringtone_note app_tone_av_connect[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_av_disconnect[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_av_remote_control[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_av_connected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D6,  SEMIQUAVER),
    RINGTONE_NOTE(A6,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_av_disconnected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A6,  SEMIQUAVER),
    RINGTONE_NOTE(D6,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_av_link_loss[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_STOP
};
#endif

const ringtone_note app_tone_hfp_connect[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_connected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D6,  SEMIQUAVER),
    RINGTONE_NOTE(A6,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_disconnected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A6,  SEMIQUAVER),
    RINGTONE_NOTE(D6,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_link_loss[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_STOP
};
        
const ringtone_note app_tone_hfp_sco_connected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(AS5, DEMISEMIQUAVER),
    RINGTONE_NOTE(DS6, DEMISEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_sco_disconnected[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(DS6, DEMISEMIQUAVER),
    RINGTONE_NOTE(AS5, DEMISEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_mute_reminder[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_sco_unencrypted_reminder[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_ring[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_NOTE(REST, SEMIQUAVER),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_NOTE(D7,   SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_ring_caller_id[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B6,   SEMIQUAVER),
    RINGTONE_NOTE(G6,   SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_voice_dial[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_voice_dial_disable[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_answer[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_hangup[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_mute_active[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(CS7, SEMIQUAVER),
    RINGTONE_NOTE(DS7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_mute_inactive[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(DS7, SEMIQUAVER),
    RINGTONE_NOTE(CS7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_hfp_talk_long_press[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_pairing[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_paired[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A6, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_pairing_deleted[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(A6, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_volume[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_volume_limit[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_error[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_battery_empty[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B6, SEMIQUAVER),
    RINGTONE_NOTE(B6, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_power_on[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(CS5, SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_power_off[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(CS5, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_paging_reminder[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_peer_pairing[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(D7, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_peer_pairing_error[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_NOTE(B5, SEMIQUAVER),
    RINGTONE_STOP
};

const ringtone_note app_tone_peer_pairing_reminder[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_NOTE(A5,  SEMIQUAVER),
    RINGTONE_STOP
};

#ifdef INCLUDE_DFU
const ringtone_note app_tone_dfu[] =
{
    RINGTONE_TIMBRE(sine), RINGTONE_DECAY(16),
    RINGTONE_NOTE(CS5, SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_NOTE(D5,  SEMIQUAVER),
    RINGTONE_STOP
};
#endif

/*! \endcond constant_well_named_tones */

/*! \brief Play tone to completion.
    \param tone The tone to play.
*/
void appUiPlayTone(const ringtone_note *tone)
{
#ifdef INCLUDE_TONES
    /* Only play tone if application is active */
    if (appSmIsActive())
        appKymeraTonePlay(tone, FALSE);
#else
    UNUSED(tone);
#endif
}

/*! \brief Play tone allowing another tone/event to interrupt (stop) this tone
     before completion.
    \param tone The tone to play.
*/
void appUiPlayToneInterruptible(const ringtone_note *tone)
{
#ifdef INCLUDE_TONES
    /* Only play tone if application is active */
    if (appSmIsActive())
        appKymeraTonePlay(tone, TRUE);
#else
    UNUSED(tone);
#endif
}

/*! \brief Report a generic error on LEDs and play tone */
void appUiError(void)
{
    appUiPlayTone(app_tone_error);
    appLedSetPattern(app_led_pattern_error, LED_PRI_EVENT);
}

/*! \brief Play HFP error tone and set LED error pattern.
    \param silent If TRUE the error is not presented on the UI.
*/
void appUiHfpError(bool silent)
{
    if (!silent)
    {
        appUiPlayTone(app_tone_error);
        appLedSetPattern(app_led_pattern_error, LED_PRI_EVENT);
    }
}

/*! \brief Play AV error tone and set LED error pattern.
    \param silent If TRUE the error is not presented on the UI.
*/
void appUiAvError(bool silent)
{
    if (!silent)
    {
        appUiPlayTone(app_tone_error);
        appLedSetPattern(app_led_pattern_error, LED_PRI_EVENT);
    }
}

/*! \brief Play power on tone and LED pattern */
void appUiPowerOn(void)
{
    /* Enable LEDs */
    appLedEnable(TRUE);
    
    appLedSetPattern(app_led_pattern_power_on, LED_PRI_EVENT);
    appUiPlayTone(app_tone_power_on);
}

#ifdef INCLUDE_POWER_CONTROL
/*! \brief Play power off tone and LED pattern */
void appUiPowerOff(void)
{
    appLedSetPattern(app_led_pattern_power_off, LED_PRI_EVENT);
    appUiPlayTone(app_tone_power_off);
    
    /* Disable LEDs */
    appLedEnable(FALSE);
}
#endif

/*! \brief Message Handler

    This function is the main message handler for the UI module, all user button
    presses are handled by this function.

    NOTE - only a single button config is currently defined for both earbuds.
    The following defines could be used to split config amongst the buttons on
    two earbuds.

        APP_RIGHT_CONFIG
        APP_SINGLE_CONFIG
        APP_LEFT_CONFIG
*/    
static void appUiHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    appPowerOffTimerRestart();
    
    switch (id)
    {
        /* HFP call/reject & A2DP play/pause */
        case APP_MFB_BUTTON_PRESS:
        {
    	#ifdef APP_TWS_T08
            app_button_flag++;

			if (1 == app_button_flag)
			{
                MessageSendLater(appGetUiTask(), APP_BUTTON_FLAG, NULL, 500);
			}
		#else		
            DEBUG_LOG("APP_MFB_BUTTON_PRESS");
            if (appSmIsOutOfCase())
            {
                /* If voice call active, hangup */
                if (appHfpIsCallActive())
                    appHfpCallHangup();
                    /* Sco Forward can be streaming a ring tone */
                else if (appScoFwdIsReceiving() && !appScoFwdIsCallIncoming())
                    appScoFwdCallHangup();
                /* If outgoing voice call, hangup */
                else if (appHfpIsCallOutgoing())
                    appHfpCallHangup();
                /* If incoming voice call, accept */
                else if (appHfpIsCallIncoming())
                    appHfpCallAccept();
                else if (appScoFwdIsCallIncoming())
                    appScoFwdCallAccept();
                /* If AVRCP to handset connected, send play or pause */
                else if (appDeviceIsHandsetAvrcpConnected())
                    appAvPlayToggle(TRUE);
                /* If AVRCP is peer is connected and peer is connected to handset, send play or pause */
                else if (appDeviceIsPeerAvrcpConnectedForAv() && appSmIsPeerSyncComplete() && appSmIsPeerHandsetAvrcpConnected())
                    appAvPlayToggle(TRUE);
                else if (appDeviceIsHandsetHfpConnected() && appDeviceIsHandsetA2dpConnected())
                    appUiError();
                else
                    appSmConnectHandset();
            }
		#endif
        }
        break;

    #ifdef APP_TWS_T08
        case APP_BUTTON_FLAG:
        {
        	if (appSmIsOutOfCase())
            {
            	if (3 == app_button_flag)
            	{
            		UNUSED(0);
            	}
				else if (2 == app_button_flag)
				{
                	//appTestHandsetHfpVoiceDial();
            		if (appHfpIsConnected())
            		{
                		appHfpCallVoice();
            		}
				}
				else
				{
					/* If voice call active, hangup */
                	if (appHfpIsCallActive())
                    	appHfpCallHangup();
                    /* Sco Forward can be streaming a ring tone */
                	else if (appScoFwdIsReceiving() && !appScoFwdIsCallIncoming())
                    	appScoFwdCallHangup();
                	/* If outgoing voice call, hangup */
                	else if (appHfpIsCallOutgoing())
                    	appHfpCallHangup();
                	/* If incoming voice call, accept */
                	else if (appHfpIsCallIncoming())
                    	appHfpCallAccept();
                	else if (appScoFwdIsCallIncoming())
                    	appScoFwdCallAccept();
                	/* If AVRCP is peer is connected and peer is connected to handset, send play or pause */
                	else if (appDeviceIsPeerAvrcpConnectedForAv() && appSmIsPeerSyncComplete() && appSmIsPeerHandsetAvrcpConnected())
                    	appAvPlayToggle(TRUE);
                	/* If AVRCP to handset connected, send play or pause */
                	else if (appDeviceIsHandsetAvrcpConnected())
                    	appAvPlayToggle(TRUE);
                	else if (appDeviceIsHandsetHfpConnected() && appDeviceIsHandsetA2dpConnected())
                    	appUiError();
                	else
                    	appSmConnectHandset();
				}
        	}

			app_button_flag = 0;
        }
        break;
	#endif

        case APP_MFB_BUTTON_1_SECOND:
        {
            DEBUG_LOG("APP_MFB_BUTTON_1_SECOND");
            if (appSmIsOutOfCase())
            {
                if (appHfpIsCallActive())  /* Mic Mute not handled for SCO Fwding */
                {
                    if (appHfpIsScoActive())
                        appHfpTransferToAg();
                    else
                        appHfpTransferToHeadset();
                }
                else if (appHfpIsCallIncoming())
                    appHfpCallReject();
                else if (appScoFwdIsCallIncoming())
                    appScoFwdCallReject();
                else if (appAvHasAConnection())
				#ifdef APP_TWS_T08
					appAvForward();
				#else
                    appAvStop(TRUE);
				#endif
                else
                    appUiAvError(FALSE);
            }
        }
        break;

#ifdef APP_BUTTON_HELD_1
        case APP_BUTTON_HELD_1:
#endif
        case APP_MFB_BUTTON_HELD_1:
            DEBUG_LOG("APP_(MFB)_BUTTON_HELD_1");
            if (appSmIsOutOfCase())
                appUiButton();
            break;

        case APP_MFB_BUTTON_HELD_2:
            DEBUG_LOG("APP_MFB_BUTTON_HELD_2");
            if (appSmIsOutOfCase())
                appUiButton2();
            break;

        case APP_MFB_BUTTON_HELD_3:
            DEBUG_LOG("APP_MFB_BUTTON_HELD_3");
            if (appSmIsOutOfCase())
                appUiButton3();
            break;

#ifdef APP_MFB_BUTTON_HELD_4
        case APP_MFB_BUTTON_HELD_4:
            DEBUG_LOG("APP_MFB_BUTTON_HELD_4");
            if (appSmIsOutOfCase())
                appUiButton4();
            break;
#endif
        /* Handset Pairing */
        case APP_MFB_BUTTON_6_SECOND:
            DEBUG_LOG("APP_MFB_BUTTON_6_SECOND");
            if (appSmIsOutOfCase())
            {
                if (!appSmIsPairing())
                    appSmPairHandset();
                else
                {
                    // TODO: Cancel pairing
                }
            }
            break;

        /* Delete Handset Pairings */
        case APP_MFB_BUTTON_8_SECOND:
            DEBUG_LOG("APP_MFB_BUTTON_8_SECOND");
            if (appSmIsOutOfCase())
                appSmDeleteHandsets();
            break;

#if defined(HAVE_6_BUTTONS) || defined(HAVE_9_BUTTONS)

        /* Mute Volume */
        case APP_BUTTON_VOLUME_UP_DOWN:
        {
            DEBUG_LOG("APP_BUTTON_VOLUME_UP_DOWN");
            if (appSmIsOutOfCase())
            {
                /* Make sure any volume change repeat in progress is cancelled */
                if (appHfpIsScoActive())
                    appHfpVolumeStop(0);
#ifdef INCLUDE_AV
                else if (appAvIsStreaming())
                    appAvVolumeStop(0);
#endif
                else if (appHfpIsConnected())
                    appHfpVolumeStop(0);

                if (appHfpIsScoActive())
                    appHfpMuteToggle();
                else
                    appUiHfpError(FALSE);
            }
        }
        break;

        /* Volume Down */
        case APP_BUTTON_VOLUME_DOWN:
        {
            DEBUG_LOG("APP_BUTTON_VOLUME_DOWN");
            uint16 num_pending_msgs = MessageCancelAll(appGetUiTask(), APP_BUTTON_VOLUME_DOWN);

            if (appSmIsOutOfCase())
            {
                uint16 num_steps = num_pending_msgs + 1;
                int16 hfp_change = - (appConfigGetHfpVolumeStep() * num_steps);
                int16 av_change = - (appConfigGetAvVolumeStep() * num_steps);

                if (appHfpIsScoActive())
                {
                    appHfpVolumeStart(hfp_change);
                }
                else if (appScoFwdIsReceiving())
                {
                    appScoFwdVolumeDown(hfp_change);
                }
#ifdef INCLUDE_AV
                else if (appAvIsStreaming())
                {
                    appAvVolumeStart(av_change);
                }
#endif
                else if (appHfpIsConnected())
                {
                    appHfpVolumeStart(hfp_change);
                }
                else if (appScoFwdHasConnection())
                {
                    appScoFwdVolumeDown(hfp_change);
                }
                else
                {
                    appUiHfpError(FALSE);
                }
            }
        }
        break;

        /* Volume Up */
        case APP_BUTTON_VOLUME_UP:
        {
            DEBUG_LOG("APP_BUTTON_VOLUME_UP");
            uint16 num_pending_msgs = MessageCancelAll(appGetUiTask(), APP_BUTTON_VOLUME_UP);

            if (appSmIsOutOfCase())
            {
                uint16 num_steps = num_pending_msgs + 1;
                int16 hfp_change = appConfigGetHfpVolumeStep() * num_steps;
                int16 av_change = appConfigGetAvVolumeStep() * num_steps;

                if (appHfpIsScoActive())
                {
                    appHfpVolumeStart(hfp_change);
                }
                else if (appScoFwdIsReceiving())
                {
                    appScoFwdVolumeUp(hfp_change);
                }
#ifdef INCLUDE_AV
                else if (appAvIsStreaming())
                {
                    appAvVolumeStart(av_change);
                }
#endif
                else if (appHfpIsConnected())
                {
                    appHfpVolumeStart(hfp_change);
                }
                else if (appScoFwdHasConnection())
                {
                    appScoFwdVolumeUp(av_change);
                }
                else
                {
                    appUiHfpError(FALSE);
                }
            }
        }
        break;

        case APP_BUTTON_VOLUME_DOWN_RELEASE:
        {
            DEBUG_LOG("APP_BUTTON_VOLUME_DOWN_RELEASE");
            if (appHfpIsScoActive())
                appHfpVolumeStop(-appConfigGetHfpVolumeStep());
#ifdef INCLUDE_AV
            else if (appAvIsStreaming())
                appAvVolumeStop(-appConfigGetAvVolumeStep());
#endif
            else if (appHfpIsConnected())
                appHfpVolumeStop(-appConfigGetHfpVolumeStep());
        }
        break;

        case APP_BUTTON_VOLUME_UP_RELEASE:
        {
            DEBUG_LOG("APP_BUTTON_VOLUME_UP_RELEASE");
            if (appHfpIsScoActive())
                appHfpVolumeStop(appConfigGetHfpVolumeStep());
#ifdef INCLUDE_AV
            else if (appAvIsStreaming())
                appAvVolumeStop(appConfigGetAvVolumeStep());
#endif
            else if (appHfpIsConnected())
                appHfpVolumeStop(appConfigGetHfpVolumeStep());
        }
        break;

        case APP_BUTTON_FORWARD:
        {
            if (appSmIsOutOfCase())
                appAvForward();
        }
        break;

        case APP_BUTTON_BACKWARD:
        {
            if (appSmIsOutOfCase())
                appAvBackward();
        }
        break;

        case APP_BUTTON_FORWARD_HELD:
        {
            if (appSmIsOutOfCase())
                appAvFastForwardStart();
        }
        break;

        case APP_BUTTON_FORWARD_HELD_RELEASE:
        {
             appAvFastForwardStop();
        }
        break;

        case APP_BUTTON_BACKWARD_HELD:
        {
            if (appSmIsOutOfCase())
                appAvRewindStart();
        }
        break;

        case APP_BUTTON_BACKWARD_HELD_RELEASE:
        {
            appAvRewindStop();
        }
        break;

#endif /* HAVE_6_BUTTONS || HAVE_9_BUTTONS */

#ifdef INCLUDE_DFU
        case APP_BUTTON_DFU:
        {
            if (appSmIsOutOfCase() && appSmIsOutOfEar() && appUpgradeUiDfuRequest())
                appUiDfuRequested();
        }
        break;

        case APP_BUTTON_HELD_DFU:
        {
            if (appSmIsOutOfCase() && appSmIsOutOfEar())
                appUiButtonDfu();
        }
        break;

#endif /* INCLUDE_DFU */

#ifdef APP_TWS_T08
		case APP_MFB_BUTTON_3_SECOND:
			DEBUG_LOG("APP_MFB_BUTTON_3_SECOND");
            if (appSmIsOutOfCase())
            {
                if (!appSmIsPeerPairing())
               	{
                    //appSmPairPeer();
					appTestPairPeer();
                }
                else
                {
                	appTestPairPeer();
                    // TODO: Cancel pairing
                }
            }
            break;
#else
	#ifdef INCLUDE_POWER_CONTROL
        case APP_BUTTON_POWER_OFF:
            appPowerOff();
            break;
	#endif
#endif

        case APP_BUTTON_FACTORY_RESET:
        {
            if (appSmIsOutOfCase())
                appSmFactoryReset();
        }
        break;

        case APP_BUTTON_HELD_FACTORY_RESET:
        {
            if (appSmIsOutOfCase())
                appUiButtonFactoryReset();
        }
        break;
    }
}

/*! brief Initialise UI module */
void appUiInit(void)
{
    uiTaskData *theUi = appGetUi();
    
    /* Set up task handler */
    theUi->task.handler = appUiHandleMessage;
    
    /* Initialise input event manager with auto-generated tables for
     * the target platform */
    theUi->input_event_task = InputEventManagerInit(appGetUiTask(), InputEventActions,
                                                    sizeof(InputEventActions),
                                                    &InputEventConfig);
}
