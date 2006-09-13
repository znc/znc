//! @author prozac@rottenboy.com

#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"

class CRawMod : public CModule {
public:
	MODCONSTRUCTOR(CRawMod) {}
	virtual ~CRawMod() {}

	virtual EModRet OnRaw(CString& sLine) {
		PutModule("IRC -> [" + sLine + "]");
		return CONTINUE;
	}

	virtual void OnModCommand(const CString& sCommand) {
		m_pUser->PutUser(sCommand);
	}

	virtual EModRet OnUserRaw(CString& sLine) {
		PutModule("YOU -> [" + sLine + "]");
		return CONTINUE;
	}
};

MODULEDEFS(CRawMod, "View all of the raw traffic")

