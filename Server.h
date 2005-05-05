#ifndef _SERVER_H
#define _SERVER_H

#include "String.h"

class CServer {
public:
	CServer(const CString& sName, unsigned short uPort = 6667, const CString& sPass = "", bool bSSL = false);
	virtual ~CServer();

	const CString& GetName() const;
	unsigned short GetPort() const;
	const CString& GetPass() const;
	bool IsSSL() const;
	static bool IsValidHostName(const CString& sHostName);
private:
protected:
	CString			m_sName;
	unsigned short	m_uPort;
	CString			m_sPass;
	bool			m_bSSL;
};

#endif // !_SERVER_H
