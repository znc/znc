#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"

class CRawMod : public CModule {
public:
	MODCONSTRUCTOR(CRawMod) {}
	virtual ~CRawMod() {}

	virtual string GetDescription() {
		return "View all of the raw traffic.";
	}

	virtual bool OnRaw(string& sLine) {
		PutModule("IRC -> [" + sLine + "]");
		return false;
	}

	virtual bool OnUserRaw(string& sLine) {
		PutModule("YOU -> [" + sLine + "]");
		return false;
	}
};

MODULEDEFS(CRawMod)

