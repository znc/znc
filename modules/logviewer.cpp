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
#include <znc/Translation.h>
#include <algorithm>

class CLogViewerMod : public CModule {
  public:
    MODCONSTRUCTOR(CLogViewerMod) {
        AddSubPage(std::make_shared<CWebSubPage>("logviewer", t_s("Log Viewer"), VPair(), 0));
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

        // Set template variables
        Tmpl["LogPath"] = sLogPath;

        // Check if log directory exists
        CFile file(sLogPath);
        if (!file.Exists() || !file.IsDir()) {
            CString sPageRet;
            WebSock.PrintTemplate("index", sPageRet, this);
            WebSock.Write(sPageRet);
            return true;
        }

        if (!sFileParam.empty()) {
            // Prevent directory traversal attacks
            if (sFileParam.find("..") != CString::npos ||
                sFileParam.find('/') != CString::npos ||
                sFileParam.Left(1) == "/") {
                WebSock.PrintErrorPage("403 Forbidden");
                return true;
            }

            // Ensure the file is within the log directory
            CString sFullPath = CDir::CheckPathPrefix(sLogPath, sLogPath + "/" + sFileParam);
            if (sFullPath.empty()) {
                WebSock.PrintErrorPage("403 Forbidden");
                return true;
            }

            CFile LogFile(sFullPath);

            if (!LogFile.Exists() || !LogFile.IsReg()) {
                WebSock.PrintErrorPage("404 Not Found");
                return true;
            }

            CString sContent;
            LogFile.Open(O_RDONLY);
            LogFile.ReadFile(sContent);
            LogFile.Close();

            Tmpl["LogFileName"] = sFileParam;
            Tmpl["LogContent"] = sContent;

            CString sPageRet;
            WebSock.PrintTemplate("index", sPageRet, this);
            WebSock.Write(sPageRet);
            return true;
        }

        // List all .log files recursively
        VCString vsFiles;
        CollectLogFiles(sLogPath, sLogPath, vsFiles);
        std::sort(vsFiles.begin(), vsFiles.end());

        if (vsFiles.empty()) {
            CString sPageRet;
            WebSock.PrintTemplate("index", sPageRet, this);
            WebSock.Write(sPageRet);
            return true;
        }

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

        // Add files to template
        for (const auto& [sDir, vsNames] : mDirs) {
            if (sDir.empty()) {
                CTemplate& Row = Tmpl.AddRow("RootFilesLoop");
                for (const CString& sName : vsNames) {
                    CTemplate& FileRow = Row.AddRow("LogFilesLoop");
                    FileRow["FileName"] = sName;
                    FileRow["FilePath"] = sName;
                }
            } else {
                CTemplate& DirRow = Tmpl.AddRow("LogDirsLoop");
                DirRow["DirName"] = sDir;
                for (const CString& sName : vsNames) {
                    CTemplate& FileRow = DirRow.AddRow("LogFilesLoop");
                    FileRow["FileName"] = sName;
                    FileRow["FilePath"] = sDir + "/" + sName;
                }
            }
        }

        CString sPageRet;
        WebSock.PrintTemplate("index", sPageRet, this);
        WebSock.Write(sPageRet);
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
            } else if (pFile->IsReg() && sName.EndsWith(".log")) {
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
