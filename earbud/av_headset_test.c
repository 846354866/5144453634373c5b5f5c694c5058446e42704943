/*!
\copyright  Copyright (c) 2017 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       av_headset_test.c
\brief      Implementation of specifc application testing functions 
*/


#include "av_headset.h"
#include "av_headset_log.h"

#include <cryptovm.h>
#include <ps.h>
#include <boot.h>
#include <feature.h>

#ifdef INCLUDE_SCOFWD

static void testTaskHandler(Task task, MessageId id, Message message);
TaskData testTask = {testTaskHandler};

#endif /* INCLUDE_SCOFWD */


uint16 appTestBatteryVoltage = 0;

/*! \brief Returns the current battery voltage
 */
uint16 appTestGetBatteryVoltage(void)
{
    return appBatteryGetVoltage();
}

void appTestSetBatteryVoltage(uint16 new_level)
{
    appTestBatteryVoltage = new_level;
    MessageSend(&appGetBattery()->task, MESSAGE_BATTERY_PROCESS_READING, NULL);
}


/*! \brief Put Earbud into Handset Pairing mode
*/
void appTestPairHandset(void)
{
    DEBUG_LOG("appTestPairHandset");
    appSmPairHandset();
}

/*! \brief Delete all Handset pairing
*/
void appTestDeleteHandset(void)
{
    DEBUG_LOG("appTestDeleteHandset");
    appSmDeleteHandsets();
}

/*! \brief Put Earbud into Peer Pairing mode
*/
void appTestPairPeer(void)
{
    DEBUG_LOG("appTestPairPeer");
    appPairingPeerPair(NULL, FALSE);
}

/*! \brief Delete Earbud peer pairing
*/
bool appTestDeletePeer(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestDeletePeer");
    /* Check if we have previously paired with an earbud */
    if (appDeviceGetPeerBdAddr(&bd_addr))
    {
        return appDeviceDelete(&bd_addr);
    }
    else
    {
        DEBUG_LOG("appTestDeletePeer: NO PEER TO DELETE");
        return FALSE;
    }
}

bool appTestGetPeerAddr(bdaddr *peer_address)
{
    DEBUG_LOG("appTestGetPeerAddr");
    if (appDeviceGetPeerBdAddr(peer_address))
        return TRUE;
    else
        return FALSE;
}


/*! \brief Return if Earbud is in a Pairing mode
*/
bool appTestIsPairingInProgress(void)
{
    DEBUG_LOG("appTestIsPairingInProgress");
    return !appPairingIsIdle();
}

/*! \brief Initiate Earbud A2DP connection to the Handset
*/
bool appTestHandsetA2dpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestHandsetA2dpConnect");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        return appAvA2dpConnectRequest(&bd_addr, A2DP_CONNECT_NOFLAGS);
    }
    else
    {
        return FALSE;
    }
}

/*! \brief Return if Earbud has an Handset A2DP connection
*/
bool appTestIsHandsetA2dpConnected(void)
{
    bdaddr bd_addr;
    appDeviceAttributes attributes;
    int index = 0;

    DEBUG_LOG("appTestIsHandsetA2dpConnected");
    while (appDeviceGetHandsetAttributes(&bd_addr, &attributes, &index))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appA2dpIsConnected(theInst);
    }

    /* If we get here then there's no A2DP connected for handset */
    return FALSE;
}

bool appTestIsHandsetA2dpMediaConnected(void)
{
    bdaddr bd_addr;
    appDeviceAttributes attributes;
    int index = 0;

    DEBUG_LOG("appTestIsHandsetA2dpMediaConnected");
    while (appDeviceGetHandsetAttributes(&bd_addr, &attributes, &index))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appA2dpIsConnectedMedia(theInst);
    }

    /* If we get here then there's no A2DP connected for handset */
    return FALSE;
}

/*! \brief Return if Earbud is in A2DP streaming mode with the handset
*/
bool appTestIsHandsetA2dpStreaming(void)
{
    bdaddr bd_addr;
    appDeviceAttributes attributes;
    int index = 0;

    DEBUG_LOG("appTestIsHandsetA2dpStreaming");
    while (appDeviceGetHandsetAttributes(&bd_addr, &attributes, &index))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appA2dpIsStreaming(theInst);
    }

    /* If we get here then there's no A2DP connected for handset */
    return FALSE;
}

