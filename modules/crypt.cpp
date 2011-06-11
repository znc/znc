/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

//! @author prozac@rottenboy.com
//
// The encryption here was designed to be compatible with mircryption's CBC mode.
//
// TODO:
//
// 1) Encrypt key storage file
// 2) Secure key exchange using pub/priv keys and the DH algorithm
// 3) Some way of notifying the user that the current channel is in "encryption mode" verses plain text
// 4) Temporarily disable a target (nick/chan)
//
// NOTE: This module is currently NOT intended to secure you from your shell admin.
//       The keys are currently stored in plain text, so anyone with access to your account (or root) can obtain them.
//       It is strongly suggested that you enable SSL between znc and your client otherwise the encryption stops at znc and gets sent to your client in plain text.
//

#include "Chan.h"
#include "User.h"

#define REQUIRESSL	1

class CCryptMod : public CModule {
public:
	MODCONSTRUCTOR(CCryptMod) {}
	virtual ~CCryptMod() {}

	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage) {
		sTarget.TrimLeft("\244");

		if (sMessage.Left(2) == "``") {
			sMessage.LeftChomp(2);
			return CONTINUE;
		}

		MCString::iterator it = FindNV(sTarget.AsLower());

		if (it != EndNV()) {
			CChan* pChan = m_pUser->FindChan(sTarget);
			if (pChan) {
				if (pChan->KeepBuffer())
					pChan->AddBuffer(":\244" + m_pUser->GetIRCNick().GetNickMask() + " PRIVMSG " + sTarget + " :" + sMessage);
				m_pUser->PutUser(":\244" + m_pUser->GetIRCNick().GetNickMask() + " PRIVMSG " + sTarget + " :" + sMessage, NULL, m_pClient);
			}

			CString sMsg = MakeIvec() + sMessage;
			sMsg.Encrypt(it->second);
			sMsg.Base64Encode();
			sMsg = "+OK *" + sMsg;

			PutIRC("PRIVMSG " + sTarget + " :" + sMsg);
			return HALTCORE;
		}

		return CONTINUE;
	}

	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage) {
		FilterIncoming(Nick.GetNick(), Nick, sMessage);
		return CONTINUE;
	}

	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
		FilterIncoming(Channel.GetName(), Nick, sMessage);
		return CONTINUE;
	}

	void FilterIncoming(const CString& sTarget, CNick& Nick, CString& sMessage) {
		if (sMessage.Left(5) == "+OK *") {
			MCString::iterator it = FindNV(sTarget.AsLower());

			if (it != EndNV()) {
				sMessage.LeftChomp(5);
				sMessage.Base64Decode();
				sMessage.Decrypt(it->second);
				sMessage.LeftChomp(8);
				sMessage = sMessage.c_str();
				Nick.SetNick("\244" + Nick.GetNick());
			}
		}

	}

	virtual void OnModCommand(const CString& sCommand) {
		CString sCmd = sCommand.Token(0);

		if (sCmd.Equals("DELKEY")) {
			CString sTarget = sCommand.Token(1);

			if (!sTarget.empty()) {
				if (DelNV(sTarget.AsLower())) {
					PutModule("Target [" + sTarget + "] deleted");
				} else {
					PutModule("Target [" + sTarget + "] not found");
				}
			} else {
				PutModule("Usage DelKey <#chan|Nick>");
			}
		} else if (sCmd.Equals("SETKEY")) {
			CString sTarget = sCommand.Token(1);
			CString sKey = sCommand.Token(2, true);

			// Strip "cbc:" from beginning of string incase someone pastes directly from mircryption
			sKey.TrimPrefix("cbc:");

			if (!sKey.empty()) {
				SetNV(sTarget.AsLower(), sKey);
				PutModule("Set encryption key for [" + sTarget + "] to [" + sKey + "]");
			} else {
				PutModule("Usage: SetKey <#chan|Nick> <Key>");
			}
		} else if (sCmd.Equals("LISTKEYS")) {
			if (BeginNV() == EndNV()) {
				PutModule("You have no encryption keys set.");
			} else {
				CTable Table;
				Table.AddColumn("Target");
				Table.AddColumn("Key");

				for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
					Table.AddRow();
					Table.SetCell("Target", it->first);
					Table.SetCell("Key", it->second);
				}

				PutModule(Table);
			}
		} else if (sCmd.Equals("HELP")) {
			PutModule("Try: SetKey, DelKey, ListKeys");
		} else {
			PutModule("Unknown command, try 'Help'");
		}
	}

	CString MakeIvec() {
		CString sRet;
		time_t t;
		time(&t);
		int r = rand();
		sRet.append((char*) &t, 4);
		sRet.append((char*) &r, 4);

		return sRet;
	}
};

template<> void TModInfo<CCryptMod>(CModInfo& Info) {
	Info.SetWikiPage("crypt");
}

MODULEDEFS(CCryptMod, "Encryption for channel/private messages")
