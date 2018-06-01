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

#include <znc/Message.h>
#include <znc/Utils.h>

CMessage::CMessage(const CString& sMessage) {
    Parse(sMessage);
    InitTime();
}

CMessage::CMessage(const CNick& Nick, const CString& sCommand,
                   const VCString& vsParams, const MCString& mssTags)
    : m_Nick(Nick),
      m_sCommand(sCommand),
      m_vsParams(vsParams),
      m_mssTags(mssTags) {
    InitTime();
}

bool CMessage::Equals(const CMessage& Other) const {
    return m_Nick.NickEquals(Other.GetNick().GetNick()) &&
           m_sCommand.Equals(Other.GetCommand()) &&
           m_vsParams == Other.GetParams();
}

void CMessage::Clone(const CMessage& Message) {
    if (&Message != this) {
        *this = Message;
    }
}

void CMessage::SetCommand(const CString& sCommand) {
    m_sCommand = sCommand;
    InitType();
}

CString CMessage::GetParamsColon(unsigned int uIdx, unsigned int uLen) const {
    if (m_vsParams.empty() || uLen == 0) {
        return "";
    }
    if (uLen > m_vsParams.size() - uIdx - 1) {
        uLen = m_vsParams.size() - uIdx;
    }
    VCString vsParams;
    unsigned uParams = m_vsParams.size();
    for (unsigned int i = uIdx; i < uIdx + uLen; ++i) {
        CString sParam = m_vsParams[i];
        if (i == uParams - 1 &&
            (m_bColon || sParam.empty() || sParam.StartsWith(":") ||
             sParam.Contains(" "))) {
            sParam = ":" + sParam;
        }
        vsParams.push_back(sParam);
    }
    return CString(" ").Join(vsParams.begin(), vsParams.end());
}

void CMessage::SetParams(const VCString& vsParams) {
    m_vsParams = vsParams;
    m_bColon = false;

    if (m_eType == Type::Text || m_eType == Type::Notice ||
        m_eType == Type::Action || m_eType == Type::CTCP) {
        InitType();
    }
}

CString CMessage::GetParam(unsigned int uIdx) const {
    if (uIdx >= m_vsParams.size()) {
        return "";
    }
    return m_vsParams[uIdx];
}

void CMessage::SetParam(unsigned int uIdx, const CString& sParam) {
    if (uIdx >= m_vsParams.size()) {
        m_vsParams.resize(uIdx + 1);
    }
    m_vsParams[uIdx] = sParam;

    if (uIdx == 1 && (m_eType == Type::Text || m_eType == Type::Notice ||
                      m_eType == Type::Action || m_eType == Type::CTCP)) {
        InitType();
    }
}

CString CMessage::GetTag(const CString& sKey) const {
    MCString::const_iterator it = m_mssTags.find(sKey);
    if (it != m_mssTags.end()) {
        return it->second;
    }
    return "";
}

void CMessage::SetTag(const CString& sKey, const CString& sValue) {
    m_mssTags[sKey] = sValue;
}

CString CMessage::ToString(unsigned int uFlags) const {
    CString sMessage;

    // <tags>
    if (!(uFlags & ExcludeTags) && !m_mssTags.empty()) {
        CString sTags;
        for (const auto& it : m_mssTags) {
            if (!sTags.empty()) {
                sTags += ";";
            }
            sTags += it.first;
            if (!it.second.empty())
                sTags += "=" + it.second.Escape_n(CString::EMSGTAG);
        }
        sMessage = "@" + sTags;
    }

    // <prefix>
    if (!(uFlags & ExcludePrefix)) {
        CString sPrefix = m_Nick.GetHostMask();
        if (!sPrefix.empty()) {
            if (!sMessage.empty()) {
                sMessage += " ";
            }
            sMessage += ":" + sPrefix;
        }
    }

    // <command>
    if (!m_sCommand.empty()) {
        if (!sMessage.empty()) {
            sMessage += " ";
        }
        sMessage += m_sCommand;
    }

    // <params>
    if (!m_vsParams.empty()) {
        if (!sMessage.empty()) {
            sMessage += " ";
        }
        sMessage += GetParamsColon(0);
    }

    return sMessage;
}

