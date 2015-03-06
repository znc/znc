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

class CStripControlsMod : public CModule {
public:
	MODCONSTRUCTOR(CStripControlsMod) {}

	EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) override {
		sMessage.StripControls();
		return CONTINUE;
	}

	EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) override {
		sMessage.StripControls();
		return CONTINUE;
	}

	EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
		sMessage.StripControls();
		return CONTINUE;
	}

	EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) override {
		sMessage.StripControls();
		return CONTINUE;
	}

	EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
		sMessage.StripControls();
		return CONTINUE;
	}

	EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
		sMessage.StripControls();
		return CONTINUE;
	}
};

template<> void TModInfo<CStripControlsMod>(CModInfo& Info) {
	Info.SetWikiPage("stripcontrols");
	Info.AddType(CModInfo::UserModule);
}

NETWORKMODULEDEFS(CStripControlsMod, "Strips control codes (Colors, Bold, ..) from channel and private messages.")