bool appTestIsA2dpPlaying(void)
{
    return appAvPlayStatus() == avrcp_play_status_playing;
}

/*! \brief Initiate Earbud AVRCP connection to the Handset
*/
bool appTestHandsetAvrcpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestHandsetAvrcpConnect");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
        return  appAvAvrcpConnectRequest(NULL, &bd_addr);
    else
        return FALSE;
}

/*! \brief Return if Earbud has an Handset AVRCP connection
*/
bool appTestIsHandsetAvrcpConnected(void)
{
    bdaddr bd_addr;
    appDeviceAttributes attributes;
    int index = 0;

    DEBUG_LOG("appTestIsHandsetAvrcpConnected");
    while (appDeviceGetHandsetAttributes(&bd_addr, &attributes, &index))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appAvrcpIsConnected(theInst);
    }

    /* If we get here then there's no AVRCP connected for handset */
    return FALSE;
}

/*! \brief Initiate Earbud HFP connection to the Handset
*/
bool appTestHandsetHfpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestHandsetHfpConnect");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
        return appHfpConnectWithBdAddr(&bd_addr, hfp_handsfree_107_profile);
    else
        return FALSE;
}

/*! \brief Return if Earbud has an Handset HFP connection
*/
bool appTestIsHandsetHfpConnected(void)
{
    DEBUG_LOG("appTestIsHandsetHfpConnected");
    return appHfpIsConnected();
}

/*! \brief Return if Earbud has an Handset HFP SCO connection
*/
bool appTestIsHandsetHfpScoActive(void)
{
    DEBUG_LOG("appTestIsHandsetHfpScoActive");
    return appHfpIsScoActive();
}

/*! \brief Initiate Earbud HFP Voice Dial request to the Handset
*/
bool appTestHandsetHfpVoiceDial(void)
{
    DEBUG_LOG("appTestHandsetHfpVoiceDial");
    if (appHfpIsConnected())
    {
        appHfpCallVoice();
        return TRUE;
    }
    else
        return FALSE;
}

