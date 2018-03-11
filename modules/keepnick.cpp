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

#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>

using std::vector;

class CKeepNickMod;

class CKeepNickTimer : public CTimer {
  public:
    CKeepNickTimer(CKeepNickMod* pMod);
    ~CKeepNickTimer() override {}

    void RunJob() override;

  private:
    CKeepNickMod* m_pMod;
};

class CKeepNickMod : public CModule {
  public:
    MODCONSTRUCTOR(CKeepNickMod) {
        AddHelpCommand();
        AddCommand("Enable", "", t_d("Try to get your primary nick"),
                   [=](const CString& sLine) { OnEnableCommand(sLine); });
        AddCommand("Disable", "",
                   t_d("No longer trying to get your primary nick"),
                   [=](const CString& sLine) { OnDisableCommand(sLine); });
        AddCommand("State", "", t_d("Show the current state"),
                   [=](const CString& sLine) { OnStateCommand(sLine); });
    }

    ~CKeepNickMod() override {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        m_pTimer = nullptr;

        // Check if we need to start the timer
        if (GetNetwork()->IsIRCConnected()) OnIRCConnected();

        return true;
    }

    void KeepNick() {
        if (!m_pTimer)
            // No timer means we are turned off
            return;

        CIRCSock* pIRCSock = GetNetwork()->GetIRCSock();

        if (!pIRCSock) return;

        // Do we already have the nick we want?
        if (pIRCSock->GetNick().Equals(GetNick())) return;

        PutIRC("NICK " + GetNick());
    }

    CString GetNick() {
        CString sConfNick = GetNetwork()->GetNick();
        CIRCSock* pIRCSock = GetNetwork()->GetIRCSock();

        if (pIRCSock) sConfNick = sConfNick.Left(pIRCSock->GetMaxNickLen());

        return sConfNick;
    }

    void OnNick(const CNick& Nick, const CString& sNewNick,
                const vector<CChan*>& vChans) override {
        if (sNewNick == GetNetwork()->GetIRCSock()->GetNick()) {
            // We are changing our own nick
            if (Nick.NickEquals(GetNick())) {
                // We are changing our nick away from the conf setting.
                // Let's assume the user wants this and disable
                // this module (to avoid fighting nickserv).
                Disable();
            } else if (sNewNick.Equals(GetNick())) {
                // We are changing our nick to the conf setting,
                // so we don't need that timer anymore.
                Disable();
            }
            return;
        }

        // If the nick we want is free now, be fast and get the nick
        if (Nick.NickEquals(GetNick())) {
            KeepNick();
        }
    }

    void OnQuit(const CNick& Nick, const CString& sMessage,
                const vector<CChan*>& vChans) override {
        // If someone with the nick we want quits, be fast and get the nick
        if (Nick.NickEquals(GetNick())) {
            KeepNick();
        }
    }

    void OnIRCDisconnected() override {
        // No way we can do something if we aren't connected to IRC.
        Disable();
    }

    void OnIRCConnected() override {
        if (!GetNetwork()->GetIRCSock()->GetNick().Equals(GetNick())) {
            // We don't have the nick we want, try to get it
            Enable();
        }
    }

    void Enable() {
        if (m_pTimer) return;

        m_pTimer = new CKeepNickTimer(this);
        AddTimer(m_pTimer);
    }

    void Disable() {
        if (!m_pTimer) return;

        m_pTimer->Stop();
        RemTimer(m_pTimer);
        m_pTimer = nullptr;
    }

    EModRet OnUserRawMessage(CMessage& Message) override {
        // We don't care if we are not connected to IRC
        if (!GetNetwork()->IsIRCConnected()) return CONTINUE;

        // We are trying to get the config nick and this is a /nick?
        if (!m_pTimer || Message.GetType() != CMessage::Type::Nick)
            return CONTINUE;

        // Is the nick change for the nick we are trying to get?
        const CString sNick = Message.As<CNickMessage>().GetNewNick();

        if (!sNick.Equals(GetNick())) return CONTINUE;

        // Indeed trying to change to this nick, generate a 433 for it.
        // This way we can *always* block incoming 433s from the server.
        PutUser(":" + GetNetwork()->GetIRCServer() + " 433 " +
                GetNetwork()->GetIRCNick().GetNick() + " " + sNick + " :" +
                t_s("ZNC is already trying to get this nickname"));
        return CONTINUE;
    }

    EModRet OnNumericMessage(CNumericMessage& msg) override {
        if (m_pTimer) {
            // Are we trying to get our primary nick and we caused this error?
            // :irc.server.net 433 mynick badnick :Nickname is already in use.
            if (msg.GetCode() == 433 && msg.GetParam(1).Equals(GetNick())) {
                return HALT;
            }
            // clang-format off
            // :leguin.freenode.net 435 mynick badnick #chan :Cannot change nickname while banned on channel
            // clang-format on
            if (msg.GetCode() == 435) {
                PutModule(t_f("Unable to obtain nick {1}: {2}, {3}")(
                    msg.GetParam(1), msg.GetParam(3), msg.GetParam(2)));
                Disable();
            }
            // clang-format off
            // :irc1.unrealircd.org 447 mynick :Can not change nickname while on #chan (+N)
            // clang-format on
            if (msg.GetCode() == 447) {
                PutModule(t_f("Unable to obtain nick {1}")(msg.GetParam(1)));
                Disable();
            }
        }

        return CONTINUE;
    }

    void OnEnableCommand(const CString& sCommand) {
        Enable();
        PutModule(t_s("Trying to get your primary nick"));
    }

    void OnDisableCommand(const CString& sCommand) {
        Disable();
        PutModule(t_s("No longer trying to get your primary nick"));
    }

    void OnStateCommand(const CString& sCommand) {
        if (m_pTimer)
            PutModule(t_s("Currently trying to get your primary nick"));
        else
            PutModule(t_s("Currently disabled, try 'enable'"));
    }

  private:
    // If this is nullptr, we are turned off for some reason
    CKeepNickTimer* m_pTimer = nullptr;
};

CKeepNickTimer::CKeepNickTimer(CKeepNickMod* pMod)
    : CTimer(pMod, 30, 0, "KeepNickTimer",
             "Tries to acquire this user's primary nick") {
    m_pMod = pMod;
}

void CKeepNickTimer::RunJob() { m_pMod->KeepNick(); }

template <>
void TModInfo<CKeepNickMod>(CModInfo& Info) {
    Info.SetWikiPage("keepnick");
}

NETWORKMODULEDEFS(CKeepNickMod, t_s("Keeps trying for your primary nick"))
