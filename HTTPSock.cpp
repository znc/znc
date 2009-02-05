/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifdef _MODULES

#include "HTTPSock.h"
#include "znc.h"

CHTTPSock::CHTTPSock(CModule *pMod) : CSocket(pMod) {
	Init();
}

CHTTPSock::CHTTPSock(CModule *pMod, const CString& sHostname, unsigned short uPort, int iTimeout) : CSocket(pMod, sHostname, uPort, iTimeout) {
	Init();
}

void CHTTPSock::Init() {
	m_bSentHeader = false;
	m_bGotHeader = false;
	m_bLoggedIn = false;
	m_bPost = false;
	m_bDone = false;
	m_bHTTP10Client = false;
	m_uPostLen = 0;
	EnableReadLine();
	SetMaxBufferThreshold(10240);
}

CHTTPSock::~CHTTPSock() {}

void CHTTPSock::ReadData(const char* data, int len) {
	string s;
	s.append(data, len);

	if (!m_bDone && m_bGotHeader && m_bPost) {
		m_sPostData.append(data, len);
		CheckPost();
	}
}

void CHTTPSock::CheckPost() {
	if (m_sPostData.size() >= m_uPostLen) {
		ParseParams(m_sPostData.Left(m_uPostLen));
		GetPage();
		m_sPostData.clear();
		m_bDone = true;
		Close(Csock::CLT_AFTERWRITE);
	}
}

void CHTTPSock::ReadLine(const CString& sData) {
	CString sLine = sData;
	sLine.TrimRight("\r\n");

	if (m_bGotHeader) {
		return;
	}

	CString sName = sLine.Token(0);

	if (sName.Equals("GET")) {
		m_bPost = false;
		m_sURI = sLine.Token(1);
		m_bHTTP10Client = sLine.Token(2).Equals("HTTP/1.0");
		ParseURI();
	} else if (sName.Equals("POST")) {
		m_bPost = true;
		m_sURI = sLine.Token(1);
		ParseURI();
	} else if (sName.Equals("Authorization:")) {
		CString sUnhashed;
		sLine.Token(2).Base64Decode(sUnhashed);
		m_sUser = sUnhashed.Token(0, false, ":");
		m_sPass = sUnhashed.Token(1, true, ":");
		m_bLoggedIn = OnLogin(m_sUser, m_sPass);
	} else if (sName.Equals("Content-Length:")) {
		m_uPostLen = sLine.Token(1).ToULong();
	} else if (sName.Equals("If-None-Match:")) {
		// this is for proper client cache support (HTTP 304) on static files:
		m_sIfNoneMatch = sLine.Token(1, true);
	} else if (sLine.empty()) {
		m_bGotHeader = true;
		DisableReadLine();

		if (m_bPost) {
			m_sPostData = GetInternalReadBuffer();
			CheckPost();
		} else {
			GetPage();
		}
	}
}

void CHTTPSock::GetPage() {
	CString sPage;

	DEBUG("Page Request [" << m_sURI << "] ");

	if (!OnPageRequest(m_sURI, sPage)) {
		PrintNotFound();
		return;
	}

	if (PrintHeader(sPage.length())) {
		Write(sPage);
		Close(Csock::CLT_AFTERWRITE);
	}
}

