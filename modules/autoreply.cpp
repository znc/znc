/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
 * Copyright (C) 2008 Michael "Svedrin" Ziegler diese-addy@funzt-halt.net
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
#include <znc/IRCSock.h>

class CAutoReplyMod : public CModule {
public:
	MODCONSTRUCTOR(CAutoReplyMod) {
		m_Messaged.SetTTL(1000 * 120);
	}

	virtual ~CAutoReplyMod() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (!sArgs.empty()) {
			SetReply(sArgs);
		}

		return true;
	}

	void SetReply(const CString& sReply) {
		SetNV("Reply", sReply);
	}

	CString GetReply() {
		CString sReply = GetNV("Reply");
		if (sReply.empty()) {
			sReply = "%nick% is currently away, try again later";
			SetReply(sReply);
		}

		return ExpandString(sReply);
	}

	void Handle(const CString& sNick) {
		CIRCSock *pIRCSock = m_pNetwork->GetIRCSock();
		if (!pIRCSock)
			// WTF?
			return;
		if (sNick == pIRCSock->GetNick())
			return;
		if (m_Messaged.HasItem(sNick))
			return;

		if (m_pNetwork->IsUserAttached())
			return;

		m_Messaged.AddItem(sNick);
		PutIRC("NOTICE " + sNick + " :" + GetReply());
	}

	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage) {
		Handle(Nick.GetNick());
		return CONTINUE;
	}

	virtual void OnModCommand(const CString& sCommand) {
		const CString& sCmd = sCommand.Token(0);

		if (sCmd.Equals("SHOW")) {
			PutModule("Current reply is: " + GetNV("Reply")
					+ " (" + GetReply() + ")");
		} else if (sCmd.Equals("SET")) {
			SetReply(sCommand.Token(1, true));
			PutModule("New reply set");
		} else {
			PutModule("Available commands are:");
			PutModule("Show        - Displays the current query reply");
			PutModule("Set <reply> - Sets a new reply");
		}
	}

private:
	TCacheMap<CString> m_Messaged;
};

template<> void TModInfo<CAutoReplyMod>(CModInfo& Info) {
	Info.SetWikiPage("autoreply");
	Info.AddType(CModInfo::NetworkModule);
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("You might specify a reply text. It is used when automatically answering queries, if you are not connected to ZNC.");
}

USERMODULEDEFS(CAutoReplyMod, "Reply to queries when you are away")

