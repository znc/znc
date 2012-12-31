/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Buffer.h>
#include <znc/znc.h>
#include <znc/Client.h>
#include <znc/User.h>

CBufLine::CBufLine(const CString& sFormat, const CString& sText, const timeval* ts) {
	m_sFormat = sFormat;
	m_sText = sText;
	if (ts == NULL)
		UpdateTime();
	else
		m_time = *ts;
}

CBufLine::~CBufLine() {}

void CBufLine::UpdateTime() {
	if (0 == gettimeofday(&m_time, NULL)) {
		return;
	}
	m_time.tv_sec = time(NULL);
	m_time.tv_usec = 0;
}

CString CBufLine::GetLine(const CClient& Client, const MCString& msParams) const {
	MCString msThisParams = msParams;

	if (Client.HasServerTime()) {
		msThisParams["text"] = m_sText;
		CString sStr = CString::NamedFormat(m_sFormat, msThisParams);
		CString s_msec(m_time.tv_usec / 1000);
		while (s_msec.length() < 3) {
			s_msec = "0" + s_msec;
		}
		// TODO support leap seconds properly
		// TODO support message-tags properly
		struct tm stm;
		memset(&stm, 0, sizeof(stm));
		gmtime_r(&m_time.tv_sec, &stm);
		char sTime[20] = {};
		strftime(sTime, sizeof(sTime), "%Y-%m-%dT%H:%M:%S", &stm);
		return "@time=" + CString(sTime) + "." + s_msec + "Z " + sStr;
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
