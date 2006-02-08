#ifndef _HTTPSOCK_H
#define _HTTPSOCK_H

#include "main.h"
#include "Utils.h"
#include "FileUtils.h"
#include "User.h"

class CHTTPSock : public Csock {
public:
	CHTTPSock();
	CHTTPSock(const CString& sHostname, unsigned short uPort, int iTimeout = 60);
	virtual ~CHTTPSock();

	// Csocket derived members
	virtual void ReadData(const char* data, int len);
	virtual void ReadLine(const CString& sData);
	virtual void SockError(int iErrno);
	virtual void Timeout();
	virtual void Connected();
	virtual void Disconnected();
	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort);
	// !Csocket derived members

	// Hooks
	virtual bool OnPageRequest(const CString& sURI, CString& sPageRet);
	virtual bool OnLogin(const CString& sUser, const CString& sPass);
	// !Hooks

	void CheckPost();
	bool SentHeader() const;
	bool PrintHeader(unsigned long uContentLength, const CString& sContentType = "", unsigned int uStatusId = 200, const CString& sStatusMsg = "OK");
	void AddHeader(const CString& sName, const CString& sValue);
	void SetContentType(const CString& sContentType);

	bool PrintNotFound();
	bool Redirect(const CString& sURL);
	bool ForceLogin();
	CString GetErrorPage(unsigned int uStatusId, const CString& sStatusMsg, const CString& sMessage);
	bool PrintErrorPage(unsigned int uStatusId, const CString& sStatusMsg, const CString& sMessage);
	void ParseParams(const CString& sParams);
	void ParseURI();
	void GetPage();
	bool PrintFile(const CString& sFileName, const CString& sContentType);

	// Setters
	void SetDocRoot(const CString& s);
	// !Setters

	// Getters
	bool HasParam(const CString& sName) const;
	CString GetParam(const CString& sName) const;
	const CString& GetDocRoot() const;
	const CString& GetUser() const;
	const CString& GetPass() const;
	const CString& GetParamString() const;
	const CString& GetContentType() const;
	unsigned int GetParamValues(const CString& sName, VCString& vsRet) const;
	unsigned int GetParamValues(const CString& sName, set<CString>& ssRet) const;
	const map<CString, VCString>& GetParams() const;
	// !Getters
private:
protected:
	bool					m_bSentHeader;
	bool					m_bGotHeader;
	bool					m_bLoggedIn;
	bool					m_bPost;
	bool					m_bDone;
	unsigned long			m_uPostLen;
	CString					m_sPostData;
	CString					m_sURI;
	CString					m_sUser;
	CString					m_sPass;
	CString					m_sContentType;
	CString					m_sDocRoot;
	map<CString, VCString>	m_msvsParams;
	MCString				m_msHeaders;
};

#endif // !_HTTPSOCK_H

