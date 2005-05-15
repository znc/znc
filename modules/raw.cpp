#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"

class CRawMod : public CModule {
public:
	MODCONSTRUCTOR(CRawMod) {}
	virtual ~CRawMod() {}

	virtual CString GetDescription() {
		return "View all of the raw traffic.";
	}

	virtual EModRet OnRaw(CString& sLine) {
		PutModule("IRC -> [" + sLine + "]");
		return CONTINUE;
	}

	virtual EModRet OnUserRaw(CString& sLine) {
		PutModule("YOU -> [" + sLine + "]");
		return CONTINUE;
	}
};

MODULEDEFS(CRawMod)

