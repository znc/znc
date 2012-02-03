/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Chan.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>

class CBuffExtras : public CModule {
public:
	MODCONSTRUCTOR(CBuffExtras) {}

	virtual ~CBuffExtras() {}

	void AddBuffer(CChan& Channel, const CString& sMessage) {
		// If they have keep buffer disabled, only add messages if no client is connected
		if (!Channel.KeepBuffer() && m_pNetwork->IsUserOnline())
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

