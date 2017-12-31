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

#ifndef ZNC_SERVER_H
#define ZNC_SERVER_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>

class CServer {
  public:
    CServer(const CString& sName, unsigned short uPort = 6667,
            const CString& sPass = "", bool bSSL = false);
    ~CServer();

    const CString& GetName() const;
    unsigned short GetPort() const;
    const CString& GetPass() const;
    bool IsSSL() const;
    CString GetString(bool bIncludePassword = true) const;
    static bool IsValidHostName(const CString& sHostName);

  private:
  protected:
    CString m_sName;
    unsigned short m_uPort;
    CString m_sPass;
    bool m_bSSL;
};

#endif  // !ZNC_SERVER_H
