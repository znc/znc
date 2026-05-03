/*
 * Copyright (C) 2004-2026 ZNC, see the NOTICE file for details.
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
#include <znc/Modules.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/WebModules.h>
#include <dirent.h>
#include <sys/stat.h>

class CLogViewerMod : public CModule {
  public:
    MODCONSTRUCTOR(CLogViewerMod) {
        AddSubPage(std::make_shared<CWebSubPage>("logviewer", "Log Viewer", VPair(), 0));
    }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        if (sPageName != "logviewer")
            return false;

        CString sLogPath = GetSavePath();
        CString sFileParam = WebSock.GetParam("file");

        if (!sFileParam.empty()) {
            if (sFileParam.find("..") != CString::npos ||
                sFileParam.Left(1) == "/") {
                WebSock.PrintHeader(403, "text/html");
                WebSock.Write("<html><body><h1>403 Forbidden</h1></body></html>");
                WebSock.Close(Csock::CLT_AFTERWRITE);
                return true;
            }

            CString sFullPath = sLogPath + "/" + sFileParam;
            CFile LogFile(sFullPath);

            if (!LogFile.Exists() || !LogFile.IsReg()) {
                WebSock.PrintHeader(404, "text/html");
                WebSock.Write("<html><body><h1>404 Not Found</h1></body></html>");
                WebSock.Close(Csock::CLT_AFTERWRITE);
                return true;
            }

            CString sContent;
            LogFile.Open(O_RDONLY);
            LogFile.ReadFile(sContent);
            LogFile.Close();

            WebSock.PrintHeader(200, "text/html");
            WebSock.Write("<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
                          "<title>Log: " + sFileParam.Escape_n(CString::EHTML) + "</title>"
                          "<style>body{font-family:monospace;background:#111;color:#eee;padding:1em;}"
                          "pre{white-space:pre-wrap;word-break:break-all;}a{color:#7af;}"
                          "</style></head><body>"
                          "<p><a href=\"?\">&larr; Back</a></p>"
                          "<h2>" + sFileParam.Escape_n(CString::EHTML) + "</h2>"
                          "<pre>" + sContent.Escape_n(CString::EHTML) + "</pre>"
                          "</body></html>");
            WebSock.Close(Csock::CLT_AFTERWRITE);
            return true;
        }

        // List all .log files recursively
        VCString vsFiles;
        CollectLogFiles(sLogPath, sLogPath, vsFiles);
        std::sort(vsFiles.begin(), vsFiles.end());

        WebSock.PrintHeader(200, "text/html");
        WebSock.Write("<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
                      "<title>ZNC Log Viewer</title>"
                      "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:1em;}"
                      "a{color:#7af;}ul{list-style:none;padding:0;}li{padding:0.2em 0;}"
                      "</style></head><body><h1>ZNC Log Viewer</h1>");

        if (vsFiles.empty()) {
            WebSock.Write("<p>No log files found in <code>" +
                          sLogPath.Escape_n(CString::EHTML) + "</code>.</p>");
        } else {
            WebSock.Write("<p>Log directory: <code>" +
                          sLogPath.Escape_n(CString::EHTML) + "</code></p><ul>");
            for (const CString& sFile : vsFiles) {
                WebSock.Write("<li><a href=\"?file=" +
                              sFile.Escape_n(CString::EURL) + "\">" +
                              sFile.Escape_n(CString::EHTML) + "</a></li>");
            }
            WebSock.Write("</ul>");
        }

        WebSock.Write("</body></html>");
        WebSock.Close(Csock::CLT_AFTERWRITE);
        return true;
    }

  private:
    void CollectLogFiles(const CString& sBase, const CString& sCurrent,
                         VCString& vsFiles) {
        DIR* pDir = opendir(sCurrent.c_str());
        if (!pDir) return;

        struct dirent* pEntry;
        while ((pEntry = readdir(pDir)) != nullptr) {
            CString sName(pEntry->d_name);
            if (sName == "." || sName == "..") continue;

            CString sFullPath = sCurrent + "/" + sName;
            struct stat st;
            if (stat(sFullPath.c_str(), &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                CollectLogFiles(sBase, sFullPath, vsFiles);
            } else if (S_ISREG(st.st_mode) && sName.Right(4) == ".log") {
                vsFiles.push_back(sFullPath.substr(sBase.length() + 1));
            }
        }
        closedir(pDir);
    }
};

template <>
void TModInfo<CLogViewerMod>(CModInfo& Info) {
    Info.AddType(CModInfo::NetworkModule);
    Info.SetWikiPage("logviewer");
}

USERMODULEDEFS(CLogViewerMod, t_s("Browse ZNC logs via the web interface."))
