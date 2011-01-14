/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Modules.h"

class CPreventIdMsgMod : public CModule {
public:
	MODCONSTRUCTOR(CPreventIdMsgMod) {}

	virtual EModRet OnUserRaw(CString& sLine) {
		if (sLine.Token(0).AsLower() == "capab") {
			if (sLine.AsLower().find("identify-msg") != CString::npos
					|| sLine.AsLower().find("identify-ctcp") != CString::npos)
				return HALTCORE;
		}
		return CONTINUE;
	}
};

MODULEDEFS(CPreventIdMsgMod, "Prevent client from sending IDENTIFY-MSG to server")
