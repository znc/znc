/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/Query.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Message.h>

using std::vector;

CQuery::CQuery(const CString& sName, CIRCNetwork* pNetwork) : m_bDetached(false), m_sName(sName), m_pNetwork(pNetwork), m_Buffer() {
	SetBufferCount(m_pNetwork->GetUser()->GetQueryBufferSize(), true);
}

CQuery::~CQuery() {
}

void CQuery::SendBuffer(CClient* pClient) {
	SendBuffer(pClient, m_Buffer);
}

void CQuery::SendBuffer(CClient* pClient, const CBuffer& Buffer) {
	if (m_pNetwork && m_pNetwork->IsUserAttached()) {
		// Based on CChan::SendBuffer()
		if (!Buffer.IsEmpty()) {
			const vector<CClient*> & vClients = m_pNetwork->GetClients();
			for (CClient* pEachClient : vClients) {
				CClient * pUseClient = (pClient ? pClient : pEachClient);

				MCString msParams;
				msParams["target"] = pUseClient->GetNick();

				bool bWasPlaybackActive = pUseClient->IsPlaybackActive();
				pUseClient->SetPlaybackActive(true);

				bool bBatch = pUseClient->HasBatch();
				CString sBatchName = m_sName.MD5();

				if (bBatch) {
					m_pNetwork->PutUser(":znc.in BATCH +" + sBatchName + " znc.in/playback " + m_sName, pUseClient);
				}

				size_t uSize = Buffer.Size();
				for (size_t uIdx = 0; uIdx < uSize; uIdx++) {
					const CBufLine& BufLine = Buffer.GetBufLine(uIdx);
					CMessage Message = BufLine.ToMessage(*pUseClient, msParams);
					if (!pUseClient->HasEchoMessage() && !pUseClient->HasSelfMessage()) {
						if (Message.GetNick().NickEquals(pUseClient->GetNick())) {
							continue;
						}
					}
					Message.SetNetwork(m_pNetwork);
					Message.SetClient(pUseClient);
					if (bBatch) {
						Message.SetTag("batch", sBatchName);
					}
					bool bContinue = false;
					NETWORKMODULECALL(OnPrivBufferPlayMessage(Message), m_pNetwork->GetUser(), m_pNetwork, nullptr, &bContinue);
					if (bContinue) continue;
					m_pNetwork->PutUser(Message, pUseClient);
				}

				if (bBatch) {
					m_pNetwork->PutUser(":znc.in BATCH -" + sBatchName, pUseClient);
				}

				pUseClient->SetPlaybackActive(bWasPlaybackActive);

				if (pClient)
					break;
			}
		}
	}
}

void CQuery::AttachUser(CClient* pClient)
{
	SetDetached(false);
	SendBuffer(pClient);
}

void CQuery::DetachUser()
{
	SetDetached(true);
}
