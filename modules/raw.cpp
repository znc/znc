/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/Modules.h>

class CRawMod : public CModule {
public:
	MODCONSTRUCTOR(CRawMod) {}
	virtual ~CRawMod() {}

	EModRet OnRaw(CString& sLine) override {
		PutModule("IRC -> [" + sLine + "]");
		return CONTINUE;
	}

	void OnModCommand(const CString& sCommand) override {
		PutIRC(sCommand);
	}

	EModRet OnUserRaw(CString& sLine) override {
		PutModule("YOU -> [" + sLine + "]");
		return CONTINUE;
	}
};

template<> void TModInfo<CRawMod>(CModInfo& Info) {
	Info.SetWikiPage("raw");
	Info.AddType(CModInfo::UserModule);
}

NETWORKMODULEDEFS(CRawMod, "View all of the raw traffic")

