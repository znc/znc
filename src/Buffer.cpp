/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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

#include <znc/znc.h>
#include <znc/User.h>

CBufLine::CBufLine(const CString& sFormat, const CString& sText, const timeval* ts) {
	m_sFormat = sFormat;
	m_sText = sText;
	if (ts == nullptr)
		UpdateTime();
	else
		m_time = *ts;
}

CBufLine::~CBufLine() {}

void CBufLine::UpdateTime() {
	if (0 == gettimeofday(&m_time, nullptr)) {
		return;
	}
	m_time.tv_sec = time(nullptr);
	m_time.tv_usec = 0;
}

CString CBufLine::GetLine(const CClient& Client, const MCString& msParams) const {
	MCString msThisParams = msParams;

	if (Client.HasServerTime()) {
		msThisParams["text"] = m_sText;
		CString sStr = CString::NamedFormat(m_sFormat, msThisParams);
		return "@time=" + CUtils::FormatServerTime(m_time) + " " + sStr;
	} else {
		msThisParams["text"] = Client.GetUser()->AddTimestamp(m_time.tv_sec, m_sText);
		return CString::NamedFormat(m_sFormat, msThisParams);
	}
}

CBuffer::CBuffer(unsigned int uLineCount) {
	m_uLineCount = uLineCount;
}

CBuffer::~CBuffer() {}

CBuffer::size_type CBuffer::AddLine(const CString& sFormat, const CString& sText, const timeval* ts) {
	if (!m_uLineCount) {
		return 0;
	}

	while (size() >= m_uLineCount) {
		erase(begin());
	}

	push_back(CBufLine(sFormat, sText, ts));
	return size();
}

CBuffer::size_type CBuffer::UpdateLine(const CString& sMatch, const CString& sFormat, const CString& sText) {
	for (iterator it = begin(); it != end(); ++it) {
		if (it->GetFormat().compare(0, sMatch.length(), sMatch) == 0) {
			it->SetFormat(sFormat);
			it->SetText(sText);
			it->UpdateTime();
			return size();
		}
	}

	return AddLine(sFormat, sText);
}

CBuffer::size_type CBuffer::UpdateExactLine(const CString& sFormat, const CString& sText) {
	for (iterator it = begin(); it != end(); ++it) {
		if (it->GetFormat() == sFormat && it->GetText() == sText) {
			return size();
		}
	}

	return AddLine(sFormat, sText);
}

const CBufLine& CBuffer::GetBufLine(unsigned int uIdx) const {
	return (*this)[uIdx];
}

CString CBuffer::GetLine(size_type uIdx, const CClient& Client, const MCString& msParams) const {
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
