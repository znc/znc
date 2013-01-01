/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/IRCNetwork.h>
#include <znc/Chan.h>
#include <znc/Modules.h>

using std::vector;

class CClearBufferOnMsgMod : public CModule {
public:
	MODCONSTRUCTOR(CClearBufferOnMsgMod) {}

	void ClearAllBuffers() {
		if (m_pNetwork) {
			const vector<CChan*>& vChans = m_pNetwork->GetChans();
			vector<CChan*>::const_iterator it;

			for (it = vChans.begin(); it != vChans.end(); ++it) {
				// Skip detached channels, they weren't read yet
				if ((*it)->IsDetached())
					continue;

				(*it)->ClearBuffer();
				// We deny AutoClearChanBuffer on all channels since this module
				// doesn't make any sense with it
				(*it)->SetAutoClearChanBuffer(false);
			}
		}
	}

	virtual EModRet OnUserMsg(CString& sTarget, CString& sMessage) {
		ClearAllBuffers();
		return CONTINUE;
	}

	virtual EModRet OnUserCTCP(CString& sTarget, CString& sMessage) {
		ClearAllBuffers();
		return CONTINUE;
	}

	virtual EModRet OnUserAction(CString& sTarget, CString& sMessage) {
		ClearAllBuffers();
		return CONTINUE;
	}

	virtual EModRet OnUserNotice(CString& sTarget, CString& sMessage) {
		ClearAllBuffers();
		return CONTINUE;
	}

	virtual EModRet OnUserPart(CString& sChannel, CString& sMessage) {
		ClearAllBuffers();
		return CONTINUE;
	}

	virtual EModRet OnUserTopic(CString& sChannel, CString& sTopic) {
		ClearAllBuffers();
		return CONTINUE;
	}
};

template<> void TModInfo<CClearBufferOnMsgMod>(CModInfo& Info) {
	Info.SetWikiPage("clearbufferonmsg");
}

USERMODULEDEFS(CClearBufferOnMsgMod, "Clear all channel buffers whenever the user does something")
