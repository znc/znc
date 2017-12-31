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

#include <znc/Buffer.h>
#include <znc/znc.h>
#include <znc/User.h>
#include <time.h>

CBufLine::CBufLine(const CMessage& Format, const CString& sText)
    : m_Message(Format), m_sText(sText) {}

CBufLine::CBufLine(const CString& sFormat, const CString& sText,
                   const timeval* ts, const MCString& mssTags)
    : m_sText(sText) {
    m_Message.Parse(sFormat);
    m_Message.SetTags(mssTags);

    if (ts == nullptr)
        UpdateTime();
    else
        m_Message.SetTime(*ts);
}

CBufLine::~CBufLine() {}

void CBufLine::UpdateTime() {
    m_Message.SetTime(CUtils::GetTime());
}

CMessage CBufLine::ToMessage(const CClient& Client,
                             const MCString& mssParams) const {
    CMessage Line = m_Message;

    CString sSender = Line.GetNick().GetNickMask();
    Line.SetNick(CNick(CString::NamedFormat(sSender, mssParams)));

    MCString mssThisParams = mssParams;
    if (Client.HasServerTime()) {
        mssThisParams["text"] = m_sText;
    } else {
        mssThisParams["text"] =
            Client.GetUser()->AddTimestamp(Line.GetTime(), m_sText);
    }

    // make a copy of params, because the following loop modifies the original
    VCString vsParams = Line.GetParams();
    for (unsigned int uIdx = 0; uIdx < vsParams.size(); ++uIdx) {
        Line.SetParam(uIdx,
                      CString::NamedFormat(vsParams[uIdx], mssThisParams));
    }

    return Line;
}

CString CBufLine::GetLine(const CClient& Client,
                          const MCString& mssParams) const {
    CMessage Line = ToMessage(Client, mssParams);

    // Note: Discard all tags (except the time tag, conditionally) to
    // keep the same behavior as ZNC versions 1.6 and earlier had. See
    // CClient::PutClient(CMessage) documentation for more details.
    Line.SetTags(MCString::EmptyMap);

    if (Client.HasServerTime()) {
        CString sTime = m_Message.GetTag("time");
        if (sTime.empty()) {
            sTime = CUtils::FormatServerTime(m_Message.GetTime());
        }
        Line.SetTag("time", sTime);
    }

    return Line.ToString();
}

CBuffer::CBuffer(unsigned int uLineCount) : m_uLineCount(uLineCount) {}

CBuffer::~CBuffer() {}

CBuffer::size_type CBuffer::AddLine(const CMessage& Format,
                                    const CString& sText) {
    if (!m_uLineCount) {
        return 0;
    }

    while (size() >= m_uLineCount) {
        erase(begin());
    }

    push_back(CBufLine(Format, sText));
    return size();
}

CBuffer::size_type CBuffer::UpdateLine(const CString& sCommand,
                                       const CMessage& Format,
                                       const CString& sText) {
    for (CBufLine& Line : *this) {
        if (Line.GetCommand().Equals(sCommand)) {
            Line = CBufLine(Format, sText);
            return size();
        }
    }

    return AddLine(Format, sText);
}

CBuffer::size_type CBuffer::UpdateExactLine(const CMessage& Format,
                                            const CString& sText) {
    for (CBufLine& Line : *this) {
        if (Line.Equals(Format)) {
            return size();
        }
    }

    return AddLine(Format, sText);
}

CBuffer::size_type CBuffer::AddLine(const CString& sFormat,
                                    const CString& sText, const timeval* ts,
                                    const MCString& mssTags) {
    CMessage Message(sFormat);
    if (ts) {
        Message.SetTime(*ts);
    }
    Message.SetTags(mssTags);
    return AddLine(Message, sText);
}

CBuffer::size_type CBuffer::UpdateLine(const CString& sMatch,
                                       const CString& sFormat,
                                       const CString& sText) {
    return UpdateLine(CMessage(sMatch).GetCommand(), CMessage(sFormat), sText);
}

CBuffer::size_type CBuffer::UpdateExactLine(const CString& sFormat,
                                            const CString& sText) {
    return UpdateExactLine(CMessage(sFormat, sText));
}

const CBufLine& CBuffer::GetBufLine(unsigned int uIdx) const {
    return (*this)[uIdx];
}

CString CBuffer::GetLine(size_type uIdx, const CClient& Client,
                         const MCString& msParams) const {
    return (*this)[uIdx].GetLine(Client, msParams);
}

bool CBuffer::SetLineCount(unsigned int u, bool bForce) {
    if (!bForce && u > CZNC::Get().GetMaxBufferSize()) {
        return false;
    }

    m_uLineCount = u;

    // We may need to shrink the buffer if the allowed size got smaller
    while (size() > m_uLineCount) {
        erase(begin());
    }

    return true;
}
