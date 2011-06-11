/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Chan.h"
#include "User.h"
#include "znc.h"

class CChanSaverMod : public CModule {
public:
	MODCONSTRUCTOR(CChanSaverMod) {
		const vector<CChan*>& vChans = m_pUser->GetChans();
		vector<CChan*>::const_iterator it = vChans.begin();
		vector<CChan*>::const_iterator end = vChans.end();

		m_bWriteConf = false;

		for (; it != end; ++it) {
			CChan *pChan = *it;

			// If that channel isn't yet in the config,
			// we'll have to add it...
			if (!pChan->InConfig()) {
				pChan->SetInConfig(true);
				m_bWriteConf = true;
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
		if (Nick.GetNick() == m_pUser->GetIRCNick().GetNick()) {
			Channel.SetInConfig(true);
			CZNC::Get().WriteConfig();
		}
	}

	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {
		if (Nick.GetNick() == m_pUser->GetIRCNick().GetNick()) {
			Channel.SetInConfig(false);
			CZNC::Get().WriteConfig();
		}
	}

private:
	bool m_bWriteConf;
};

template<> void TModInfo<CChanSaverMod>(CModInfo& Info) {
	Info.SetWikiPage("chansaver");
}

MODULEDEFS(CChanSaverMod, "Keep config up-to-date when user joins/parts")