/*! \brief Initiate Earbud HFP Voice Transfer request to the Handset
*/
bool appTestHandsetHfpVoiceTransfer(void)
{
    DEBUG_LOG("appTestHandsetHfpVoiceTransfer");
    if (appHfpIsCall())
    {
        if (appHfpIsScoActive())
            appHfpTransferToAg();
        else
            appHfpTransferToHeadset();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpMute(void)
{
    DEBUG_LOG("appTestHandsetHfpMute");
    if (appHfpIsCall())
    {
        if (!appHfpIsMuted())
            appHfpMuteToggle();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpUnMute(void)
{
    DEBUG_LOG("appTestHandsetHfpUnMute");
    if (appHfpIsCall())
    {
        if (appHfpIsMuted())
            appHfpMuteToggle();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpVoiceTransferToAg(void)
{
    DEBUG_LOG("appTestHandsetHfpVoiceTransferToAg");
    if (appHfpIsCall() && appHfpIsScoActive())
    {
        appHfpTransferToAg();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpVoiceTransferToHeadset(void)
{
    DEBUG_LOG("appTestHandsetHfpVoiceTransferToHeadset");
    if (appHfpIsCall())
    {
        appHfpTransferToHeadset();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpCallAccept(void)
{
    DEBUG_LOG("appTestHandsetHfpCallAccept");
    if (appHfpIsCall())
    {
        appHfpCallAccept();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpCallReject(void)
{
    DEBUG_LOG("appTestHandsetHfpCallReject");
    if (appHfpIsCall())
    {
        appHfpCallReject();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpCallHangup(void)
{
    DEBUG_LOG("appTestHandsetHfpCallHangup");
    if (appHfpIsCall())
    {
        appHfpCallHangup();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpCallLastDialed(void)
{
    DEBUG_LOG("appTestHandsetHfpCallLastDialed");
    if (appHfpIsConnected())
    {
        appHfpCallLastDialed();
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestHandsetHfpSetScoVolume(uint8 volume)
{
    DEBUG_LOG("appTestHandsetHfpScoVolume");
    if (appHfpIsCall())
    {
        appKymeraScoSetVolume(volume);
        return TRUE;
    }
    else
        return FALSE;
}

bool appTestIsHandsetHfpMuted(void)
{
    DEBUG_LOG("appTestIsHandsetHfpMuted");
    return appHfpIsMuted();
}

bool appTestIsHandsetHfpCall(void)
{
    DEBUG_LOG("appTestIsHandsetHfpCall");
    return appHfpIsCall();
}

bool appTestIsHandsetHfpCallIncoming(void) 
{
    DEBUG_LOG("appTestIsHandsetHfpCallIncoming");
    return appHfpIsCallIncoming();
}

bool appTestIsHandsetHfpCallOutgoing(void)
{
    DEBUG_LOG("appTestIsHandsetHfpCallOutgoing");
    return appHfpIsCallOutgoing();
}

/*! \brief Return if Earbud has a connection to the Handset
*/
bool appTestIsHandsetConnected(void)
{
    DEBUG_LOG("appTestIsHandsetConnected");
    return appTestIsHandsetA2dpConnected() ||
           appTestIsHandsetAvrcpConnected() ||
           appTestIsHandsetHfpConnected();
}

/*! \brief Initiate Earbud A2DP connection to the the Peer
*/
bool appTestPeerA2dpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestPeerA2dpConnect");
    if (appDeviceGetPeerBdAddr(&bd_addr))
    {
        return appAvA2dpConnectRequest(&bd_addr, A2DP_CONNECT_NOFLAGS);
    }
    else
    {
        return FALSE;
    }
}

/*! \brief Return if Earbud has a Peer A2DP connection
*/
bool appTestIsPeerA2dpConnected(void)
{
    bdaddr bd_addr;
    appDeviceAttributes attributes;

    DEBUG_LOG("appTestIsPeerA2dpConnected");
    if (appDeviceGetPeerAttributes(&bd_addr, &attributes))
    {
        /* Find peer AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appA2dpIsConnected(theInst);
    }

    /* If we get here then there's no A2DP connected for handset */
    return FALSE;
}


/*! \brief Check if Earbud is in A2DP streaming mode with peer Earbud
 */
bool appTestIsPeerA2dpStreaming(void)
{
    bdaddr bd_addr;
    appDeviceAttributes attributes;

    DEBUG_LOG("appTestIsPeerA2dpStreaming");
    if (appDeviceGetPeerAttributes(&bd_addr, &attributes))
    {
        /* Find peer AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appA2dpIsStreaming(theInst);
    }

    /* If we get here then there's no A2DP connected for peer */
    return FALSE;
}


/*! \brief Initiate Earbud AVRCP connection to the the Peer
*/
bool appTestPeerAvrcpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG("appTestPeerAvrcpConnect");
    if (appDeviceGetPeerBdAddr(&bd_addr))
        return  appAvAvrcpConnectRequest(NULL, &bd_addr);
    else
        return FALSE;
}

/*! \brief Return if Earbud has a Peer AVRCP connection
*/
bool appTestIsPeerAvrcpConnected(void)
{
    bdaddr bd_addr;
    appDeviceAttributes attributes;

    DEBUG_LOG("appTestIsPeerAvrcpConnected");
    while (appDeviceGetPeerAttributes(&bd_addr, &attributes))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        if (theInst)
            return appAvrcpIsConnected(theInst);
    }

    /* If we get here then there's no AVRCP connected for handset */
    return FALSE;
}

/*! \brief Send the Avrcp pause command to the Handset
*/
void appTestAvPause(void)
{
    DEBUG_LOG("appTestAvPause");
    appAvPause(FALSE);
}

/*! \brief Send the Avrcp play command to the Handset
*/
void appTestAvPlay(void)
{
    DEBUG_LOG("appTestAvPlay");
    appAvPlay(FALSE);
}

/*! \brief Send the Avrcp stop command to the Handset
*/
void appTestAvStop(void)
{
    DEBUG_LOG("appTestAvStop");
    appAvStop(FALSE);
}

/*! \brief Send the Avrcp forward command to the Handset
*/
void appTestAvForward(void)
{
    DEBUG_LOG("appTestAvForward");
    appAvForward();
}

/*! \brief Send the Avrcp backward command to the Handset
*/
void appTestAvBackward(void)
{
    DEBUG_LOG("appTestAvBackward");
    appAvBackward();
}

/*! \brief Send the Avrcp fast forward state command to the Handset
*/
void appTestAvFastForwardStart(void)
{
    DEBUG_LOG("appTestAvFastForwardStart");
    appAvFastForwardStart();
}

/*! \brief Send the Avrcp fast forward stop command to the Handset
*/
void appTestAvFastForwardStop(void)
{
    DEBUG_LOG("appTestAvFastForwardStop");
    appAvFastForwardStop();
}

/*! \brief Send the Avrcp rewind start command to the Handset
*/
void appTestAvRewindStart(void)
{
    DEBUG_LOG("appTestAvRewindStart");
    appAvRewindStart();
}

/*! \brief Send the Avrcp rewind stop command to the Handset
*/
void appTestAvRewindStop(void)
{
    DEBUG_LOG("appTestAvRewindStop");
    appAvRewindStop();
}

/*! \brief Send the Avrcp volume change command to the Handset
*/
bool appTestAvVolumeChange(int8 step)
{
    DEBUG_LOGF("appTestAvVolumeChange %d", step);
    return appAvVolumeChange(step);
}

/*! \brief Send the Avrcp pause command to the Handset
*/
void appTestAvVolumeSet(uint8 volume)
{
    DEBUG_LOGF("appTestAvVolumeSet %d", volume);
   appAvVolumeSet(volume, NULL);
}

void appTestAvVolumeSetDb(int8 gain)
{
    /* Set default volume as set in av_headset_config.h */
    const int rangeDb = appConfigMaxVolumedB() - appConfigMinVolumedB();

    DEBUG_LOGF("appTestAvVolumeSetDb %d", gain);
    if (    gain < appConfigMinVolumedB()
        ||  gain > appConfigMaxVolumedB())
    {
        DEBUG_LOGF("appTestAvVolumeSetDb. Gain %d outside range %d-%d",
                        gain,appConfigMinVolumedB(),appConfigMaxVolumedB());
    }

    appAvVolumeSet((gain - appConfigMinVolumedB()) * 127 / rangeDb, NULL);
}

void appTestPowerAllowDormant(bool enable)
{
#ifdef INCLUDE_POWER_CONTROL
    powerTaskData *thePower = appGetPowerControl();

    DEBUG_LOGF("appTestPowerAllowDormant %d", enable);
    thePower->allow_dormant = enable;
#else
    DEBUG_LOGF("appTestPowerAllowDormant(%d): Power Control/Dormant is not in this build.",enable);
#endif
}

/*! \brief Test the generation of link kets */
extern void TestLinkkeyGen(void)
{
    bdaddr bd_addr;
    uint16 lk[8];
    uint16 lk_out[8];

    bd_addr.nap = 0x0002;
    bd_addr.uap = 0x5B;
    bd_addr.lap = 0x00FF02;

    lk[0] = 0x9541;
    lk[1] = 0xe6b4;
    lk[2] = 0x6859;
    lk[3] = 0x0791;
    lk[4] = 0x9df9;
    lk[5] = 0x95cd;
    lk[6] = 0x9570;
    lk[7] = 0x814b;

    DEBUG_LOG("appTestPowerAllowDormant");
    appPairingGenerateLinkKey(&bd_addr, lk, 0x74777332UL, lk_out);

#if 0
    bd_addr.nap = 0x0000;
    bd_addr.uap = 0x74;
    bd_addr.lap = 0x6D7031;

    lk[0] = 0xec02;
    lk[1] = 0x34a3;
    lk[2] = 0x57c8;
    lk[3] = 0xad05;
    lk[4] = 0x3410;
    lk[5] = 0x10a6;
    lk[6] = 0x0a39;
    lk[7] = 0x7d9b;
#endif

    appPairingGenerateLinkKey(&bd_addr, lk, 0x6c656272UL, lk_out);

}

/*! \brief Test the cryptographic key conversion function, producing an H6 key */
extern void TestH6(void)
{
    uint8 key_h7[16] = {0xec,0x02,0x34,0xa3,0x57,0xc8,0xad,0x05,0x34,0x10,0x10,0xa6,0x0a,0x39,0x7d,0x9b};
    //uint32 key_id = 0x6c656272;
    uint32 key_id = 0x7262656c;
    uint8 key_h6[16];

    DEBUG_LOG("appTestPowerAllowDormant");
    CryptoVmH6(key_h7, key_id, key_h6);
    printf("H6: ");
    for (int h6_i = 0; h6_i < 16; h6_i++)
        printf("%02x ", key_h6[h6_i]);
    printf("\n");
}

/*! \brief report the saved handset information */
extern void appTestHandsetInfo(void)
{
    appDeviceAttributes attributes;
    bdaddr bd_addr;
    DEBUG_LOG("appTestHandsetInfo");
    if (appDeviceGetHandsetAttributes(&bd_addr, &attributes, NULL))
    {
        DEBUG_LOGF("appTestHandsetInfo, bdaddr %04x,%02x,%06lx, version %u.%02u",
                   bd_addr.nap, bd_addr.uap, bd_addr.lap,
                   attributes.tws_version >> 8, attributes.tws_version & 0xFF);
        DEBUG_LOGF("appTestHandsetInfo, supported %02x, connected %02x",
                   attributes.supported_profiles, attributes.connected_profiles);
        DEBUG_LOGF("appTestHandsetInfo, a2dp volume %u",
                   attributes.a2dp_volume);
        DEBUG_LOGF("appTestHandsetInfo, flags %02x",
                   attributes.flags);
    }
}

#include "av_headset_log.h"

/*! \brief Simple test function to make sure that the DEBUG_LOG macros
        work */
extern void TestDebug(void)
{
    DEBUG_LOGF("test %d %d", 1, 2);
    DEBUG_LOG("test");
}

/*! \brief Generate event that Earbud is now in the case. */
void appTestPhyStateInCaseEvent(void)
{
    DEBUG_LOG("appTestPhyStateInCaseEvent");
    appPhyStateInCaseEvent();
}

/*! \brief Generate event that Earbud is now out of the case. */
void appTestPhyStateOutOfCaseEvent(void)
{
    DEBUG_LOG("appTestPhyStateOutOfCaseEvent");
    appPhyStateOutOfCaseEvent();
}

/*! \brief Generate event that Earbud is now in ear. */
void appTestPhyStateInEarEvent(void)
{
    DEBUG_LOG("appTestPhyStateInEarEvent");
    appPhyStateInEarEvent();
}

/*! \brief Generate event that Earbud is now out of the ear. */
void appTestPhyStateOutOfEarEvent(void)
{
    DEBUG_LOG("appTestPhyStateOutOfEarEvent");
    appPhyStateOutOfEarEvent();
}

/*! \brief Generate event that Earbud is now moving */
void appTestPhyStateMotionEvent(void)
{
    DEBUG_LOG("appTestPhyStateMotionEvent");
    appPhyStateMotionEvent();
}

/*! \brief Generate event that Earbud is now not moving. */
void appTestPhyStateNotInMotionEvent(void)
{
    DEBUG_LOG("appTestPhyStateNotInMotionEvent");
    appPhyStateNotInMotionEvent();
}

#define ATTRIBUTE_BASE_PSKEY_INDEX  100
#define TDL_BASE_PSKEY_INDEX        142
#define TDL_INDEX_PSKEY             141
#define TDL_SIZE                    8
/*! \brief Delete all state and reboot the earbud.
*/
void appTestResetAndReboot(void)
{
    DEBUG_LOG("appTestResetAndReboot");
    for (int i=0; i<TDL_SIZE; i++)
    {
        PsStore(ATTRIBUTE_BASE_PSKEY_INDEX+i, NULL, 0);
        PsStore(TDL_BASE_PSKEY_INDEX+i, NULL, 0);
        PsStore(TDL_INDEX_PSKEY, NULL, 0);
    }
    
    /* Flood fill PS to force a defrag on reboot */
    PsFlood();
                
    /* call SM to action the reboot */
    appSmReboot();
}

/*! \brief Reset an Earbud to factory defaults.
    Will drop any connections, delete all pairing and reboot.
*/
void appTestFactoryReset(void)
{
    DEBUG_LOG("appTestFactoryReset");
    appTestResetAndReboot();
}

/*! \brief Determine if the earbud has a paired peer earbud.
*/
bool appTestIsPeerPaired(void)
{
    DEBUG_LOG("appTestIsPeerPaired");
    return appDeviceGetPeerBdAddr(NULL);
}

void appTestConnectHandset(void)
{
    DEBUG_LOG("appTestConnectHandset");
    appSmConnectHandset();
}

bool appTestConnectHandsetA2dpMedia(void)
{
    DEBUG_LOG("appTestConnectHandsetA2dpMedia");
    return appAvConnectHandsetA2dpMedia();
}

bool appTestIsPeerSyncComplete(void)
{
    DEBUG_LOG("appTestIsPeerSyncComplete");
    return appSmIsPeerSyncComplete();
}

#ifdef INCLUDE_SCOFWD

static void testTaskHandler(Task task, MessageId id, Message message)
{
    static lp_power_mode   powermode = 42;
    static uint16          interval = (uint16)-1;

    UNUSED(task);

    switch (id)
    {
        case CL_DM_ROLE_CFM:
            {
                const CL_DM_ROLE_CFM_T *cfm = (const CL_DM_ROLE_CFM_T *)message;
                bdaddr  peer;
                tp_bdaddr sink_addr;

                if (  !SinkGetBdAddr(cfm->sink, &sink_addr)
                   || !appDeviceGetPeerBdAddr(&peer))
                {
                    return;
                }

                if (   BdaddrIsSame(&peer, &sink_addr.taddr.addr)
                    && hci_success == cfm->status)
                {
                    if (hci_role_master == cfm->role)
                    {
                        DEBUG_LOG("SCO FORWARDING LINK IS: MASTER");
                    }
                    else if (hci_role_slave == cfm->role)
                    {
                        DEBUG_LOG("SCO FORWARDING LINK IS: SLAVE");
                    }
                    else 
                    {
                        DEBUG_LOGF("SCO FORWARDING LINK STATE IS A MYSTERY: %d",cfm->role);
                    }
                    DEBUG_LOGF("SCO FORWARDING POWER MODE (cached) IS %d (sniff = %d)",powermode,lp_sniff);
                    DEBUG_LOGF("SCO FORWARDING INTERVAL (cached) IS %d",interval);
                }
            }
            break;

        case CL_DM_MODE_CHANGE_EVENT:
            {
                bdaddr peer;
                const CL_DM_MODE_CHANGE_EVENT_T *mode = (const CL_DM_MODE_CHANGE_EVENT_T *)message;

                if (!appDeviceGetPeerBdAddr(&peer))
                    return;

                if (BdaddrIsSame(&mode->bd_addr, &peer))
                {
                    powermode = mode->mode;
                    interval = mode->interval;
                }
            }
            break;
    }
}

void appTestScoFwdLinkStatus(void)
{
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    DEBUG_LOG("appTestScoFwdLinkStatus");

    ConnectionGetRole(&testTask,theScoFwd->link_sink);
}

bool appTestScoFwdConnect(void)
{
    bdaddr peer;
    if (!appDeviceGetPeerBdAddr(&peer))
        return FALSE;
    appScoFwdConnectPeer();
    return TRUE;
}

bool appTestScoFwdDisconnect(void)
{
    appScoFwdDisconnectPeer();
    return TRUE;
}

#endif

#ifdef INCLUDE_POWER_CONTROL
void appTestPowerOff(void)
{
    DEBUG_LOG("appTestPowerOff");
    appPowerOff();
}
#endif

bool appTestLicenseCheck(void)
{
    const uint8 license_table[4] = 
    {
        APTX_CLASSIC, APTX_CLASSIC_MONO, CVC_RECV, CVC_SEND_HS_1MIC
    };
    bool licenses_ok = TRUE;
    
    DEBUG_LOG("appTestLicenseCheck");
    for (int i = 0; i < 4; i++)
    {
        if (!FeatureVerifyLicense(license_table[i]))
        {
            DEBUG_LOGF("appTestLicenseCheck: License for feature %d not valid", license_table[i]);
            licenses_ok = FALSE;
        }
        else
            DEBUG_LOGF("appTestLicenseCheck: License for feature %d valid", license_table[i]);
    }
    
    return licenses_ok;
}

void appTestConnectAfterPairing(bool enable)
{
    connRulesTaskData *conn_rules = appGetConnRules();
    conn_rules->allow_connect_after_pairing = enable;
}

bool appTestScoFwdForceDroppedPackets(unsigned percentage_to_drop, int multiple_packets)
{
#ifdef INCLUDE_SCOFWD_TEST_MODE
    scoFwdTaskData *theScoFwd = appGetScoFwd();

    if (!(0 <= percentage_to_drop && percentage_to_drop <= 100))
    {
        return FALSE;
    }
    if (multiple_packets > 100)
    {
        return FALSE;
    }

    theScoFwd->percentage_to_drop = percentage_to_drop;
    theScoFwd->drop_multiple_packets = multiple_packets;
    return TRUE;
#else
    UNUSED(percentage_to_drop);
    UNUSED(multiple_packets);
    return FALSE;
#endif
}

