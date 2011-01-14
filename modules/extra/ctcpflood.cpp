/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Modules.h"
#include "Chan.h"

class CCtcpFloodMod : public CModule {
public:
	MODCONSTRUCTOR(CCtcpFloodMod) {
		m_tLastCTCP = 0;
		m_iNumCTCP = 0;
	}

	~CCtcpFloodMod() {
	}

	void Save() {
		// We save the settings twice because the module arguments can
		// be more easily edited via webadmin, while the SetNV() stuff
		// survives e.g. /msg *status reloadmod ctcpflood.
		SetNV("secs", CString(m_iThresholdSecs));
		SetNV("msgs", CString(m_iThresholdMsgs));

		SetArgs(CString(m_iThresholdMsgs) + " " + CString(m_iThresholdSecs));
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) {
		m_iThresholdMsgs = sArgs.Token(0).ToUInt();
		m_iThresholdSecs = sArgs.Token(1).ToUInt();

		if (m_iThresholdMsgs == 0 || m_iThresholdSecs == 0) {
			m_iThresholdMsgs = GetNV("msgs").ToUInt();
			m_iThresholdSecs = GetNV("secs").ToUInt();
		}

		if (m_iThresholdSecs == 0)
			m_iThresholdSecs = 2;
		if (m_iThresholdMsgs == 0)
			m_iThresholdMsgs = 4;

		Save();

		return true;
	}

	EModRet Message(const CNick& Nick, const CString& sMessage) {
		// We never block /me, because it doesn't cause a reply
		if (sMessage.Token(0).Equals("ACTION"))
			return CONTINUE;

		if (m_tLastCTCP + m_iThresholdSecs < time(NULL)) {
			m_tLastCTCP = time(NULL);
			m_iNumCTCP = 0;
		}

		m_iNumCTCP++;

		if (m_iNumCTCP < m_iThresholdMsgs)
			return CONTINUE;
		else if (m_iNumCTCP == m_iThresholdMsgs)
			PutModule("Limit reached by [" + Nick.GetHostMask() + "], blocking all CTCP");

		// Reset the timeout so that we continue blocking messages
		m_tLastCTCP = time(NULL);

		return HALT;
	}

	EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) {
		return Message(Nick, sMessage);
	}

	EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) {
		return Message(Nick, sMessage);
	}

	void OnModCommand(const CString& sCommand) {
		const CString& sCmd = sCommand.Token(0);
		const CString& sArg = sCommand.Token(1, true);

		if (sCmd.Equals("secs") && !sArg.empty()) {
			m_iThresholdSecs = sArg.ToUInt();
			if (m_iThresholdSecs == 0)
				m_iThresholdSecs = 1;

			PutModule("Set seconds limit to [" + CString(m_iThresholdSecs) + "]");
			Save();
		} else if (sCmd.Equals("lines") && !sArg.empty()) {
			m_iThresholdMsgs = sArg.ToUInt();
			if (m_iThresholdMsgs == 0)
				m_iThresholdMsgs = 2;

			PutModule("Set lines limit to [" + CString(m_iThresholdMsgs) + "]");
			Save();
		} else if (sCmd.Equals("show")) {
			PutModule("Current limit is " + CString(m_iThresholdMsgs) + " CTCPs "
					"in " + CString(m_iThresholdSecs) + " secs");
		} else {
			PutModule("Commands: show, secs [limit], lines [limit]");
		}
	}

private:
	time_t m_tLastCTCP;
	unsigned int m_iNumCTCP;

	time_t m_iThresholdSecs;
	unsigned int m_iThresholdMsgs;
};

MODULEDEFS(CCtcpFloodMod, "Don't forward CTCP floods to clients")
