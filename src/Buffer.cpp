/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Buffer.h>
#include <znc/znc.h>
#include <znc/Client.h>
#include <znc/User.h>

CBufLine::CBufLine(const CString& sFormat, const CString& sText, time_t tm) {
	m_sFormat = sFormat;
	m_sText = sText;
	if (tm == 0)
		UpdateTime();
	else
		m_tm = tm;
}

CBufLine::~CBufLine() {}

CString CBufLine::GetLine(const CClient& Client, const MCString& msParams) const {
	MCString msThisParams = msParams;

	if (Client.HasServerTime()) {
		msThisParams["text"] = m_sText;
		CString sStr = CString::NamedFormat(m_sFormat, msThisParams);
		return "@" + CString(m_tm) + " " + sStr;
	} else {
		msThisParams["text"] = Client.GetUser()->AddTimestamp(m_tm, m_sText);
		return CString::NamedFormat(m_sFormat, msThisParams);
	}
}

CBuffer::CBuffer(unsigned int uLineCount) {
	m_uLineCount = uLineCount;
}

CBuffer::~CBuffer() {}

int CBuffer::AddLine(const CString& sFormat, const CString& sText, time_t tm) {
	if (!m_uLineCount) {
		return 0;
	}

	while (size() >= m_uLineCount) {
		erase(begin());
	}

	push_back(CBufLine(sFormat, sText, tm));
	return size();
}

int CBuffer::UpdateLine(const CString& sMatch, const CString& sFormat, const CString& sText) {
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

int CBuffer::UpdateExactLine(const CString& sFormat, const CString& sText) {
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

CString CBuffer::GetLine(unsigned int uIdx, const CClient& Client, const MCString& msParams) const {
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
