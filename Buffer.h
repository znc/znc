#ifndef _BUFFER_H
#define _BUFFER_H

#include <string>
#include <vector>
using std::vector;
using std::string;

class CBufLine {
public:
	CBufLine(const string& sPre, const string& sPost);
	virtual ~CBufLine();
	void GetLine(const string& sTarget, string& sRet);
 
private:
protected:
	string	m_sPre;
	string	m_sPost;
};

class CBuffer : private vector<CBufLine> {
public:
	CBuffer(unsigned int uLineCount = 100);
	virtual ~CBuffer();

	int AddLine(const string& sPre, const string& sPost);
	bool GetNextLine(const string& sTarget, string& sRet);
	bool GetLine(const string& sTarget, string& sRet, unsigned int uIdx);
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

