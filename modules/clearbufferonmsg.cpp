/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
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

#include <znc/IRCNetwork.h>
#include <znc/Chan.h>

using std::vector;

class CClearBufferOnMsgMod : public CModule {
public:
	MODCONSTRUCTOR(CClearBufferOnMsgMod) {}

	void ClearAllBuffers() {
		if (m_pNetwork) {
			const vector<CChan*>& vChans = m_pNetwork->GetChans();
			vector<CChan*>::const_iterator it;

			for (it = vChans.begin(); it != vChans.end(); ++it) {
				// Skip detached channels, they weren't read yet
				if ((*it)->IsDetached())
					continue;

				(*it)->ClearBuffer();
				// We deny AutoClearChanBuffer on all channels since this module
				// doesn't make any sense with it
				(*it)->SetAutoClearChanBuffer(false);
			}
		}
	}

	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage) {
		ClearAllBuffers();
		return CONTINUE;
	}

	virtual EModRet OnUserCTCP(CString& sTarget, CString& sMessage) {
		ClearAllBuffers();
		return CONTINUE;
	}

	virtual EModRet OnUserAction(CString& sTarget, CString& sMessage) {
		ClearAllBuffers();
		return CONTINUE;
	}

	virtual EModRet OnUserNotice(CString& sTarget, CString& sMessage) {
		ClearAllBuffers();
		return CONTINUE;
	}

	virtual EModRet OnUserPart(CString& sChannel, CString& sMessage) {
		ClearAllBuffers();
		return CONTINUE;
	}

	virtual EModRet OnUserTopic(CString& sChannel, CString& sTopic) {
		ClearAllBuffers();
		return CONTINUE;
	}
};

template<> void TModInfo<CClearBufferOnMsgMod>(CModInfo& Info) {
	Info.SetWikiPage("clearbufferonmsg");
}

USERMODULEDEFS(CClearBufferOnMsgMod, "Clear all channel buffers whenever the user does something")
