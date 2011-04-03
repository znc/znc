/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Modules.h"
#include "Client.h"
#include "FileUtils.h"

class CMotdFileMod : public CGlobalModule {
public:
	GLOBALMODCONSTRUCTOR(CMotdFileMod) {}
	virtual ~CMotdFileMod() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (sArgs.empty()) {
			sMessage = "Argument must be path to a MOTD file";
			return false;
		}
		return true;
	}

	virtual void OnClientLogin() {
		CString sFile = GetArgs();
		CString sBuffer;
		CFile cFile(sFile);
		if (!cFile.Open(O_RDONLY)) {
			m_pClient->PutStatusNotice("Could not open MOTD file");
			return;
		}
		while (cFile.ReadLine(sBuffer))
			m_pClient->PutStatusNotice(sBuffer);
		cFile.Close();
	}
};

GLOBALMODULEDEFS(CMotdFileMod, "Send MOTD from a file")
