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

#include <syslog.h>
#include <time.h>

class CAdminLogMod : public CModule {
  public:
    MODCONSTRUCTOR(CAdminLogMod) {
        AddHelpCommand();
        AddCommand("Show", "", t_d("Show the logging target"),
                   [=](const CString& sLine) { OnShowCommand(sLine); });
        AddCommand("Target", t_d("<file|syslog|both> [path]"),
                   t_d("Set the logging target"),
                   [=](const CString& sLine) { OnTargetCommand(sLine); });
        openlog("znc", LOG_PID, LOG_DAEMON);
    }

    ~CAdminLogMod() override {
        Log("Logging ended.");
        closelog();
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        CString sTarget = GetNV("target");
        if (sTarget.Equals("syslog"))
            m_eLogMode = LOG_TO_SYSLOG;
        else if (sTarget.Equals("both"))
            m_eLogMode = LOG_TO_BOTH;
        else if (sTarget.Equals("file"))
            m_eLogMode = LOG_TO_FILE;
        else
            m_eLogMode = LOG_TO_FILE;

        SetLogFilePath(GetNV("path"));

        Log("Logging started. ZNC PID[" + CString(getpid()) + "] UID/GID[" +
            CString(getuid()) + ":" + CString(getgid()) + "]");
        return true;
    }

    void OnIRCConnected() override {
        Log("[" + GetUser()->GetUserName() + "/" + GetNetwork()->GetName() +
            "] connected to IRC: " +
            GetNetwork()->GetCurrentServer()->GetName());
    }

    void OnIRCDisconnected() override {
        Log("[" + GetUser()->GetUserName() + "/" + GetNetwork()->GetName() +
            "] disconnected from IRC");
    }

    EModRet OnRawMessage(CMessage& Message) override {
        if (Message.GetCommand().Equals("ERROR")) {
            // ERROR :Closing Link: nick[24.24.24.24] (Excess Flood)
            // ERROR :Closing Link: nick[24.24.24.24] Killer (Local kill by
            // Killer (reason))
            Log("[" + GetUser()->GetUserName() + "/" + GetNetwork()->GetName() +
                    "] disconnected from IRC: " +
                    GetNetwork()->GetCurrentServer()->GetName() + " [" +
                    Message.GetParamsColon(0) + "]",
                LOG_NOTICE);
        }
        return CONTINUE;
    }

    void OnClientLogin() override {
        Log("[" + GetUser()->GetUserName() + "] connected to ZNC from " +
            GetClient()->GetRemoteIP());
    }

    void OnClientDisconnect() override {
        Log("[" + GetUser()->GetUserName() + "] disconnected from ZNC from " +
            GetClient()->GetRemoteIP());
    }

    void OnFailedLogin(const CString& sUsername,
                       const CString& sRemoteIP) override {
        Log("[" + sUsername + "] failed to login from " + sRemoteIP,
            LOG_WARNING);
    }

    void SetLogFilePath(CString sPath) {
        if (sPath.empty()) {
            sPath = GetSavePath() + "/znc.log";
        }

        CFile LogFile(sPath);
        CString sLogDir = LogFile.GetDir();
        struct stat ModDirInfo;
        CFile::GetInfo(GetSavePath(), ModDirInfo);
        if (!CFile::Exists(sLogDir)) {
            CDir::MakeDir(sLogDir, ModDirInfo.st_mode);
        }

        m_sLogFile = sPath;
        SetNV("path", sPath);
    }

    void Log(CString sLine, int iPrio = LOG_INFO) {
        if (m_eLogMode & LOG_TO_SYSLOG) syslog(iPrio, "%s", sLine.c_str());

        if (m_eLogMode & LOG_TO_FILE) {
            time_t curtime;
            tm* timeinfo;
            char buf[23];

            time(&curtime);
            timeinfo = localtime(&curtime);
            strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S] ", timeinfo);

            CFile LogFile(m_sLogFile);

            if (LogFile.Open(O_WRONLY | O_APPEND | O_CREAT))
                LogFile.Write(buf + sLine + "\n");
            else
                DEBUG("Failed to write to [" << m_sLogFile
                                             << "]: " << strerror(errno));
        }
    }

    void OnModCommand(const CString& sCommand) override {
        if (!GetUser()->IsAdmin()) {
            PutModule(t_s("Access denied"));
        } else {
            HandleCommand(sCommand);
        }
    }

    void OnTargetCommand(const CString& sCommand) {
        CString sArg = sCommand.Token(1, false);
        CString sTarget;
        CString sMessage;
        LogMode mode;

        if (sArg.Equals("file")) {
            sTarget = "file";
            sMessage = t_s("Now logging to file");
            mode = LOG_TO_FILE;
        } else if (sArg.Equals("syslog")) {
            sTarget = "syslog";
            sMessage = t_s("Now only logging to syslog");
            mode = LOG_TO_SYSLOG;
        } else if (sArg.Equals("both")) {
            sTarget = "both";
            sMessage = t_s("Now logging to syslog and file");
            mode = LOG_TO_BOTH;
        } else {
            if (sArg.empty()) {
                PutModule(t_s("Usage: Target <file|syslog|both> [path]"));
            } else {
                PutModule(t_s("Unknown target"));
            }
            return;
        }

        if (mode != LOG_TO_SYSLOG) {
            CString sPath = sCommand.Token(2, true);
            SetLogFilePath(sPath);
            sMessage += " [" + sPath + "]";
        }

        Log(sMessage);
        SetNV("target", sTarget);
        m_eLogMode = mode;
        PutModule(sMessage);
    }

    void OnShowCommand(const CString& sCommand) {
        CString sTarget;

        switch (m_eLogMode) {
            case LOG_TO_FILE:
                sTarget = t_s("Logging is enabled for file");
                break;
            case LOG_TO_SYSLOG:
                sTarget = t_s("Logging is enabled for syslog");
                break;
            case LOG_TO_BOTH:
                sTarget = t_s("Logging is enabled for both, file and syslog");
                break;
        }

        PutModule(sTarget);
        if (m_eLogMode != LOG_TO_SYSLOG)
            PutModule(t_f("Log file will be written to {1}")(m_sLogFile));
    }

  private:
    enum LogMode {
        LOG_TO_FILE = 1 << 0,
        LOG_TO_SYSLOG = 1 << 1,
        LOG_TO_BOTH = LOG_TO_FILE | LOG_TO_SYSLOG
    };
    LogMode m_eLogMode = LOG_TO_FILE;
    CString m_sLogFile;
};

template <>
void TModInfo<CAdminLogMod>(CModInfo& Info) {
    Info.SetWikiPage("adminlog");
}

GLOBALMODULEDEFS(CAdminLogMod, t_s("Log ZNC events to file and/or syslog."))
