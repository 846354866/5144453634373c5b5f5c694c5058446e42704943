/*!
    \copyright Copyright (c) 2018 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version Part of 6.2
    \file chain_tone_gen.h
    \brief The chain_tone_gen chain. This file is generated by C:/qtil/ADK_QCC512x_QCC302x_WIN_6.2.70/tools/chaingen/chaingen.py.
*/

#ifndef _CHAIN_TONE_GEN_H__
#define _CHAIN_TONE_GEN_H__

/*!
    @startuml
        object TONE_GEN
        TONE_GEN : id = CAP_ID_RINGTONE_GENERATOR
        object RESAMPLER
        RESAMPLER : id = CAP_ID_IIR_RESAMPLER
        RESAMPLER "IN(0)"<-- "OUT(0)" TONE_GEN
        object RESAMPLER_OUT #lightblue
        RESAMPLER_OUT <-- "OUT(0)" RESAMPLER
    @enduml
*/

#include <chain.h>

enum chain_tone_gen_operators
{
    TONE_GEN,
    RESAMPLER
};

enum chain_tone_gen_endpoints
{
    RESAMPLER_OUT
};

extern const chain_config_t chain_tone_gen_config;

#endif /* _CHAIN_TONE_GEN_H__ */

