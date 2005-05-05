#ifndef _NICK_H
#define _NICK_H

#include <vector>
#include "String.h"
using std::vector;

// Forward Decl
class CUser;
class CChan;
// !Forward Decl

class CNick
{
public:
	CNick();
	CNick(const CString& sNick);
	virtual ~CNick();

	void Parse(const CString& sNickMask);
	CString GetHostMask() const;
	unsigned int GetCommonChans(vector<CChan*>& vChans, CUser* pUser) const;

	// Setters
	void SetNick(const CString& s);
	void SetIdent(const CString& s);
	void SetHost(const CString& s);
	void SetOp(bool b);
	void SetVoice(bool b);
	// !Setters

	// Getters
	bool IsOp() const;
	bool IsVoice() const;
	const CString& GetNick() const;
	const CString& GetIdent() const;
	const CString& GetHost() const;
	CString GetNickMask() const;
	// !Getters
private:
protected:
	CString		m_sNick;
	CString		m_sIdent;
	CString		m_sHost;

	bool		m_bIsOp;
	bool		m_bIsVoice;
};

#endif // !_NICK_H

