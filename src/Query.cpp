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

using std::vector;

CQuery::CQuery(const CString& sName, CIRCNetwork* pNetwork) {
	m_sName = sName;
	m_pNetwork = pNetwork;

	SetBufferCount(m_pNetwork->GetUser()->GetBufferCount(), true);
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
			for (size_t uClient = 0; uClient < vClients.size(); ++uClient) {
				CClient * pUseClient = (pClient ? pClient : vClients[uClient]);

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

					if (!pUseClient->HasSelfMessage()) {
						CNick Sender(BufLine.GetFormat().Token(0));
						if (Sender.NickEquals(pUseClient->GetNick())) {
							continue;
						}
					}

					CString sLine = BufLine.GetLine(*pUseClient, msParams);
					if (bBatch) {
						MCString msBatchTags = CUtils::GetMessageTags(sLine);
						msBatchTags["batch"] = sBatchName;
						CUtils::SetMessageTags(sLine, msBatchTags);
					}
					bool bContinue = false;
					NETWORKMODULECALL(OnPrivBufferPlayLine2(*pUseClient, sLine, BufLine.GetTime()), m_pNetwork->GetUser(), m_pNetwork, nullptr, &bContinue);
					if (bContinue) continue;
					m_pNetwork->PutUser(sLine, pUseClient);
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
