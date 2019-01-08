/*
 * Copyright (C) 2004-2019 ZNC, see the NOTICE file for details.
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
#include <znc/User.h>

class CNickServ : public CModule {
    void DoNickCommand(const CString& sCmd, const CString& sNick) {
        MCString msValues;
        msValues["nickname"] = sNick;
        msValues["password"] = GetNV("Password");
        PutIRC(CString::NamedFormat(GetNV(sCmd), msValues));
    }

  public:
    void SetCommand(const CString& sLine) {
        SetNV("Password", sLine.Token(1, true));
        PutModule(t_s("Password set"));
    }

    void ClearCommand(const CString& sLine) { DelNV("Password"); }

    void SetNSNameCommand(const CString& sLine) {
        SetNV("NickServName", sLine.Token(1, true));
        PutModule(t_s("NickServ name set"));
    }

    void ClearNSNameCommand(const CString& sLine) { DelNV("NickServName"); }

    void ViewCommandsCommand(const CString& sLine) {
        PutModule("IDENTIFY " + GetNV("IdentifyCmd"));
    }

    void SetCommandCommand(const CString& sLine) {
        CString sCmd = sLine.Token(1);
        CString sNewCmd = sLine.Token(2, true);
        if (sCmd.Equals("IDENTIFY")) {
            SetNV("IdentifyCmd", sNewCmd);
        } else {
            PutModule(
                t_s("No such editable command. See ViewCommands for list."));
            return;
        }
        PutModule(t_s("Ok"));
    }

    MODCONSTRUCTOR(CNickServ) {
        AddHelpCommand();
        AddCommand("Set", t_d("password"), t_d("Set your nickserv password"),
                   [=](const CString& sLine) { SetCommand(sLine); });
        AddCommand("Clear", "", t_d("Clear your nickserv password"),
                   [=](const CString& sLine) { ClearCommand(sLine); });
        AddCommand("SetNSName", t_d("nickname"),
                   t_d("Set NickServ name (Useful on networks like EpiKnet, "
                       "where NickServ is named Themis"),
                   [=](const CString& sLine) { SetNSNameCommand(sLine); });
        AddCommand("ClearNSName", "",
                   t_d("Reset NickServ name to default (NickServ)"),
                   [=](const CString& sLine) { ClearNSNameCommand(sLine); });
        AddCommand(
            "ViewCommands", "",
            t_d("Show patterns for lines, which are being sent to NickServ"),
            [=](const CString& sLine) { ViewCommandsCommand(sLine); });
        AddCommand("SetCommand", t_d("cmd new-pattern"),
                   t_d("Set pattern for commands"),
                   [=](const CString& sLine) { SetCommandCommand(sLine); });
    }

    ~CNickServ() override {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        if (!sArgs.empty() && sArgs != "<hidden>") {
            SetNV("Password", sArgs);
            SetArgs("<hidden>");
        }

        if (GetNV("IdentifyCmd").empty()) {
            SetNV("IdentifyCmd", "NICKSERV IDENTIFY {password}");
        }

        return true;
    }

    void HandleMessage(CNick& Nick, const CString& sMessage) {
        CString sNickServName = (!GetNV("NickServName").empty())
                                    ? GetNV("NickServName")
                                    : "NickServ";
        if (!GetNV("Password").empty() && Nick.NickEquals(sNickServName) &&
            (sMessage.find("msg") != CString::npos ||
             sMessage.find("authenticate") != CString::npos ||
             sMessage.find("choose a different nickname") != CString::npos ||
             sMessage.find("please choose a different nick") != CString::npos ||
             sMessage.find("If this is your nick, identify yourself with") !=
                 CString::npos ||
             sMessage.find("If this is your nick, type") != CString::npos ||
             sMessage.find("This is a registered nickname, please identify") !=
                 CString::npos ||
             sMessage.find("is a registered nick - you must auth to account") !=
                 CString::npos ||
             sMessage.StripControls_n().find(
                 "type /NickServ IDENTIFY password") != CString::npos ||
             sMessage.StripControls_n().find(
                 "type /msg NickServ IDENTIFY password") != CString::npos) &&
            sMessage.AsUpper().find("IDENTIFY") != CString::npos &&
            sMessage.find("help") == CString::npos) {
            MCString msValues;
            msValues["password"] = GetNV("Password");
            PutIRC(CString::NamedFormat(GetNV("IdentifyCmd"), msValues));
        }
    }

    EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
        HandleMessage(Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
        HandleMessage(Nick, sMessage);
        return CONTINUE;
    }
};

template <>
void TModInfo<CNickServ>(CModInfo& Info) {
    Info.SetWikiPage("nickserv");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(Info.t_s("Please enter your nickserv password."));
}

NETWORKMODULEDEFS(CNickServ,
                  t_s("Auths you with NickServ (prefer SASL module instead)"))
