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
#include <algorithm>

class CLogViewerMod : public CModule {
  public:
    MODCONSTRUCTOR(CLogViewerMod) {
        AddSubPage(std::make_shared<CWebSubPage>("logviewer", "Log Viewer", VPair(), 0));
    }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        if (sPageName != "logviewer")
            return false;

        // Get the standard ZNC log directory (absolute path)
        CString sLogPath = GetSavePath();
        sLogPath = CDir::ChangeDir(sLogPath, "../log");
        // Ensure we have an absolute path
        sLogPath = CDir::CheckPathPrefix(GetUser()->GetUserPath(), sLogPath);
        CString sFileParam = WebSock.GetParam("file");

        // Check if log directory exists
        CFile file(sLogPath);
        if (!file.Exists() || !file.IsDir()) {
            WebSock.PrintHeader(200, "text/html");
            WebSock.Write("<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
                          "<title>ZNC Log Viewer</title>"
                          "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:1em;}"
                          "a{color:#7af;}ul{list-style:none;padding:0;}li{padding:0.2em 0;}"
                          "</style></head><body><h1>ZNC Log Viewer</h1>"
                          "<p>No log files found in <code>" +
                          sLogPath.Escape(CString::EHTML) + "</code>.</p>"
                          "</body></html>");
            WebSock.Close(Csock::CLT_AFTERWRITE);
            return true;
        }

        if (!sFileParam.empty()) {
            // Prevent directory traversal attacks
            if (sFileParam.find("..") != CString::npos ||
                sFileParam.find('/') != CString::npos ||
                sFileParam.Left(1) == "/") {
                WebSock.PrintHeader(403, "text/html");
                WebSock.Write("<html><body><h1>403 Forbidden</h1></body></html>");
                WebSock.Close(Csock::CLT_AFTERWRITE);
                return true;
            }

            // Ensure the file is within the log directory
            CString sFullPath = CDir::CheckPathPrefix(sLogPath, sLogPath + "/" + sFileParam);
            if (sFullPath.empty()) {
                WebSock.PrintHeader(403, "text/html");
                WebSock.Write("<html><body><h1>403 Forbidden</h1></body></html>");
                WebSock.Close(Csock::CLT_AFTERWRITE);
                return true;
            }

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
                          "<title>Log: " + sFileParam.Escape(CString::EHTML) + "</title>"
                          "<style>body{font-family:monospace;background:#111;color:#eee;padding:1em;}"
                          "pre{white-space:pre-wrap;word-break:break-all;}a{color:#7af;}"
                          "</style></head><body>"
                          "<p><a href=\"?\">&larr; Back</a></p>"
                          "<h2>" + sFileParam.Escape(CString::EHTML) + "</h2>"
                          "<pre>" + sContent.Escape(CString::EHTML) + "</pre>"
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
                      "details{margin-left:1em;}summary{margin-left:-1em;cursor:pointer;}"
                      "</style></head><body><h1>ZNC Log Viewer</h1>");

        if (vsFiles.empty()) {
            WebSock.Write("<p>No log files found in <code>" +
                          sLogPath.Escape(CString::EHTML) + "</code>.</p>");
        } else {
            // Group files by directory
            std::map<CString, VCString> mDirs;
            for (const CString& sFile : vsFiles) {
                size_t iSlash = sFile.find_last_of('/');
                if (iSlash == CString::npos) {
                    mDirs[""].push_back(sFile);
                } else {
                    CString sDir = sFile.substr(0, iSlash);
                    CString sName = sFile.substr(iSlash + 1);
                    mDirs[sDir].push_back(sName);
                }
            }

            WebSock.Write("<p>Log directory: <code>" +
                          sLogPath.Escape(CString::EHTML) + "</code></p>");

            // Output files grouped by directory
            for (const auto& [sDir, vsNames] : mDirs) {
                if (sDir.empty()) {
                    WebSock.Write("<ul>");
                    for (const CString& sName : vsNames) {
                        CString sEscapedURL = sName;
                        CString sEscapedHTML = sName;
                        WebSock.Write("<li><a href=\"?file=" +
                                      sEscapedURL.Escape(CString::EURL) + "\">" +
                                      sEscapedHTML.Escape(CString::EHTML) + "</a></li>");
                    }
                    WebSock.Write("</ul>");
                } else {
                    CString sEscapedDir = sDir;
                    WebSock.Write("<details><summary>" +
                                  sEscapedDir.Escape(CString::EHTML) +
                                  "</summary><ul>");
                    for (const CString& sName : vsNames) {
                        CString sFullPath = sDir + "/" + sName;
                        CString sEscapedURL = sFullPath;
                        CString sEscapedHTML = sName;
                        WebSock.Write("<li><a href=\"?file=" +
                                      sEscapedURL.Escape(CString::EURL) + "\">" +
                                      sEscapedHTML.Escape(CString::EHTML) + "</a></li>");
                    }
                    WebSock.Write("</ul></details>");
                }
            }
        }

        WebSock.Write("</body></html>");
        WebSock.Close(Csock::CLT_AFTERWRITE);
        return true;
    }

  private:
    void CollectLogFiles(const CString& sBase, const CString& sCurrent,
                         VCString& vsFiles) const {
        CDir dir;
        if (!dir.Fill(sCurrent)) {
            return;
        }

        for (CFile* pFile : dir) {
            const CString& sName = pFile->GetShortName();
            if (sName == "." || sName == "..") {
                continue;
            }

            if (pFile->IsDir()) {
                CollectLogFiles(sBase, sCurrent + "/" + sName, vsFiles);
            } else if (pFile->IsReg() && sName.Right(4).Equals(".log")) {
                CString sRelativePath = sCurrent + "/" + sName;
                sRelativePath = sRelativePath.substr(sBase.length() + 1);
                vsFiles.push_back(sRelativePath);
            }
        }
    }
};

template <>
void TModInfo<CLogViewerMod>(CModInfo& Info) {
    Info.AddType(CModInfo::NetworkModule);
    Info.SetWikiPage("logviewer");
}

USERMODULEDEFS(CLogViewerMod, t_s("Browse ZNC logs via the web interface."))
