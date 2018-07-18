/*!
\copyright  Copyright (c) 2005 - 2018 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    Part of 6.2
\file       av_headset_sm_peer_sync.h
\brief	    Application state machine peer synchronisation logic interface.
*/

#ifndef _AV_HEADSET_SM_PEER_SYNC_H_
#define _AV_HEADSET_SM_PEER_SYNC_H_

#include "av_headset_peer_signalling.h"

/*! \brief Send a peer sync to peer earbud.
    \param response [IN] TRUE if this is a response peer sync.
 */
void appSmSendPeerSync(bool response);

/*! \brief Handle confirmation of peer sync transmission.
 */
void appSmHandlePeerSigSyncConfirm(PEER_SIG_SYNC_CFM_T *cfm);

/*! \brief Handle indication of incoming peer sync from peer.
 */
void appSmHandlePeerSigSyncIndication(PEER_SIG_SYNC_IND_T *ind);

/*! \brief Determine if peer sync is complete.

    A complete peer sync is defined as both peers having sent their most
    up to date peer sync message and having received a peer sync from 
    their peer.
 */
bool appSmIsPeerSyncComplete(void);

#endif /* _AV_HEADSET_SM_PEER_SYNC_H_ */
