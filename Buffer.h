#ifndef _BUFFER_H
#define _BUFFER_H

#include "String.h"
#include <vector>
using std::vector;

class CBufLine {
public:
	CBufLine(const CString& sPre, const CString& sPost);
	virtual ~CBufLine();
	void GetLine(const CString& sTarget, CString& sRet);

private:
protected:
	CString	m_sPre;
	CString	m_sPost;
};

class CBuffer : private vector<CBufLine> {
public:
	CBuffer(unsigned int uLineCount = 100);
	virtual ~CBuffer();

	int AddLine(const CString& sPre, const CString& sPost);
	bool GetNextLine(const CString& sTarget, CString& sRet);
	bool GetLine(const CString& sTarget, CString& sRet, unsigned int uIdx);
	bool IsEmpty() { return empty(); }
	void Clear() { clear(); }

	// Setters
	void SetLineCount(unsigned int u) { m_uLineCount = u; }
	// !Setters

	// Getters
	unsigned int GetLineCount() const { return m_uLineCount; }
	// !Getters
private:
protected:
	unsigned int	m_uLineCount;
};

#endif // !_BUFFER_H
