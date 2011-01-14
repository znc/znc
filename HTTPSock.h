/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _HTTPSOCK_H
#define _HTTPSOCK_H

#include "zncconfig.h"
#include "Socket.h"

class CModule;

class CHTTPSock : public CSocket {
public:
	CHTTPSock(CModule *pMod);
	CHTTPSock(CModule *pMod, const CString& sHostname, unsigned short uPort, int iTimeout = 60);
	virtual ~CHTTPSock();

	// Csocket derived members
	virtual void ReadData(const char* data, size_t len);
	virtual void ReadLine(const CString& sData);
	virtual void ReachedMaxBuffer();
	virtual void SockError(int iErrno);
	virtual void Timeout();
	virtual void Connected();
	virtual void Disconnected();
	virtual Csock* GetSockObj(const CString& sHost, unsigned short uPort) = 0;
	// !Csocket derived members

	// Hooks
	virtual bool ForceLogin();
	virtual bool OnLogin(const CString& sUser, const CString& sPass);
	virtual void OnPageRequest(const CString& sURI) = 0;
	virtual bool PrintFile(const CString& sFileName, CString sContentType = "");
	// !Hooks

	void CheckPost();
	bool SentHeader() const;
	bool PrintHeader(off_t uContentLength, const CString& sContentType = "", unsigned int uStatusId = 200, const CString& sStatusMsg = "OK");
	void AddHeader(const CString& sName, const CString& sValue);
	void SetContentType(const CString& sContentType);

	bool PrintNotFound();
	bool Redirect(const CString& sURL);
	bool PrintErrorPage(unsigned int uStatusId, const CString& sStatusMsg, const CString& sMessage);
	static void ParseParams(const CString& sParams, map<CString, VCString>& msvsParams);
	void ParseURI();
	void GetPage();
	static CString GetDate(time_t tm = 0);

	// Cookies
	CString GetRequestCookie(const CString& sKey) const;
	bool SendCookie(const CString& sKey, const CString& sValue);
	// Cookies

	// Setters
	void SetDocRoot(const CString& s);
	void SetLoggedIn(bool b) { m_bLoggedIn = b; }
	// !Setters

	// Getters
	CString GetPath() const;
	bool IsLoggedIn() const { return m_bLoggedIn; }
	const CString& GetDocRoot() const;
	const CString& GetUser() const;
	const CString& GetPass() const;
	const CString& GetParamString() const;
	const CString& GetContentType() const;
	bool IsPost() const;
	// !Getters

	// Parameter access
	CString GetParam(const CString& sName, bool bPost = true, const CString& sFilter = "\r\n") const;
	CString GetRawParam(const CString& sName, bool bPost = true) const;
	bool HasParam(const CString& sName, bool bPost = true) const;
	const map<CString, VCString>& GetParams(bool bPost = true) const;
	unsigned int GetParamValues(const CString& sName, VCString& vsRet, bool bPost = true, const CString& sFilter = "\r\n") const;
	unsigned int GetParamValues(const CString& sName, set<CString>& ssRet, bool bPost = true, const CString& sFilter = "\r\n") const;
	// !Parameter access
private:
	static CString GetRawParam(const CString& sName, const map<CString, VCString>& msvsParams);
	static CString GetParam(const CString& sName, const map<CString, VCString>& msvsParams, const CString& sFilter);
	static unsigned int GetParamValues(const CString& sName, VCString& vsRet, const map<CString, VCString>& msvsParams, const CString& sFilter);
	static unsigned int GetParamValues(const CString& sName, set<CString>& ssRet, const map<CString, VCString>& msvsParams, const CString& sFilter);

protected:
	void PrintPage(const CString& sPage);
	void Init();

	bool                     m_bSentHeader;
	bool                     m_bGotHeader;
	bool                     m_bLoggedIn;
	bool                     m_bPost;
	bool                     m_bDone;
	unsigned long            m_uPostLen;
	CString                  m_sPostData;
	CString                  m_sURI;
	CString                  m_sUser;
	CString                  m_sPass;
	CString                  m_sContentType;
	CString                  m_sDocRoot;
	map<CString, VCString>   m_msvsPOSTParams;
	map<CString, VCString>   m_msvsGETParams;
	MCString                 m_msHeaders;
	bool                     m_bHTTP10Client;
	CString                  m_sIfNoneMatch;
	MCString                 m_msRequestCookies;
	MCString                 m_msResponseCookies;
};

#endif // !_HTTPSOCK_H
