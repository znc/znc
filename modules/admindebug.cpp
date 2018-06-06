/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
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

#include <znc/FileUtils.h>
#include <znc/Server.h>
#include <znc/IRCNetwork.h>
#include <znc/User.h>
#include <znc/ZNCDebug.h>

class CAdminDebugMod : public CModule {
  public:
    MODCONSTRUCTOR(CAdminDebugMod) {
        AddHelpCommand();
        AddCommand("Enable", "", t_d("Enable Debug Mode"),
                   [=](const CString& sLine) { CommandEnable(sLine); });
        AddCommand("Disable", "", t_d("Disable Debug Mode"),
                   [=](const CString& sLine) { CommandDisable(sLine); });
        AddCommand("Status", "", t_d("Show the Debug Mode status"),
                   [=](const CString& sLine) { CommandStatus(sLine); });
    }

    void CommandEnable(const CString& sCommand) {
        if (GetUser()->IsAdmin() == false) {
            PutModule(t_s("Access denied!"));
            return;
        }

        ToggleDebug(true, GetUser()->GetNick());
    }

    void CommandDisable(const CString& sCommand) {
        if (GetUser()->IsAdmin() == false) {
            PutModule(t_s("Access denied!"));
            return;
        }

        ToggleDebug(false, GetNV("enabled_by"));
    }

    bool ToggleDebug(bool bEnable, CString sEnabledBy) {
        bool bValue = CDebug::Debug();

        if (bEnable == bValue) {
            if (bEnable) {
                PutModule(t_s("Already enabled."));
            } else {
                PutModule(t_s("Already disabled."));
            }
            return false;
        }

        CDebug::SetDebug(bEnable);
        CString sEnabled = CString(bEnable ? "on" : "off");
        CZNC::Get().Broadcast(CString(
            "An administrator has just turned Debug Mode \02" + sEnabled + "\02. It was enabled by \02" + sEnabledBy + "\02."
        ));
        if (bEnable) {
            CZNC::Get().Broadcast(
                CString("Messages, credentials, and other sensitive data may"
                " become exposed to the host during this period.")
            );
            SetNV("enabled_by", sEnabledBy);
        } else {
            DelNV("enabled_by");
        }

        return true;
    }

    void CommandStatus(const CString& sCommand) {
        if (CDebug::Debug()) {
            PutModule(t_s("Debugging mode is \02on\02."));
        } else {
             PutModule(t_s("Debugging mode is \02off\02."));
        }
        PutModule(t_s("Logging to: \02stdout\02."));
    }
};

template <>
void TModInfo<CAdminDebugMod>(CModInfo& Info) {
    Info.SetWikiPage("admindebug");
}

GLOBALMODULEDEFS(CAdminDebugMod, t_s("Enable Debug mode dynamically."))
