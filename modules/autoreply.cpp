/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 * Copyright (C) 2008 Michael "Svedrin" Ziegler diese-addy@funzt-halt.net
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Modules.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <znc/User.h>

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

		if (m_pNetwork) {
			return m_pNetwork->ExpandString(sReply);
		}

		return m_pUser->ExpandString(sReply);
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
		PutIRC("PRIVMSG " + sNick + " :" + GetReply());
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
}

USERMODULEDEFS(CAutoReplyMod, "Reply to queries when you are away")

