#ifndef _SERVER_H
#define _SERVER_H

#include <string.h>

class CServer {
public:
	CServer(const string& sName, unsigned short uPort = 6667, const string& sPass = "", bool bSSL = false);
	virtual ~CServer();

	const string& GetName() const;
	unsigned short GetPort() const;
	const string& GetPass() const;
	bool IsSSL() const;
private:
protected:
	string			m_sName;
	unsigned short	m_uPort;
	string			m_sPass;
	bool			m_bSSL;
};

#endif // !_SERVER_H
