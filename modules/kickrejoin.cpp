/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
 * This was originally written by cycomate.
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

/*
 * Autorejoin module
 * rejoin channel (after a delay) when kicked
 * Usage: LoadModule = rejoin [delay]
 *
 */

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>

class CRejoinJob : public CTimer {
  public:
    CRejoinJob(CModule* pModule, unsigned int uInterval, unsigned int uCycles,
               const CString& sLabel, const CString& sDescription)
        : CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}

    ~CRejoinJob() override {}

  protected:
    void RunJob() override {
        CIRCNetwork* pNetwork = GetModule()->GetNetwork();
        CChan* pChan = pNetwork->FindChan(GetName().Token(1, true));

        if (pChan) {
            pChan->Enable();
            GetModule()->PutIRC("JOIN " + pChan->GetName() + " " +
                                pChan->GetKey());
        }
    }
};

class CRejoinMod : public CModule {
  private:
    unsigned int delay = 10;

  public:
    MODCONSTRUCTOR(CRejoinMod) {
        AddHelpCommand();
        AddCommand("SetDelay", t_d("<secs>"), t_d("Set the rejoin delay"),
                   [=](const CString& sLine) { OnSetDelayCommand(sLine); });
        AddCommand("ShowDelay", "", t_d("Show the rejoin delay"),
                   [=](const CString& sLine) { OnShowDelayCommand(sLine); });
    }
    ~CRejoinMod() override {}

    bool OnLoad(const CString& sArgs, CString& sErrorMsg) override {
        if (sArgs.empty()) {
            CString sDelay = GetNV("delay");

            if (sDelay.empty())
                delay = 10;
            else
                delay = sDelay.ToUInt();
        } else {
            int i = sArgs.ToInt();
            if ((i == 0 && sArgs == "0") || i > 0)
                delay = i;
            else {
                sErrorMsg =
                    t_s("Illegal argument, must be a positive number or 0");
                return false;
            }
        }

        return true;
    }

    void OnSetDelayCommand(const CString& sCommand) {
        int i;
        i = sCommand.Token(1).ToInt();

        if (i < 0) {
            PutModule(t_s("Negative delays don't make any sense!"));
            return;
        }

        delay = i;
        SetNV("delay", CString(delay));

        if (delay)
            PutModule(t_p("Rejoin delay set to 1 second",
                          "Rejoin delay set to {1} seconds", delay)(delay));
        else
            PutModule(t_s("Rejoin delay disabled"));
    }

    void OnShowDelayCommand(const CString& sCommand) {
        if (delay)
            PutModule(t_p("Rejoin delay is set to 1 second",
                          "Rejoin delay is set to {1} seconds", delay)(delay));
        else
            PutModule(t_s("Rejoin delay is disabled"));
    }

    void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& pChan,
                const CString& sMessage) override {
        if (GetNetwork()->GetCurNick().Equals(sKickedNick)) {
            if (!delay) {
                PutIRC("JOIN " + pChan.GetName() + " " + pChan.GetKey());
                pChan.Enable();
                return;
            }
            AddTimer(new CRejoinJob(this, delay, 1, "Rejoin " + pChan.GetName(),
                                    "Rejoin channel after a delay"));
        }
    }
};

template <>
void TModInfo<CRejoinMod>(CModInfo& Info) {
    Info.SetWikiPage("kickrejoin");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(Info.t_s(
        "You might enter the number of seconds to wait before rejoining."));
}

NETWORKMODULEDEFS(CRejoinMod, t_s("Autorejoins on kick"))
