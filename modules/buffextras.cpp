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

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>

using std::vector;

class CBuffExtras : public CModule {
public:
	MODCONSTRUCTOR(CBuffExtras) {}

	virtual ~CBuffExtras() {}

	void AddBuffer(CChan& Channel, const CString& sMessage, const timeval* tv = nullptr, const MCString& mssTags = MCString::EmptyMap) {
		// If they have AutoClearChanBuffer enabled, only add messages if no client is connected
		if (Channel.AutoClearChanBuffer() && GetNetwork()->IsUserOnline())
			return;

		Channel.AddBuffer(":" + GetModNick() + "!" + GetModName() + "@znc.in PRIVMSG " + _NAMEDFMT(Channel.GetName()) + " :{text}", sMessage, tv, mssTags);
	}

	void OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes, const CString& sArgs) override {
		const CString sNickMask = pOpNick ? pOpNick->GetNickMask() : "Server";
		AddBuffer(Channel, sNickMask + " set mode: " + sModes + " " + sArgs);
	}

	void OnKickMessage(CKickMessage& Message) override {
		const CNick& OpNick = Message.GetNick();
		const CString sKickedNick = Message.GetKickedNick();
		CChan& Channel = *Message.GetChan();
		const CString sMessage = Message.GetReason();
		AddBuffer(Channel, OpNick.GetNickMask() + " kicked " + sKickedNick + " Reason: [" + sMessage + "]", &Message.GetTime(), Message.GetTags());
	}

	void OnQuitMessage(CQuitMessage& Message, const vector<CChan*>& vChans) override {
		const CNick& Nick = Message.GetNick();
		const CString sMessage = Message.GetReason();
		CString sMsg = Nick.GetNickMask() + " quit with message: [" + sMessage + "]";
		for (CChan* pChan : vChans) {
			AddBuffer(*pChan, sMsg, &Message.GetTime(), Message.GetTags());
		}
	}

	void OnJoinMessage(CJoinMessage& Message) override {
		const CNick& Nick = Message.GetNick();
		CChan& Channel = *Message.GetChan();
		AddBuffer(Channel, Nick.GetNickMask() + " joined", &Message.GetTime(), Message.GetTags());
	}

	void OnPartMessage(CPartMessage& Message) override {
		const CNick& Nick = Message.GetNick();
		CChan& Channel = *Message.GetChan();
		const CString sMessage = Message.GetReason();
		AddBuffer(Channel, Nick.GetNickMask() + " parted with message: [" + sMessage + "]", &Message.GetTime(), Message.GetTags());
	}

	void OnNickMessage(CNickMessage& Message, const vector<CChan*>& vChans) override {
		const CNick& OldNick = Message.GetNick();
		const CString sNewNick = Message.GetNewNick();
		CString sMsg = OldNick.GetNickMask() + " is now known as " + sNewNick;
		for (CChan* pChan : vChans) {
			AddBuffer(*pChan, sMsg, &Message.GetTime(), Message.GetTags());
		}
	}

	EModRet OnTopicMessage(CTopicMessage& Message) override {
		const CNick& Nick = Message.GetNick();
		CChan& Channel = *Message.GetChan();
		const CString sTopic = Message.GetTopic();
		AddBuffer(Channel, Nick.GetNickMask() + " changed the topic to: " + sTopic, &Message.GetTime(), Message.GetTags());

		return CONTINUE;
	}
};

template<> void TModInfo<CBuffExtras>(CModInfo& Info) {
	Info.SetWikiPage("buffextras");
	Info.AddType(CModInfo::NetworkModule);
}

USERMODULEDEFS(CBuffExtras, "Add joins, parts etc. to the playback buffer")