bool CHTTPSock::PrintFile(const CString& sFileName, CString sContentType) {
	CString sFilePath = sFileName;

	if (!m_sDocRoot.empty()) {
		sFilePath.TrimLeft("/");

		sFilePath = CDir::ChangeDir(m_sDocRoot, sFilePath, m_sDocRoot);

		if (sFilePath.Left(m_sDocRoot.size()) != m_sDocRoot) {
			PrintErrorPage(403, "Forbidden", "You don't have permission to access that file on this server.");
			DEBUG("THIS FILE:     [" << sFilePath << "] does not live in ...");
			DEBUG("DOCUMENT ROOT: [" << m_sDocRoot << "]");
			return false;
		}
	}

	CFile File(sFilePath);

	if (!File.Open()) {
		PrintNotFound();
		return false;
	}

	if (sContentType.empty()) {
		if (sFileName.Right(5).Equals(".html") || sFileName.Right(4).Equals(".htm")) {
			sContentType = "text/html";
		} else if (sFileName.Right(4).Equals(".css")) {
			sContentType = "text/css";
		} else if (sFileName.Right(3).Equals(".js")) {
			sContentType = "application/x-javascript";
		} else if (sFileName.Right(4).Equals(".jpg")) {
			sContentType = "image/jpeg";
		} else if (sFileName.Right(4).Equals(".gif")) {
			sContentType = "image/gif";
		} else if (sFileName.Right(4).Equals(".ico")) {
			sContentType = "image/x-icon";
		} else if (sFileName.Right(4).Equals(".png")) {
			sContentType = "image/png";
		} else if (sFileName.Right(4).Equals(".bmp")) {
			sContentType = "image/bmp";
		} else {
			sContentType = "text/plain";
		}
	}

	const time_t iMTime = File.GetMTime();
	bool bNotModified = false;
	CString sETag;

	if (iMTime > 0 && !m_bHTTP10Client) {
		sETag = "-" + CString(iMTime); // lighttpd style ETag

		AddHeader("ETag", "\"" + sETag + "\"");
		AddHeader("Cache-Control", "private");

		if (!m_sIfNoneMatch.empty()) {
			m_sIfNoneMatch.Trim("\\\"'");
			bNotModified = (m_sIfNoneMatch.Equals(sETag, true));
		}
	}

	if (bNotModified) {
		PrintHeader(0, sContentType, 304, "Not Modified");
	} else {
		char szBuf[4096];
		int iLen = 0;

		PrintHeader(File.GetSize(), sContentType);

		while ((iLen = File.Read(szBuf, 4096)) > 0) {
			Write(szBuf, iLen);
		}
	}

	DEBUG("- ETag: [" << sETag << "] / If-None-Match [" << m_sIfNoneMatch << "]");

	Close(Csock::CLT_AFTERWRITE);

	return true;
}

void CHTTPSock::ParseURI() {
	ParseParams(m_sURI.Token(1, true, "?"));
	m_sURI = m_sURI.Token(0, false, "?");
}

void CHTTPSock::ParseParams(const CString& sParams) {
	m_msvsParams.clear();

	VCString vsPairs;
	sParams.Split("&", vsPairs, true);

	for (unsigned int a = 0; a < vsPairs.size(); a++) {
		const CString& sPair = vsPairs[a];
		CString sName = sPair.Token(0, false, "=").Escape_n(CString::EURL, CString::EASCII);
		CString sValue = sPair.Token(1, true, "=").Escape_n(CString::EURL, CString::EASCII);

		m_msvsParams[sName].push_back(sValue);
	}
}

void CHTTPSock::SetDocRoot(const CString& s) {
	m_sDocRoot = s + "/";
	m_sDocRoot.Replace("//", "/");
}

const CString& CHTTPSock::GetDocRoot() const {
	return m_sDocRoot;
}

const CString& CHTTPSock::GetUser() const {
	return m_sUser;
}

const CString& CHTTPSock::GetPass() const {
	return m_sPass;
}

const CString& CHTTPSock::GetContentType() const {
	return m_sContentType;
}

const CString& CHTTPSock::GetParamString() const {
	return m_sPostData;
}

bool CHTTPSock::HasParam(const CString& sName) const {
	return (m_msvsParams.find(sName) != m_msvsParams.end());
}

CString CHTTPSock::GetParam(const CString& sName) const {
	CString sRet;

	VCString vsParams;
	if (GetParamValues(sName, vsParams)) {
		sRet = vsParams[0];
	}

	return sRet;
}

unsigned int CHTTPSock::GetParamValues(const CString& sName, set<CString>& ssRet) const {
	ssRet.clear();

	map<CString, VCString>::const_iterator it = m_msvsParams.find(sName);

	if (it != m_msvsParams.end()) {
		for (unsigned int a = 0; a < it->second.size(); a++) {
			ssRet.insert(it->second[a]);
		}
	}

	return ssRet.size();
}

unsigned int CHTTPSock::GetParamValues(const CString& sName, VCString& vsRet) const {
	vsRet.clear();

	map<CString, VCString>::const_iterator it = m_msvsParams.find(sName);

	if (it != m_msvsParams.end()) {
		vsRet = it->second;
	}

	return vsRet.size();
}

const map<CString, VCString>& CHTTPSock::GetParams() const {
	return m_msvsParams;
}

bool CHTTPSock::PrintNotFound() {
	return PrintErrorPage(404, "Not Found", "The requested URL was not found on this server.");
}

