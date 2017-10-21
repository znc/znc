/*
 * Copyright (C) 2004-2017 ZNC, see the NOTICE file for details.
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
#include <znc/IRCNetwork.h>

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
        PutModule("Password set");
    }

    void ClearCommand(const CString& sLine) { DelNV("Password"); }

    void SetNSNameCommand(const CString& sLine) {
        SetNV("NickServName", sLine.Token(1, true));
        PutModule("NickServ name set");
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
            PutModule("No such editable command. See ViewCommands for list.");
            return;
        }
        PutModule("Ok");
    }

    void SetJoinAfterIdentifiedCommand(const CString& sLine) {
        if(sLine.Token(1, true).ToBool()) {
            SetNV("JoinAfterIdentified", "true");
            m_bJoinAfterIdentified = true;
            PutModule("Channels will be joined after identification.");
        } else {
            SetNV("JoinAfterIdentified", "false");
            m_bJoinAfterIdentified = false;
            PutModule("Channels will be joined immediately.");
        }
    }

    MODCONSTRUCTOR(CNickServ) {
        AddHelpCommand();
        AddCommand("Set",
                   static_cast<CModCommand::ModCmdFunc>(&CNickServ::SetCommand),
                   "password");
        AddCommand("Clear", static_cast<CModCommand::ModCmdFunc>(
                                &CNickServ::ClearCommand),
                   "", "Clear your nickserv password");
        AddCommand("SetNSName", static_cast<CModCommand::ModCmdFunc>(
                                    &CNickServ::SetNSNameCommand),
                   "nickname",
                   "Set NickServ name (Useful on networks like EpiKnet, where "
                   "NickServ is named Themis)");
        AddCommand("ClearNSName", static_cast<CModCommand::ModCmdFunc>(
                                      &CNickServ::ClearNSNameCommand),
                   "", "Reset NickServ name to default (NickServ)");
        AddCommand("ViewCommands", static_cast<CModCommand::ModCmdFunc>(
                                       &CNickServ::ViewCommandsCommand),
                   "",
                   "Show patterns for lines, which are being sent to NickServ");
        AddCommand("SetCommand", static_cast<CModCommand::ModCmdFunc>(
                                     &CNickServ::SetCommandCommand),
                   "cmd new-pattern", "Set pattern for commands");
        AddCommand("SetJoinAfterIdentified", static_cast<CModCommand::ModCmdFunc>(
                                    &CNickServ::SetJoinAfterIdentifiedCommand),
                   "true/false",
                   "Set whether to join channels after NickServ Identification or instantly after connecting");
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

        CString sTmp;
        m_bJoinAfterIdentified = (sTmp = GetNV("JoinAfterIdentified")).empty() ? false : sTmp.ToBool();

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
        } else if(Nick.NickEquals(sNickServName) && m_bJoinAfterIdentified &&
                 (sMessage.find("Password accepted") != CString::npos ||
                  sMessage.find("now recognized") != CString::npos ||
                  sMessage.find("now identified") != CString::npos ||
                  sMessage.find("now logged in as") != CString::npos)) {
            m_bIdentified = true;
            GetNetwork()->JoinChans();
        }
    }

    EModRet OnJoining(CChan& Channel) override {
        if (!m_bIdentified && m_bJoinAfterIdentified)
            return HALT;
        return CONTINUE;
    }

    EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
        HandleMessage(Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
        HandleMessage(Nick, sMessage);
        return CONTINUE;
    }

    void OnIRCDisconnected() override {
        m_bIdentified = false;
    }
  private:
    bool m_bIdentified{};
    bool m_bJoinAfterIdentified{};
};

template <>
void TModInfo<CNickServ>(CModInfo& Info) {
    Info.SetWikiPage("nickserv");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText("Please enter your nickserv password.");
}

NETWORKMODULEDEFS(CNickServ, "Auths you with NickServ")
