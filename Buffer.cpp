/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Buffer.h"

CBufLine::CBufLine(const CString& sPre, const CString& sPost, bool bIncNick=true) {
	m_sPre = sPre;
	m_sPost = sPost;
	m_bIncNick = bIncNick;
}

CBufLine::~CBufLine() {}

void CBufLine::GetLine(const CString& sTarget, CString& sRet) const {
	if (m_bIncNick)
		sRet = m_sPre + sTarget + m_sPost;
	else
		sRet = m_sPre + m_sPost;
}

CBuffer::CBuffer(unsigned int uLineCount) {
	m_uLineCount = uLineCount;
}

CBuffer::~CBuffer() {}

int CBuffer::AddLine(const CString& sPre, const CString& sPost, bool bIncNick) {
	if (!m_uLineCount) {
		return 0;
	}

	while (size() >= m_uLineCount) {
		erase(begin());
	}

	push_back(CBufLine(sPre, sPost, bIncNick));
	return size();
}

int CBuffer::UpdateLine(const CString& sPre, const CString& sPost, bool bIncNick) {
	for (iterator it = begin(); it != end(); ++it) {
		if (it->GetPre() == sPre) {
			it->SetPost(sPost);
			it->SetIncNick(bIncNick);
			return size();
		}
	}

	return AddLine(sPre, sPost, bIncNick);
}

int CBuffer::UpdateExactLine(const CString& sPre, const CString& sPost, bool bIncNick) {
	for (iterator it = begin(); it != end(); ++it) {
		if (it->GetPre() == sPre && it->GetPost() == sPost)
			return size();
	}

	return AddLine(sPre, sPost, bIncNick);
}

bool CBuffer::GetLine(const CString& sTarget, CString& sRet, unsigned int uIdx) const {
	if (uIdx >= size()) {
		return false;
	}

	(*this)[uIdx].GetLine(sTarget, sRet);
	return true;
}

bool CBuffer::GetNextLine(const CString& sTarget, CString& sRet) {
	sRet = "";

	if (!size()) {
		return false;
	}

	begin()->GetLine(sTarget, sRet);
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
