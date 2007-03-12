//! @author prozac@rottenboy.com

// @todo handle raw 433 (nick in use)
#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "IRCSock.h"

class CAwayNickMod;

class CAwayNickTimer : public CTimer {
public:
	CAwayNickTimer(CAwayNickMod& Module);

private:
	virtual void RunJob();

private:
	CAwayNickMod&	m_Module;
};

class CBackNickTimer : public CTimer {
public:
	CBackNickTimer(CModule& Module)
		: CTimer(&Module, 3, 1, "BackNickTimer", "Set your nick back when you reattach"),
		  m_Module(Module) {}

private:
	virtual void RunJob() {
		CUser* pUser = m_Module.GetUser();

		if (pUser->IsUserAttached() && pUser->IsIRCConnected()) {
			CString sConfNick = pUser->GetNick();
			m_Module.PutIRC("NICK " + sConfNick);
		}
	}

private:
	CModule&	m_Module;
};

class CAwayNickMod : public CModule {
public:
	MODCONSTRUCTOR(CAwayNickMod) {}

	virtual bool OnLoad(const CString& sArgs) {
		m_sFormat = sArgs;

		if (m_sFormat.empty()) {
			m_sFormat = "zz_%nick%";
		}

		return true;
	}

	virtual ~CAwayNickMod() {
	}

	void StartAwayNickTimer() {
		RemTimer("AwayNickTimer");
		AddTimer(new CAwayNickTimer(*this));
	}

	void StartBackNickTimer() {
		CIRCSock* pIRCSock = m_pUser->GetIRCSock();

		if (pIRCSock) {
			CString sConfNick = m_pUser->GetNick();

			if (pIRCSock->GetNick().CaseCmp(GetAwayNick().Left(pIRCSock->GetNick().length())) == 0) {
				RemTimer("BackNickTimer");
				AddTimer(new CBackNickTimer(*this));
			}
		}
	}

	virtual void OnIRCConnected() {
		if (m_pUser && !m_pUser->IsUserAttached()) {
			StartAwayNickTimer();
		}
	}

	virtual void OnIRCDisconnected() {
		RemTimer("AwayNickTimer");
		RemTimer("BackNickTimer");
	}

	virtual void OnUserAttached() {
		StartBackNickTimer();
	}

	virtual void OnUserDetached() {
		if (!m_pUser->IsUserAttached()) {
			StartAwayNickTimer();
		}
	}

	virtual void OnModCommand(const CString& sCommand) {
		if (strcasecmp(sCommand.c_str(), "TIMERS") == 0) {
			ListTimers();
		} else if (sCommand.Token(0).CaseCmp("SET") == 0) {
			CString sFormat(sCommand.Token(1));

			if (!sFormat.empty()) {
				m_sFormat = sFormat;
			}

			if (m_pUser) {
				CString sExpanded = GetAwayNick();
				CString sMsg = "AwayNick is set to [" + m_sFormat + "]";

				if (m_sFormat != sExpanded) {
					sMsg += " (" + sExpanded + ")";
				}

				PutModule(sMsg);
			}
		} else if (sCommand.Token(0).CaseCmp("SHOW") == 0) {
			if (m_pUser) {
				CString sExpanded = GetAwayNick();
				CString sMsg = "AwayNick is set to [" + m_sFormat + "]";

				if (m_sFormat != sExpanded) {
					sMsg += " (" + sExpanded + ")";
				}

				PutModule(sMsg);
			}
		} else if(sCommand.Token(0).CaseCmp("HELP") == 0) {
			PutModule("Commands are: show, timers, set [awaynick]");
		}
	}

	CString GetAwayNick() {
		unsigned int uLen = 9;
		CIRCSock* pIRCSock = m_pUser->GetIRCSock();

		if (pIRCSock) {
			uLen = pIRCSock->GetMaxNickLen();
		}

		return m_pUser->ExpandString(m_sFormat).Left(uLen);
	}

private:
	CString		m_sFormat;
};

CAwayNickTimer::CAwayNickTimer(CAwayNickMod& Module)
	: CTimer(&Module, 30, 1, "AwayNickTimer", "Set your nick while you're detached"),
	  m_Module(Module) {}

void CAwayNickTimer::RunJob() {
	CUser* pUser = m_Module.GetUser();

	if (!pUser->IsUserAttached() && pUser->IsIRCConnected()) {
		m_Module.PutIRC("NICK " + m_Module.GetAwayNick());
	}
}

MODULEDEFS(CAwayNickMod, "Change your nick while you are away")
