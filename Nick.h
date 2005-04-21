#ifndef _NICK_H
#define _NICK_H

#include <vector>
#include <string>
using std::string;
using std::vector;

// Forward Decl
class CUser;
class CChan;
// !Forward Decl

class CNick
{
public:
	CNick();
	CNick(const string& sNick);
	virtual ~CNick();

	void Parse(const string& sNickMask);
	string GetHostMask() const;
	unsigned int GetCommonChans(vector<CChan*>& vChans, CUser* pUser) const;

	// Setters
	void SetNick(const string& s);
	void SetIdent(const string& s);
	void SetHost(const string& s);
	void SetOp(bool b);
	void SetVoice(bool b);
	// !Setters

	// Getters
	bool IsOp() const;
	bool IsVoice() const;
	const string& GetNick() const;
	const string& GetIdent() const;
	const string& GetHost() const;
	string GetNickMask() const;
	// !Getters
private:
protected:
	string		m_sNick;
	string		m_sIdent;
	string		m_sHost;

	bool		m_bIsOp;
	bool		m_bIsVoice;
};

#endif // !_NICK_H

