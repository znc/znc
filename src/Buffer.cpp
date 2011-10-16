/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Buffer.h>
#include <znc/znc.h>

CBufLine::CBufLine(const CString& sFormat) {
	m_sFormat = sFormat;
}

CBufLine::~CBufLine() {}

CString CBufLine::GetLine(const MCString& msParams) const {
	return CString::NamedFormat(m_sFormat, msParams);
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

const CBufLine& CBuffer::GetBufLine(unsigned int uIdx) const {
	return (*this)[uIdx];
}

CString CBuffer::GetLine(unsigned int uIdx, const MCString& msParams) const {
	return (*this)[uIdx].GetLine(msParams);
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
