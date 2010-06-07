/*
 * Copyright (C) 2004-2010  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Modules.h"
#include "User.h"

class CPreventIdMsgMod : public CModule {
public:
	MODCONSTRUCTOR(CPreventIdMsgMod) {}

	/* Old style:
	 * -> CAPAB :identify-msg
	 * We just block this request from the client.
	 *
	 * New style:
	 * -> CAP LS
	 * <- :farmer.freenode.net CAP <nick> LS :identify-msg <other stuff>
	 * -> CAP REQ :identify-msg
	 * <- :farmer.freenode.net CAP a ACK :identify-msg
	 * We block the identify-msg token from CAP LS and CAP REQ,
	 * just to be sure.
	 */

	// Returns true if an "identify-msg" was skipped
	static bool MakeCleanCapList(CString& sList) {
		bool bRet = false;
		VCString vsList;
		sList.Split(" ", vsList);
		sList = "";

		// Append each token to the output list
		for (VCString::const_iterator it = vsList.begin();
				it != vsList.end(); ++it) {
			if (!it->Equals("identify-msg")) {
				sList += *it + " ";
			} else {
				bRet = true;
			}
		}

		// Remove the last space we added
		sList.RightChomp();

		return bRet;
	}

	virtual EModRet OnUserRaw(CString& sLine) {
		if (sLine.Token(0).Equals("CAPAB")) {
			if (sLine.AsLower().find("identify-msg") != CString::npos
					|| sLine.AsLower().find("identify-ctcp") != CString::npos)
				return HALTCORE;
		} else if (sLine.Token(0).Equals("CAP")) {
			if (sLine.Token(1).Equals("REQ")) {
				CString sList = sLine.Token(2, true).TrimPrefix_n(":");
				if (MakeCleanCapList(sList)) {
					// There was an identify-msg requested,
					// block the whole list
					PutUser(":irc.znc.in CAP " + m_pUser->GetIRCNick().GetNick()
							+ " NAK " + sLine.Token(2, true));
					return HALTCORE;
				}
			}
		}
		return CONTINUE;
	}

	virtual EModRet OnRaw(CString& sLine) {
		if (!sLine.Token(1).Equals("CAP"))
			return CONTINUE;
		if (!sLine.Token(3).Equals("LS"))
			return CONTINUE;

		// It's CAP LS, strip identify-msg

		// First, copy over the "prefix" from the old line
		CString sNewLine = sLine.Token(0) + " " + sLine.Token(1) + " ";
		sNewLine += sLine.Token(2) + " " + sLine.Token(3) + " :";

		// Grab the list of CAP tokens and clean them up
		CString sTokens = sLine.Token(4, true).TrimPrefix_n(":");
		MakeCleanCapList(sTokens);

		sLine = sNewLine + sTokens;
		return CONTINUE;
	}
};

MODULEDEFS(CPreventIdMsgMod, "Prevent client from sending IDENTIFY-MSG to server")
