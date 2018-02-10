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

#ifndef ZNC_NICK_H
#define ZNC_NICK_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <vector>

// Forward Decl
class CIRCNetwork;
class CChan;
// !Forward Decl

class CNick {
  public:
    CNick();
    CNick(const CString& sNick);
    ~CNick();

    CNick(const CNick&) = default;
    CNick& operator=(const CNick&) = default;

    void Reset();
    void Parse(const CString& sNickMask);
    CString GetHostMask() const;
    size_t GetCommonChans(std::vector<CChan*>& vChans,
                          CIRCNetwork* pNetwork) const;
    bool NickEquals(const CString& nickname) const;

    // Setters
    void SetNetwork(CIRCNetwork* pNetwork);
    void SetNick(const CString& s);
    void SetIdent(const CString& s);
    void SetHost(const CString& s);
    /// e.g. '@' for chanop.
    bool AddPerm(char cPerm);
    /// e.g. '@' for chanop.
    bool RemPerm(char cPerm);
    // !Setters

    // Getters
    /// e.g. '@' for chanop.
    CString GetPermStr() const;
    /// e.g. '@' for chanop.
    char GetPermChar() const;
    /// e.g. '@' for chanop.
    bool HasPerm(char cPerm) const;
    const CString& GetNick() const;
    const CString& GetIdent() const;
    const CString& GetHost() const;
    CString GetNickMask() const;
    // !Getters

    void Clone(const CNick& SourceNick);

  private:
  protected:
    CString m_sChanPerms;
    CIRCNetwork* m_pNetwork;
    CString m_sNick;
    CString m_sIdent;
    CString m_sHost;
};

#endif  // !ZNC_NICK_H
