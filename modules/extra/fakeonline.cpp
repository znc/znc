/*
 * Copyright (C) 2008-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"
#include "znc.h"

class CFOModule : public CModule {
public:
	MODCONSTRUCTOR(CFOModule) {}
	virtual ~CFOModule() {}

	bool IsOnlineModNick(const CString& sNick) {
		const CString& sPrefix = m_pUser->GetStatusPrefix();
		if (!sNick.Equals(sPrefix, false, sPrefix.length()))
			return false;

		CString sModNick = sNick.substr(sPrefix.length());
		if (!sModNick.Equals("status") &&
				!m_pUser->GetModules().FindModule(sModNick) &&
				!CZNC::Get().GetModules().FindModule(sModNick))
			return false;
		return true;
	}

	virtual EModRet OnUserRaw(CString& sLine) {
		//Handle ISON
		if (sLine.Token(0).Equals("ison")) {
			VCString vsNicks;
			VCString::const_iterator it;

			// Get the list of nicks which are being asked for
			sLine.Token(1, true).TrimLeft_n(":").Split(" ", vsNicks, false);

			CString sBNCNicks;
			for (it = vsNicks.begin(); it != vsNicks.end(); ++it) {
				if (IsOnlineModNick(*it)) {
					sBNCNicks += " " + *it;
				}
			}
			// Remove the leading space
			sBNCNicks.LeftChomp();

			if (!m_pUser->GetIRCSock()) {
				// if we are not connected to any IRC server, send
				// an empty or module-nick filled response.
				PutUser(":irc.znc.in 303 " + m_pUser->GetNick() + " :" + sBNCNicks);
			} else {
				// We let the server handle this request and then act on
				// the 303 response from the IRC server.
				m_ISONRequests.push_back(sBNCNicks);
			}
		}

		//Handle WHOIS
		if (sLine.Token(0).Equals("whois")) {
			CString sNick = sLine.Token(1);

			if (IsOnlineModNick(sNick)) {
				PutUser(":znc.in 311 " + m_pUser->GetCurNick() + " " + sNick + " " + sNick + " znc.in * :" + sNick);
				PutUser(":znc.in 312 " + m_pUser->GetCurNick() + " " + sNick + " *.znc.in :Bouncer");
				PutUser(":znc.in 318 " + m_pUser->GetCurNick() + " " + sNick + " :End of /WHOIS list.");

				return HALT;
			}
		}

		return CONTINUE;
	}

	virtual EModRet OnRaw(CString& sLine) {
		//Handle 303 reply if m_Requests is not empty
		if (sLine.Token(1) == "303" && !m_ISONRequests.empty()) {
			VCString::iterator it = m_ISONRequests.begin();

			sLine.Trim();

			// Only append a space if this isn't an empty reply
			if (sLine.Right(1) != ":") {
				sLine += " ";
			}

			//add BNC nicks to the reply
			sLine += *it;
			m_ISONRequests.erase(it);
		}

		return CONTINUE;
	}

private:
	VCString m_ISONRequests;
};

MODULEDEFS(CFOModule, "Fakes online status of ZNC *-users.")
