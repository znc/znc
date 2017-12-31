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
#include <znc/IRCSock.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>

class CIdentFileModule : public CModule {
    CString m_sOrigISpoof;
    CFile* m_pISpoofLockFile;
    CIRCSock* m_pIRCSock;

  public:
    MODCONSTRUCTOR(CIdentFileModule) {
        AddHelpCommand();
        AddCommand("GetFile", "", t_d("Show file name"),
                   [=](const CString& sLine) { GetFile(sLine); });
        AddCommand("SetFile", t_d("<file>"), t_d("Set file name"),
                   [=](const CString& sLine) { SetFile(sLine); });
        AddCommand("GetFormat", "", t_d("Show file format"),
                   [=](const CString& sLine) { GetFormat(sLine); });
        AddCommand("SetFormat", t_d("<format>"), t_d("Set file format"),
                   [=](const CString& sLine) { SetFormat(sLine); });
        AddCommand("Show", "", t_d("Show current state"),
                   [=](const CString& sLine) { Show(sLine); });

        m_pISpoofLockFile = nullptr;
        m_pIRCSock = nullptr;
    }

    ~CIdentFileModule() override { ReleaseISpoof(); }

    void GetFile(const CString& sLine) {
        PutModule(t_f("File is set to: {1}")(GetNV("File")));
    }

    void SetFile(const CString& sLine) {
        SetNV("File", sLine.Token(1, true));
        PutModule(t_f("File has been set to: {1}")(GetNV("File")));
    }

    void SetFormat(const CString& sLine) {
        SetNV("Format", sLine.Token(1, true));
        PutModule(t_f("Format has been set to: {1}")(GetNV("Format")));
        PutModule(t_f("Format would be expanded to: {1}")(
            ExpandString(GetNV("Format"))));
    }

    void GetFormat(const CString& sLine) {
        PutModule(t_f("Format is set to: {1}")(GetNV("Format")));
        PutModule(t_f("Format would be expanded to: {1}")(
            ExpandString(GetNV("Format"))));
    }

    void Show(const CString& sLine) {
        PutModule("m_pISpoofLockFile = " +
                  CString((long long)m_pISpoofLockFile));
        PutModule("m_pIRCSock = " + CString((long long)m_pIRCSock));
        if (m_pIRCSock) {
            PutModule("user/network - " +
                      m_pIRCSock->GetNetwork()->GetUser()->GetUserName() + "/" +
                      m_pIRCSock->GetNetwork()->GetName());
        } else {
            PutModule(t_s("identfile is free"));
        }
    }

    void OnModCommand(const CString& sCommand) override {
        if (GetUser()->IsAdmin()) {
            HandleCommand(sCommand);
        } else {
            PutModule(t_s("Access denied"));
        }
    }

    void SetIRCSock(CIRCSock* pIRCSock) {
        if (m_pIRCSock) {
            CZNC::Get().ResumeConnectQueue();
        }

        m_pIRCSock = pIRCSock;

        if (m_pIRCSock) {
            CZNC::Get().PauseConnectQueue();
        }
    }

    bool WriteISpoof() {
        if (m_pISpoofLockFile != nullptr) {
            return false;
        }

        m_pISpoofLockFile = new CFile;
        if (!m_pISpoofLockFile->TryExLock(GetNV("File"), O_RDWR | O_CREAT)) {
            delete m_pISpoofLockFile;
            m_pISpoofLockFile = nullptr;
            return false;
        }

        char buf[1024];
        memset((char*)buf, 0, 1024);
        m_pISpoofLockFile->Read(buf, 1024);
        m_sOrigISpoof = buf;

        if (!m_pISpoofLockFile->Seek(0) || !m_pISpoofLockFile->Truncate()) {
            delete m_pISpoofLockFile;
            m_pISpoofLockFile = nullptr;
            return false;
        }

        CString sData = ExpandString(GetNV("Format"));

        // If the format doesn't contain anything expandable, we'll
        // assume this is an "old"-style format string.
        if (sData == GetNV("Format")) {
            sData.Replace("%", GetUser()->GetIdent());
        }

        DEBUG("Writing [" + sData + "] to ident spoof file [" +
              m_pISpoofLockFile->GetLongName() + "] for user/network [" +
              GetUser()->GetUserName() + "/" + GetNetwork()->GetName() + "]");

        m_pISpoofLockFile->Write(sData + "\n");

        return true;
    }

    void ReleaseISpoof() {
        DEBUG("Releasing ident spoof for user/network [" +
              (m_pIRCSock
                   ? m_pIRCSock->GetNetwork()->GetUser()->GetUserName() + "/" +
                         m_pIRCSock->GetNetwork()->GetName()
                   : "<no user/network>") +
              "]");

        SetIRCSock(nullptr);

        if (m_pISpoofLockFile != nullptr) {
            if (m_pISpoofLockFile->Seek(0) && m_pISpoofLockFile->Truncate()) {
                m_pISpoofLockFile->Write(m_sOrigISpoof);
            }

            delete m_pISpoofLockFile;
            m_pISpoofLockFile = nullptr;
        }
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        m_pISpoofLockFile = nullptr;
        m_pIRCSock = nullptr;

        if (GetNV("Format").empty()) {
            SetNV("Format", "global { reply \"%ident%\" }");
        }

        if (GetNV("File").empty()) {
            SetNV("File", "~/.oidentd.conf");
        }

        return true;
    }

    EModRet OnIRCConnecting(CIRCSock* pIRCSock) override {
        if (m_pISpoofLockFile != nullptr) {
            DEBUG("Aborting connection, ident spoof lock file exists");
            PutModule(
                t_s("Aborting connection, another user or network is currently "
                    "connecting and using the ident spoof file"));
            return HALTCORE;
        }

        if (!WriteISpoof()) {
            DEBUG("identfile [" + GetNV("File") + "] could not be written");
            PutModule(
                t_f("[{1}] could not be written, retrying...")(GetNV("File")));
            return HALTCORE;
        }

        SetIRCSock(pIRCSock);
        return CONTINUE;
    }

    void OnIRCConnected() override {
        if (m_pIRCSock == GetNetwork()->GetIRCSock()) {
            ReleaseISpoof();
        }
    }

    void OnIRCConnectionError(CIRCSock* pIRCSock) override {
        if (m_pIRCSock == pIRCSock) {
            ReleaseISpoof();
        }
    }

    void OnIRCDisconnected() override {
        if (m_pIRCSock == GetNetwork()->GetIRCSock()) {
            ReleaseISpoof();
        }
    }
};

template <>
void TModInfo<CIdentFileModule>(CModInfo& Info) {
    Info.SetWikiPage("identfile");
}

GLOBALMODULEDEFS(
    CIdentFileModule,
    t_s("Write the ident of a user to a file when they are trying to connect."))
