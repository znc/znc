/*
 * Copyright (C) 2004-2008  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "main.h"
#include "znc.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"

class CChanSaverMod : public CModule {
public:
	MODCONSTRUCTOR(CChanSaverMod) {
		m_bWriteConf = false;
	}

	virtual ~CChanSaverMod() {
	}

	virtual EModRet OnRaw(CString& sLine) {
		if (m_bWriteConf) {
			CZNC::Get().WriteConfig();
			m_bWriteConf = false;
		}

		if (sLine.Token(1) == "324" && sLine.Token(4).find("k") != CString::npos) {
			CChan* pChan = m_pUser->FindChan(sLine.Token(3));

			if (pChan) {
				pChan->SetInConfig(true);
				m_bWriteConf = true;
			}
		}

		return CONTINUE;
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		if (Nick.GetNick() == m_pUser->GetIRCNick().GetNick()) {
			Channel.SetInConfig(true);
			CZNC::Get().WriteConfig();
		}
	}

	virtual void OnPart(const CNick& Nick, CChan& Channel) {
		if (Nick.GetNick() == m_pUser->GetIRCNick().GetNick()) {
			Channel.SetInConfig(false);
			CZNC::Get().WriteConfig();
		}
	}

private:
	bool	m_bWriteConf;
};

MODULEDEFS(CChanSaverMod, "Keep config up-to-date when user joins/parts")
