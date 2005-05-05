#include "Buffer.h"

CBufLine::CBufLine(const CString& sPre, const CString& sPost) {
	m_sPre = sPre;
	m_sPost = sPost;
}

CBufLine::~CBufLine() {}

void CBufLine::GetLine(const CString& sTarget, CString& sRet) {
	sRet = m_sPre + sTarget + m_sPost;
}

CBuffer::CBuffer(unsigned int uLineCount) {
	m_uLineCount = uLineCount;
}

CBuffer::~CBuffer() {}

int CBuffer::AddLine(const CString& sPre, const CString& sPost) {
	if (!m_uLineCount) {
		return 0;
	}

	if (size() >= m_uLineCount) {
		erase(begin());
	}

	push_back(CBufLine(sPre, sPost));
	return size();
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
