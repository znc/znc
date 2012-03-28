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
#include <znc/znc.h>

class CChanSaverMod : public CModule {
public:
	MODCONSTRUCTOR(CChanSaverMod) {
		m_bWriteConf = false;

		vector<CIRCNetwork*> vNetworks = pUser->GetNetworks();
		for (vector<CIRCNetwork*>::iterator it = vNetworks.begin(); it != vNetworks.end(); ++it) {
			const vector<CChan*>& vChans = (*it)->GetChans();

			for (vector<CChan*>::const_iterator it2 = vChans.begin(); it2 != vChans.end(); ++it2) {
				CChan *pChan = *it2;

				// If that channel isn't yet in the config,
				// we'll have to add it...
				if (!pChan->InConfig()) {
					pChan->SetInConfig(true);
					m_bWriteConf = true;
				}
			}
		}
	}

	virtual ~CChanSaverMod() {
	}

	virtual EModRet OnRaw(CString& sLine) {
		if (m_bWriteConf) {
			CZNC::Get().WriteConfig();
			m_bWriteConf = false;
		}

		return CONTINUE;
	}

	virtual void OnMode(const CNick& OpNick, CChan& Channel, char uMode, const CString& sArg, bool bAdded, bool bNoChange) {
		// This is called when we join (ZNC requests the channel modes
		// on join) *and* when someone changes the channel keys.
		// We ignore channel key "*" because of some broken nets.
		if (uMode != 'k' || bNoChange || !bAdded || sArg == "*")
			return;

		Channel.SetKey(sArg);
		m_bWriteConf = true;
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		if (Nick.GetNick() == m_pNetwork->GetIRCNick().GetNick()) {
			Channel.SetInConfig(true);
			CZNC::Get().WriteConfig();
		}
	}

	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {
		if (Nick.GetNick() == m_pNetwork->GetIRCNick().GetNick()) {
			Channel.SetInConfig(false);
			CZNC::Get().WriteConfig();
		}
	}

private:
	bool m_bWriteConf;
};

template<> void TModInfo<CChanSaverMod>(CModInfo& Info) {
	Info.SetWikiPage("chansaver");
	Info.AddType(CModInfo::NetworkModule);
}

USERMODULEDEFS(CChanSaverMod, "Keep config up-to-date when user joins/parts")
