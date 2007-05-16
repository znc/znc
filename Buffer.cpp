//! @author prozac@rottenboy.com

#include "Buffer.h"

CBufLine::CBufLine(const CString& sPre, const CString& sPost, bool bIncNick=true) {
	m_sPre = sPre;
	m_sPost = sPost;
	m_bIncNick = bIncNick;
}

CBufLine::~CBufLine() {}

void CBufLine::GetLine(const CString& sTarget, CString& sRet) {
	if(m_bIncNick)
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

	if (size() >= m_uLineCount) {
		erase(begin());
	}

	push_back(CBufLine(sPre, sPost, bIncNick));
	return size();
}

int CBuffer::UpdateLine(const CString& sPre, const CString& sPost, bool bIncNick) {
	for(iterator it = begin(); it != end(); it++) {
		if(it->GetPre() == sPre) {
			it->SetPost(sPost);
			it->SetIncNick(bIncNick);
			return size();
		}
	}
	
	return AddLine(sPre, sPost, bIncNick);
}

bool CBuffer::GetLine(const CString& sTarget, CString& sRet, unsigned int uIdx) {
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
