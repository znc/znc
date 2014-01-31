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

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>

using std::vector;

class CBuffExtras : public CModule {
public:
	MODCONSTRUCTOR(CBuffExtras) {}

	virtual ~CBuffExtras() {}

	void AddBuffer(CChan& Channel, const CString& sMessage) {
		// If they have AutoClearChanBuffer enabled, only add messages if no client is connected
		if (Channel.AutoClearChanBuffer() && m_pNetwork->IsUserOnline())
			return;

		Channel.AddBuffer(":" + GetModNick() + "!" + GetModName() + "@znc.in PRIVMSG " + _NAMEDFMT(Channel.GetName()) + " :{text}", sMessage);
	}

	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {
		AddBuffer(Channel, OpNick.GetNickMask() + " set mode: " + sModes + " " + sArgs);
	}

	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {
		AddBuffer(Channel, OpNick.GetNickMask() + " kicked " + sKickedNick + " Reason: [" + sMessage + "]");
	}

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
		vector<CChan*>::const_iterator it;
		CString sMsg = Nick.GetNickMask() + " quit with message: [" + sMessage + "]";
		for (it = vChans.begin(); it != vChans.end(); ++it) {
			AddBuffer(**it, sMsg);
		}
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		AddBuffer(Channel, Nick.GetNickMask() + " joined");
	}

	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {
		AddBuffer(Channel, Nick.GetNickMask() + " parted with message: [" + sMessage + "]");
	}

	virtual void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans) {
		vector<CChan*>::const_iterator it;
		CString sMsg = OldNick.GetNickMask() + " is now known as " + sNewNick;
		for (it = vChans.begin(); it != vChans.end(); ++it) {
			AddBuffer(**it, sMsg);
		}
	}

	virtual EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) {
		AddBuffer(Channel, Nick.GetNickMask() + " changed the topic to: " + sTopic);

		return CONTINUE;
	}
};

template<> void TModInfo<CBuffExtras>(CModInfo& Info) {
	Info.SetWikiPage("buffextras");
}

USERMODULEDEFS(CBuffExtras, "Add joins, parts etc. to the playback buffer")

