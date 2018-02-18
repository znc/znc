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

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>

#define STICKYCHAN_TIMER_INTERVAL 60

using std::vector;

class CStickyChan : public CModule {
  public:
    MODCONSTRUCTOR(CStickyChan) {
        AddHelpCommand();
        AddCommand("Stick", t_d("<#channel> [key]"), t_d("Sticks a channel"),
                   [=](const CString& sLine) { OnStickCommand(sLine); });
        AddCommand("Unstick", t_d("<#channel>"), t_d("Unsticks a channel"),
                   [=](const CString& sLine) { OnUnstickCommand(sLine); });
        AddCommand("List", "", t_d("Lists sticky channels"),
                   [=](const CString& sLine) { OnListCommand(sLine); });
    }
    ~CStickyChan() override {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override;

    EModRet OnUserPart(CString& sChannel, CString& sMessage) override {
        if (!GetNetwork()) {
            return CONTINUE;
        }

        for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
            if (sChannel.Equals(it->first)) {
                CChan* pChan = GetNetwork()->FindChan(sChannel);

                if (pChan) {
                    pChan->JoinUser();
                    return HALT;
                }
            }
        }

        return CONTINUE;
    }

    void OnMode(const CNick& pOpNick, CChan& Channel, char uMode,
                const CString& sArg, bool bAdded, bool bNoChange) override {
        if (uMode == CChan::M_Key) {
            if (bAdded) {
                // We ignore channel key "*" because of some broken nets.
                if (sArg != "*") {
                    SetNV(Channel.GetName(), sArg, true);
                }
            } else {
                SetNV(Channel.GetName(), "", true);
            }
        }
    }

    void OnStickCommand(const CString& sCommand) {
        CString sChannel = sCommand.Token(1).AsLower();
        if (sChannel.empty()) {
            PutModule(t_s("Usage: Stick <#channel> [key]"));
            return;
        }
        SetNV(sChannel, sCommand.Token(2), true);
        PutModule(t_f("Stuck {1}")(sChannel));
    }

    void OnUnstickCommand(const CString& sCommand) {
        CString sChannel = sCommand.Token(1);
        if (sChannel.empty()) {
            PutModule(t_s("Usage: Unstick <#channel>"));
            return;
        }
        DelNV(sChannel, true);
        PutModule(t_f("Unstuck {1}")(sChannel));
    }

    void OnListCommand(const CString& sCommand) {
        int i = 1;
        for (MCString::iterator it = BeginNV(); it != EndNV(); ++it, i++) {
            if (it->second.empty())
                PutModule(CString(i) + ": " + it->first);
            else
                PutModule(CString(i) + ": " + it->first + " (" + it->second +
                          ")");
        }
        PutModule(t_s(" -- End of List"));
    }

    void RunJob() {
        CIRCNetwork* pNetwork = GetNetwork();
        if (!pNetwork->GetIRCSock()) return;

        for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
            CChan* pChan = pNetwork->FindChan(it->first);
            if (!pChan) {
                pChan = new CChan(it->first, pNetwork, true);
                if (!it->second.empty()) pChan->SetKey(it->second);
                if (!pNetwork->AddChan(pChan)) {
                    /* AddChan() deleted that channel */
                    PutModule(t_f("Could not join {1} (# prefix missing?)")(
                        it->first));
                    continue;
                }
            }
            if (!pChan->IsOn() && pNetwork->IsIRCConnected()) {
                PutModule("Joining [" + pChan->GetName() + "]");
                PutIRC("JOIN " + pChan->GetName() +
                       (pChan->GetKey().empty() ? "" : " " + pChan->GetKey()));
            }
        }
    }

    CString GetWebMenuTitle() override { return t_s("Sticky Channels"); }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        if (sPageName == "index") {
            bool bSubmitted = (WebSock.GetParam("submitted").ToInt() != 0);

            const vector<CChan*>& Channels = GetNetwork()->GetChans();
            for (CChan* pChan : Channels) {
                const CString sChan = pChan->GetName();
                bool bStick = FindNV(sChan) != EndNV();

                if (bSubmitted) {
                    bool bNewStick =
                        WebSock.GetParam("stick_" + sChan).ToBool();
                    if (bNewStick && !bStick)
                        SetNV(sChan, "");  // no password support for now unless
                                           // chansaver is active too
                    else if (!bNewStick && bStick) {
                        MCString::iterator it = FindNV(sChan);
                        if (it != EndNV()) DelNV(it);
                    }
                    bStick = bNewStick;
                }

                CTemplate& Row = Tmpl.AddRow("ChannelLoop");
                Row["Name"] = sChan;
                Row["Sticky"] = CString(bStick);
            }

            if (bSubmitted) {
                WebSock.GetSession()->AddSuccess(
                    t_s("Changes have been saved!"));
            }

            return true;
        }

        return false;
    }

    bool OnEmbeddedWebRequest(CWebSock& WebSock, const CString& sPageName,
                              CTemplate& Tmpl) override {
        if (sPageName == "webadmin/channel") {
            CString sChan = Tmpl["ChanName"];
            bool bStick = FindNV(sChan) != EndNV();
            if (Tmpl["WebadminAction"].Equals("display")) {
                Tmpl["Sticky"] = CString(bStick);
            } else if (WebSock.GetParam("embed_stickychan_presented")
                           .ToBool()) {
                bool bNewStick =
                    WebSock.GetParam("embed_stickychan_sticky").ToBool();
                if (bNewStick && !bStick) {
                    // no password support for now unless chansaver is active
                    // too
                    SetNV(sChan, "");
                    WebSock.GetSession()->AddSuccess(
                        t_s("Channel became sticky!"));
                } else if (!bNewStick && bStick) {
                    DelNV(sChan);
                    WebSock.GetSession()->AddSuccess(
                        t_s("Channel stopped being sticky!"));
                }
            }
            return true;
        }
        return false;
    }

    EModRet OnNumericMessage(CNumericMessage& msg) override {
        if (msg.GetCode() == 479) {
            // ERR_BADCHANNAME (juped channels or illegal channel name - ircd
            // hybrid)
            // prevent the module from getting into an infinite loop of trying
            // to join it.
            // :irc.network.net 479 mynick #channel :Illegal channel name

            const CString sChannel = msg.GetParam(1);
            for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
                if (sChannel.Equals(it->first)) {
                    PutModule(
                        t_f("Channel {1} cannot be joined, it is an illegal "
                            "channel name. Unsticking.")(sChannel));
                    OnUnstickCommand("unstick " + sChannel);
                    return CONTINUE;
                }
            }
        }

        return CONTINUE;
    }
};

static void RunTimer(CModule* pModule, CFPTimer* pTimer) {
    ((CStickyChan*)pModule)->RunJob();
}

bool CStickyChan::OnLoad(const CString& sArgs, CString& sMessage) {
    VCString vsChans;
    sArgs.Split(",", vsChans, false);

    for (const CString& s : vsChans) {
        CString sChan = s.Token(0);
        CString sKey = s.Token(1, true);
        SetNV(sChan, sKey);
    }

    // Since we now have these channels added, clear the argument list
    SetArgs("");

    AddTimer(RunTimer, "StickyChanTimer", STICKYCHAN_TIMER_INTERVAL);
    return (true);
}

template <>
void TModInfo<CStickyChan>(CModInfo& Info) {
    Info.SetWikiPage("stickychan");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(Info.t_s("List of channels, separated by comma."));
}

NETWORKMODULEDEFS(
    CStickyChan,
    t_s("configless sticky chans, keeps you there very stickily even"))
