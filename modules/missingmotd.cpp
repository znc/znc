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

class CMissingMotd : public CModule {
public:
	MODCONSTRUCTOR(CMissingMotd) {}

	void OnClientLogin() override {
		PutUser(":irc.znc.in 422 :MOTD File is missing");
	}
};

template<> void TModInfo<CMissingMotd>(CModInfo& Info)
{
    Info.SetWikiPage("missingmotd");
    Info.SetHasArgs(false);
}

USERMODULEDEFS(CMissingMotd, "Sends 422 to clients when they login")