void CMessage::Parse(CString sMessage) {
    // <tags>
    m_mssTags.clear();
    if (sMessage.StartsWith("@")) {
        VCString vsTags;
        sMessage.Token(0).TrimPrefix_n("@").Split(";", vsTags, false);
        for (const CString& sTag : vsTags) {
            CString sKey = sTag.Token(0, false, "=", true);
            CString sValue = sTag.Token(1, true, "=", true);
            m_mssTags[sKey] =
                sValue.Escape(CString::EMSGTAG, CString::CString::EASCII);
        }
        sMessage = sMessage.Token(1, true);
    }

    //  <message>  ::= [':' <prefix> <SPACE> ] <command> <params> <crlf>
    //  <prefix>   ::= <servername> | <nick> [ '!' <user> ] [ '@' <host> ]
    //  <command>  ::= <letter> { <letter> } | <number> <number> <number>
    //  <SPACE>    ::= ' ' { ' ' }
    //  <params>   ::= <SPACE> [ ':' <trailing> | <middle> <params> ]
    //  <middle>   ::= <Any *non-empty* sequence of octets not including SPACE
    //                 or NUL or CR or LF, the first of which may not be ':'>
    //  <trailing> ::= <Any, possibly *empty*, sequence of octets not including
    //                   NUL or CR or LF>

    // <prefix>
    if (sMessage.TrimPrefix(":")) {
        m_Nick.Parse(sMessage.Token(0));
        sMessage = sMessage.Token(1, true);
    }

    // <command>
    m_sCommand = sMessage.Token(0);
    sMessage = sMessage.Token(1, true);

    // <params>
    m_bColon = false;
    m_vsParams.clear();
    while (!sMessage.empty()) {
        m_bColon = sMessage.TrimPrefix(":");
        if (m_bColon) {
            m_vsParams.push_back(sMessage);
            sMessage.clear();
        } else {
            m_vsParams.push_back(sMessage.Token(0));
            sMessage = sMessage.Token(1, true);
        }
    }

    InitType();
}

void CMessage::InitTime() {
    auto it = m_mssTags.find("time");
    if (it != m_mssTags.end()) {
        m_time = CUtils::ParseServerTime(it->second);
        return;
    }

    m_time = CUtils::GetTime();
}

void CMessage::InitType() {
    if (m_sCommand.length() == 3 && isdigit(m_sCommand[0]) &&
        isdigit(m_sCommand[1]) && isdigit(m_sCommand[2])) {
        m_eType = Type::Numeric;
    } else if (m_sCommand.Equals("PRIVMSG")) {
        CString sParam = GetParam(1);
        if (sParam.TrimPrefix("\001") && sParam.EndsWith("\001")) {
            if (sParam.StartsWith("ACTION ")) {
                m_eType = Type::Action;
            } else {
                m_eType = Type::CTCP;
            }
        } else {
            m_eType = Type::Text;
        }
    } else if (m_sCommand.Equals("NOTICE")) {
        CString sParam = GetParam(1);
        if (sParam.StartsWith("\001") && sParam.EndsWith("\001")) {
            m_eType = Type::CTCP;
        } else {
            m_eType = Type::Notice;
        }
    } else {
        std::map<CString, Type> mTypes = {
            {"ACCOUNT", Type::Account},
            {"AWAY", Type::Away},
            {"CAP", Type::Capability},
            {"ERROR", Type::Error},
            {"INVITE", Type::Invite},
            {"JOIN", Type::Join},
            {"KICK", Type::Kick},
            {"MODE", Type::Mode},
            {"NICK", Type::Nick},
            {"PART", Type::Part},
            {"PING", Type::Ping},
            {"PONG", Type::Pong},
            {"QUIT", Type::Quit},
            {"TOPIC", Type::Topic},
            {"WALLOPS", Type::Wallops},
        };
        auto it = mTypes.find(m_sCommand.AsUpper());
        if (it != mTypes.end()) {
            m_eType = it->second;
        } else {
            m_eType = Type::Unknown;
        }
    }
}
