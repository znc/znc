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

class CPerform : public CModule {
    void Add(const CString& sCommand) {
        CString sPerf = sCommand.Token(1, true);

        if (sPerf.empty()) {
            PutModule(t_s("Usage: add <command>"));
            return;
        }

        m_vPerform.push_back(ParsePerform(sPerf));
        PutModule(t_s("Added!"));
        Save();
    }

    void Del(const CString& sCommand) {
        u_int iNum = sCommand.Token(1, true).ToUInt();

        if (iNum > m_vPerform.size() || iNum <= 0) {
            PutModule(t_s("Illegal # Requested"));
            return;
        } else {
            m_vPerform.erase(m_vPerform.begin() + iNum - 1);
            PutModule(t_s("Command Erased."));
        }
        Save();
    }

    void List(const CString& sCommand) {
        CTable Table;
        unsigned int index = 1;

        Table.AddColumn(t_s("Id", "list"));
        Table.AddColumn(t_s("Perform", "list"));
        Table.AddColumn(t_s("Expanded", "list"));

        for (const CString& sPerf : m_vPerform) {
            Table.AddRow();
            Table.SetCell(t_s("Id", "list"), CString(index++));
            Table.SetCell(t_s("Perform", "list"), sPerf);

            CString sExpanded = ExpandString(sPerf);

            if (sExpanded != sPerf) {
                Table.SetCell(t_s("Expanded", "list"), sExpanded);
            }
        }

        if (PutModule(Table) == 0) {
            PutModule(t_s("No commands in your perform list."));
        }
    }

    void Execute(const CString& sCommand) {
        OnIRCConnected();
        PutModule(t_s("perform commands sent"));
    }

    void Swap(const CString& sCommand) {
        u_int iNumA = sCommand.Token(1).ToUInt();
        u_int iNumB = sCommand.Token(2).ToUInt();

        if (iNumA > m_vPerform.size() || iNumA <= 0 ||
            iNumB > m_vPerform.size() || iNumB <= 0) {
            PutModule(t_s("Illegal # Requested"));
        } else {
            std::iter_swap(m_vPerform.begin() + (iNumA - 1),
                           m_vPerform.begin() + (iNumB - 1));
            PutModule(t_s("Commands Swapped."));
            Save();
        }
    }

  public:
    MODCONSTRUCTOR(CPerform) {
        AddHelpCommand();
        AddCommand(
            "Add", t_d("<command>"),
            t_d("Adds perform command to be sent to the server on connect"),
            [=](const CString& sLine) { Add(sLine); });
        AddCommand("Del", t_d("<number>"), t_d("Delete a perform command"),
                   [=](const CString& sLine) { Del(sLine); });
        AddCommand("List", "", t_d("List the perform commands"),
                   [=](const CString& sLine) { List(sLine); });
        AddCommand("Execute", "",
                   t_d("Send the perform commands to the server now"),
                   [=](const CString& sLine) { Execute(sLine); });
        AddCommand("Swap", t_d("<number> <number>"),
                   t_d("Swap two perform commands"),
                   [=](const CString& sLine) { Swap(sLine); });
    }

    ~CPerform() override {}

    CString ParsePerform(const CString& sArg) const {
        CString sPerf = sArg;

        if (sPerf.Left(1) == "/") sPerf.LeftChomp();

        if (sPerf.Token(0).Equals("MSG")) {
            sPerf = "PRIVMSG " + sPerf.Token(1, true);
        }

        if ((sPerf.Token(0).Equals("PRIVMSG") ||
             sPerf.Token(0).Equals("NOTICE")) &&
            sPerf.Token(2).Left(1) != ":") {
            sPerf = sPerf.Token(0) + " " + sPerf.Token(1) + " :" +
                    sPerf.Token(2, true);
        }

        return sPerf;
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        GetNV("Perform").Split("\n", m_vPerform, false);

        return true;
    }

    void OnIRCConnected() override {
        for (const CString& sPerf : m_vPerform) {
            PutIRC(ExpandString(sPerf));
        }
    }

    CString GetWebMenuTitle() override { return t_s("Perform"); }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        if (sPageName != "index") {
            // only accept requests to index
            return false;
        }

        if (WebSock.IsPost()) {
            VCString vsPerf;
            WebSock.GetRawParam("perform", true).Split("\n", vsPerf, false);
            m_vPerform.clear();

            for (const CString& sPerf : vsPerf)
                m_vPerform.push_back(ParsePerform(sPerf));

            Save();
        }

        for (const CString& sPerf : m_vPerform) {
            CTemplate& Row = Tmpl.AddRow("PerformLoop");
            Row["Perform"] = sPerf;
        }

        return true;
    }

  private:
    void Save() {
        CString sBuffer = "";

        for (const CString& sPerf : m_vPerform) {
            sBuffer += sPerf + "\n";
        }
        SetNV("Perform", sBuffer);
    }

    VCString m_vPerform;
};

template <>
void TModInfo<CPerform>(CModInfo& Info) {
    Info.AddType(CModInfo::UserModule);
    Info.SetWikiPage("perform");
}

NETWORKMODULEDEFS(
    CPerform,
    t_s("Keeps a list of commands to be executed when ZNC connects to IRC."))
