/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Buffer.h>

CBufLine::CBufLine(const CString& sFormat) {
	m_sFormat = sFormat;
}

CBufLine::~CBufLine() {}

void CBufLine::GetLine(CString& sRet, const MCString& msParams) const {
	sRet = CString::NamedFormat(m_sFormat, msParams);
}

CBuffer::CBuffer(unsigned int uLineCount) {
	m_uLineCount = uLineCount;
}

CBuffer::~CBuffer() {}

int CBuffer::AddLine(const CString& sFormat) {
	if (!m_uLineCount) {
		return 0;
	}

	while (size() >= m_uLineCount) {
		erase(begin());
	}

	push_back(CBufLine(sFormat));
	return size();
}

int CBuffer::UpdateLine(const CString& sMatch, const CString& sFormat) {
	for (iterator it = begin(); it != end(); ++it) {
		if (it->GetFormat().compare(0, sMatch.length(), sMatch) == 0) {
			it->SetFormat(sFormat);
			return size();
		}
	}

	return AddLine(sFormat);
}

int CBuffer::UpdateExactLine(const CString& sFormat) {
	for (iterator it = begin(); it != end(); ++it) {
		if (it->GetFormat() == sFormat) {
			return size();
		}
	}

	return AddLine(sFormat);
}

bool CBuffer::GetLine(unsigned int uIdx, CString& sRet, const MCString& msParams) const {
	if (uIdx >= size()) {
		return false;
	}

	(*this)[uIdx].GetLine(sRet, msParams);
	return true;
}

bool CBuffer::GetNextLine(CString& sRet, const MCString& msParams) {
	sRet = "";

	if (!size()) {
		return false;
	}

	begin()->GetLine(sRet, msParams);
	erase(begin());
	return true;
}

void CBuffer::SetLineCount(unsigned int u) {
	m_uLineCount = u;

	// We may need to shrink the buffer if the allowed size got smaller
	while (size() > m_uLineCount) {
		erase(begin());
	}
}
