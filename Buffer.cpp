#include "Buffer.h"

CBufLine::CBufLine(const string& sPre, const string& sPost) {
	m_sPre = sPre;
	m_sPost = sPost;
}

CBufLine::~CBufLine() {}

void CBufLine::GetLine(const string& sTarget, string& sRet) {
	sRet = m_sPre + sTarget + m_sPost;
}

CBuffer::CBuffer(unsigned int uLineCount) {
	m_uLineCount = uLineCount;
}

CBuffer::~CBuffer() {}

int CBuffer::AddLine(const string& sPre, const string& sPost) {
	if (!m_uLineCount) {
		return 0;
	}

	if (size() >= m_uLineCount) {
		erase(begin());
	}

	push_back(CBufLine(sPre, sPost));
	return size();
}

bool CBuffer::GetLine(const string& sTarget, string& sRet, unsigned int uIdx) {
	if (uIdx >= size()) {
		return false;
	}

	(*this)[uIdx].GetLine(sTarget, sRet);
	return true;
}

bool CBuffer::GetNextLine(const string& sTarget, string& sRet) {
	sRet = "";

	if (!size()) {
		return false;
	}

	begin()->GetLine(sTarget, sRet);
	erase(begin());
	return true;
}
