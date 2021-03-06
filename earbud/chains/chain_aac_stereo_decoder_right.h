/*!
    \copyright Copyright (c) 2018 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version Part of 6.2
    \file chain_aac_stereo_decoder_right.h
    \brief The chain_aac_stereo_decoder_right chain. This file is generated by C:/qtil/ADK_QCC512x_QCC302x_WIN_6.2.70/tools/chaingen/chaingen.py.
*/

#ifndef _CHAIN_AAC_STEREO_DECODER_RIGHT_H__
#define _CHAIN_AAC_STEREO_DECODER_RIGHT_H__

/*!
    @startuml
        object OPR_LATENCY_BUFFER
        OPR_LATENCY_BUFFER : id = CAP_ID_DOWNLOAD_SWITCHED_PASSTHROUGH_CONSUMER
        object OPR_AAC_DECODER
        OPR_AAC_DECODER : id = CAP_ID_AAC_DECODER
        object OPR_CONSUMER
        OPR_CONSUMER : id = CAP_ID_DOWNLOAD_SWITCHED_PASSTHROUGH_CONSUMER
        OPR_AAC_DECODER "IN(0)"<-- "OUT(0)" OPR_LATENCY_BUFFER
        OPR_CONSUMER "IN(0)"<-- "OUT_L(0)" OPR_AAC_DECODER
        object EPR_SINK_MEDIA #lightgreen
        OPR_LATENCY_BUFFER "IN(0)" <-- EPR_SINK_MEDIA
        object EPR_SOURCE_DECODED_PCM #lightblue
        EPR_SOURCE_DECODED_PCM <-- "OUT_R(1)" OPR_AAC_DECODER
    @enduml
*/

#include <chain.h>

extern const chain_config_t chain_aac_stereo_decoder_right_config;

#endif /* _CHAIN_AAC_STEREO_DECODER_RIGHT_H__ */

