/*!
    \copyright Copyright (c) 2018 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version Part of 6.2
    \file chain_scofwd_recv.h
    \brief The chain_scofwd_recv chain. This file is generated by C:/qtil/ADK_QCC512x_QCC302x_WIN_6.2.70/tools/chaingen/chaingen.py.
*/

#ifndef _CHAIN_SCOFWD_RECV_H__
#define _CHAIN_SCOFWD_RECV_H__

/*!
    @startuml
        object OPR_SCOFWD_RECV
        OPR_SCOFWD_RECV : id = SFWD_CAP_ID_ASYNC_WBS_DEC
        object OPR_SCOFWD_BUFFERING
        OPR_SCOFWD_BUFFERING : id = CAP_ID_BASIC_PASS
        object OPR_SOURCE_SYNC
        OPR_SOURCE_SYNC : id = CAP_ID_SOURCE_SYNC
        object OPR_VOLUME_CONTROL
        OPR_VOLUME_CONTROL : id = CAP_ID_VOL_CTRL_VOL
        object OPR_SCO_AEC
        OPR_SCO_AEC : id = SFWD_CAP_ID_AEC_REF
        OPR_SCOFWD_BUFFERING "IN(0)"<-- "OUT(0)" OPR_SCOFWD_RECV
        OPR_SOURCE_SYNC "IN(0)"<-- "OUT(0)" OPR_SCOFWD_BUFFERING
        OPR_VOLUME_CONTROL "MAIN_IN(0)"<-- "OUT(0)" OPR_SOURCE_SYNC
        OPR_SCO_AEC "INPUT1(0)"<-- "OUT(0)" OPR_VOLUME_CONTROL
        object EPR_SCOFWD_RX_OTA #lightgreen
        OPR_SCOFWD_RECV "FORWARDED_AUDIO(0)" <-- EPR_SCOFWD_RX_OTA
        object EPR_SCO_MIC1 #lightgreen
        OPR_SCO_AEC "MIC1(2)" <-- EPR_SCO_MIC1
        object EPR_SCO_VOLUME_AUX #lightgreen
        OPR_VOLUME_CONTROL "AUX_IN(1)" <-- EPR_SCO_VOLUME_AUX
        object EPR_SCO_SPEAKER #lightblue
        EPR_SCO_SPEAKER <-- "SPEAKER1(1)" OPR_SCO_AEC
    @enduml
*/

#include <chain.h>

extern const unsigned chain_scofwd_recv_exclude_from_configure_sample_rate[1];
extern const chain_config_t chain_scofwd_recv_config;

#endif /* _CHAIN_SCOFWD_RECV_H__ */
