/*!
    \copyright Copyright (c) 2018 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version Part of 6.2
    \file chain_scofwd_recv.c
    \brief The chain_scofwd_recv chain. This file is generated by C:/qtil/ADK_QCC512x_QCC302x_WIN_6.2.70/tools/chaingen/chaingen.py.
*/

#include <chain_scofwd_recv.h>
#include <cap_id_prim.h>
#include <opmsg_prim.h>
#include <hydra_macros.h>
#include <../av_headset_chain_roles.h>
static const operator_config_t operators[] =
{
    MAKE_OPERATOR_CONFIG(SFWD_CAP_ID_ASYNC_WBS_DEC, OPR_SCOFWD_RECV),
    MAKE_OPERATOR_CONFIG(CAP_ID_BASIC_PASS, OPR_SCOFWD_BUFFERING),
    MAKE_OPERATOR_CONFIG(CAP_ID_SOURCE_SYNC, OPR_SOURCE_SYNC),
    MAKE_OPERATOR_CONFIG(CAP_ID_VOL_CTRL_VOL, OPR_VOLUME_CONTROL),
    MAKE_OPERATOR_CONFIG(SFWD_CAP_ID_AEC_REF, OPR_SCO_AEC)
};

static const operator_endpoint_t inputs[] =
{
    {OPR_SCOFWD_RECV, EPR_SCOFWD_RX_OTA, 0},
    {OPR_SCO_AEC, EPR_SCO_MIC1, 2},
    {OPR_VOLUME_CONTROL, EPR_SCO_VOLUME_AUX, 1}
};

static const operator_endpoint_t outputs[] =
{
    {OPR_SCO_AEC, EPR_SCO_SPEAKER, 1}
};

static const operator_connection_t connections[] =
{
    {OPR_SCOFWD_RECV, 0, OPR_SCOFWD_BUFFERING, 0, 1},
    {OPR_SCOFWD_BUFFERING, 0, OPR_SOURCE_SYNC, 0, 1},
    {OPR_SOURCE_SYNC, 0, OPR_VOLUME_CONTROL, 0, 1},
    {OPR_VOLUME_CONTROL, 0, OPR_SCO_AEC, 0, 1}
};

const unsigned chain_scofwd_recv_exclude_from_configure_sample_rate[] =
{
    OPR_SCOFWD_RECV
};

const chain_config_t chain_scofwd_recv_config = {1, 0, operators, 5, inputs, 3, outputs, 1, connections, 4};

