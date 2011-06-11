/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "User.h"

class CRawMod : public CModule {
public:
	MODCONSTRUCTOR(CRawMod) {}
	virtual ~CRawMod() {}

	virtual EModRet OnRaw(CString& sLine) {
		PutModule("IRC -> [" + sLine + "]");
		return CONTINUE;
	}

	virtual void OnModCommand(const CString& sCommand) {
		PutIRC(sCommand);
	}

	virtual EModRet OnUserRaw(CString& sLine) {
		PutModule("YOU -> [" + sLine + "]");
		return CONTINUE;
	}
};

template<> void TModInfo<CRawMod>(CModInfo& Info) {
	Info.SetWikiPage("raw");
}

MODULEDEFS(CRawMod, "View all of the raw traffic")

