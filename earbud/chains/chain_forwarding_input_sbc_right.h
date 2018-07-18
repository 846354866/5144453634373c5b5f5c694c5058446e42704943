/*!
    \copyright Copyright (c) 2018 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version Part of 6.2
    \file chain_forwarding_input_sbc_right.h
    \brief The chain_forwarding_input_sbc_right chain. This file is generated by C:/qtil/ADK_QCC512x_QCC302x_WIN_6.2.70/tools/chaingen/chaingen.py.
*/

#ifndef _CHAIN_FORWARDING_INPUT_SBC_RIGHT_H__
#define _CHAIN_FORWARDING_INPUT_SBC_RIGHT_H__

/*!
    @startuml
        object OPR_RTP_DECODER
        OPR_RTP_DECODER : id = CAP_ID_RTP_DECODE
        object OPR_SWITCHED_PASSTHROUGH_CONSUMER
        OPR_SWITCHED_PASSTHROUGH_CONSUMER : id = CAP_ID_DOWNLOAD_SWITCHED_PASSTHROUGH_CONSUMER
        object OPR_SBC_DECODER
        OPR_SBC_DECODER : id = CAP_ID_SBC_DECODER
        object OPR_SBC_ENCODER
        OPR_SBC_ENCODER : id = CAP_ID_SBC_ENCODER
        OPR_SBC_DECODER "IN(0)"<-- "OUT(0)" OPR_RTP_DECODER
        OPR_SBC_ENCODER "IN(0)"<-- "OUT_0(0)" OPR_SBC_DECODER
        OPR_SWITCHED_PASSTHROUGH_CONSUMER "IN(0)"<-- "OUT(0)" OPR_SBC_ENCODER
        object EPR_SINK_MEDIA #lightgreen
        OPR_RTP_DECODER "IN(0)" <-- EPR_SINK_MEDIA
        object EPR_SOURCE_FORWARDING_MEDIA #lightblue
        EPR_SOURCE_FORWARDING_MEDIA <-- "OUT(0)" OPR_SWITCHED_PASSTHROUGH_CONSUMER
        object EPR_SOURCE_DECODED_PCM #lightblue
        EPR_SOURCE_DECODED_PCM <-- "OUT_1(1)" OPR_SBC_DECODER
    @enduml
*/

#include <chain.h>

extern const chain_config_t chain_forwarding_input_sbc_right_config;

#endif /* _CHAIN_FORWARDING_INPUT_SBC_RIGHT_H__ */