bool CHTTPSock::PrintErrorPage(unsigned int uStatusId, const CString& sStatusMsg, const CString& sMessage) {
	if (SentHeader()) {
		return false;
	}

	CString sPage = GetErrorPage(uStatusId, sStatusMsg, sMessage);
	PrintHeader(sPage.length(), "text/html", uStatusId, sStatusMsg);
	Write(sPage);
	Close(Csock::CLT_AFTERWRITE);

	return true;
}

CString CHTTPSock::GetErrorPage(unsigned int uStatusId, const CString& sStatusMsg, const CString& sMessage) {
	return "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\r\n"
		"<html><head>\r\n<title>" + CString(uStatusId) + " " + sStatusMsg.Escape_n(CString::EHTML) + "</title>\r\n"
		"</head><body>\r\n<h1>" + sStatusMsg.Escape_n(CString::EHTML) + "</h1>\r\n"
		"<p>" + sMessage.Escape_n(CString::EHTML) + "</p>\r\n"
		"<hr>\r\n<address>" + CZNC::GetTag().Escape_n(CString::EHTML) + " at " + GetLocalIP().Escape_n(CString::EHTML) + " Port " + CString(GetLocalPort()) + "</address>\r\n"
		"</body></html>\r\n";
}

bool CHTTPSock::ForceLogin() {
	if (m_bLoggedIn) {
		return true;
	}

	if (SentHeader()) {
		return false;
	}

	CString sPage = GetErrorPage(401, "Unauthorized", "You need to login to view this page.");
	AddHeader("WWW-Authenticate", "Basic realm=\"" + CZNC::GetTag() + "\"");
	PrintHeader(sPage.length(), "text/html", 401, "Unauthorized");
	Write(sPage);
	Close(Csock::CLT_AFTERWRITE);

	return false;
}

bool CHTTPSock::OnLogin(const CString& sUser, const CString& sPass) {
	return false;
}

bool CHTTPSock::SentHeader() const {
	return m_bSentHeader;
}

bool CHTTPSock::PrintHeader(unsigned long uContentLength, const CString& sContentType, unsigned int uStatusId, const CString& sStatusMsg) {
	if (SentHeader()) {
		DEBUG("- Header already sent!");
		return false;
	}

	if (!sContentType.empty()) {
		m_sContentType = sContentType;
	}

	if (m_sContentType.empty()) {
		m_sContentType = "text/html";
	}

	DEBUG("- " << uStatusId << " (" << sStatusMsg << ") [" << m_sContentType << "]");

	Write("HTTP/" + CString(m_bHTTP10Client ? "1.0 " : "1.1 ") + CString(uStatusId) + " " + sStatusMsg + "\r\n");
	//Write("Date: Tue, 28 Jun 2005 20:45:36 GMT\r\n");
	Write("Server: " + CZNC::GetTag() + "\r\n");
	if (uContentLength > 0) {
		Write("Content-Length: " + CString(uContentLength) + "\r\n");
	}
	Write("Content-Type: " + m_sContentType + "\r\n");

	for (MCString::iterator it = m_msHeaders.begin(); it != m_msHeaders.end(); it++) {
		Write(it->first + ": " + it->second + "\r\n");
	}

	Write("Connection: Close\r\n");

	Write("\r\n");
	m_bSentHeader = true;

	return true;
}

void CHTTPSock::SetContentType(const CString& sContentType) {
	m_sContentType = sContentType;
}

void CHTTPSock::AddHeader(const CString& sName, const CString& sValue) {
	m_msHeaders[sName] = sValue;
}

bool CHTTPSock::Redirect(const CString& sURL) {
	if (SentHeader()) {
		return false;
	}

	DEBUG("- Redirect to [" << sURL << "]");
	CString sPage = GetErrorPage(302, "Found", "The document has moved <a href=\"" + sURL.Escape_n(CString::EHTML) + "\">here</a>.");
	AddHeader("Location", sURL);
	PrintHeader(sPage.length(), "text/html", 302, "Found");
	Write(sPage);
	Close(Csock::CLT_AFTERWRITE);

	return true;
}

void CHTTPSock::Timeout() {
}

void CHTTPSock::SockError(int iErrno) {
}

void CHTTPSock::Connected() {
	SetTimeout(120);
}

void CHTTPSock::Disconnected() {
}

void CHTTPSock::ReachedMaxBuffer() {
	DEBUG(GetSockName() << " == ReachedMaxBuffer()");
	Close();
}

#endif // _MODULES
