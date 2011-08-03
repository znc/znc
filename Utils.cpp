/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Utils.h"
#include "MD5.h"
#include "main.h"
#include "ZNCDebug.h"
#include <errno.h>
#ifdef HAVE_LIBSSL
#include <openssl/ssl.h>
#endif /* HAVE_LIBSSL */
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Required with GCC 4.3+ if openssl is disabled
#include <cstring>
#include <cstdlib>

using std::stringstream;

CUtils::CUtils() {}
CUtils::~CUtils() {}

#ifdef HAVE_LIBSSL
void CUtils::GenerateCert(FILE *pOut, const CString& sHost) {
	EVP_PKEY *pKey = NULL;
	X509 *pCert = NULL;
	X509_NAME *pName = NULL;
	const int days = 365;

	u_int iSeed = time(NULL);
	int serial = (rand_r(&iSeed) % 9999);

	RSA *pRSA = RSA_generate_key(2048, 0x10001, NULL, NULL);
	if ((pKey = EVP_PKEY_new())) {
		if (!EVP_PKEY_assign_RSA(pKey, pRSA)) {
			EVP_PKEY_free(pKey);
			return;
		}

		PEM_write_RSAPrivateKey(pOut, pRSA, NULL, NULL, 0, NULL, NULL);

		if (!(pCert = X509_new())) {
			EVP_PKEY_free(pKey);
			return;
		}

		X509_set_version(pCert, 2);
		ASN1_INTEGER_set(X509_get_serialNumber(pCert), serial);
		X509_gmtime_adj(X509_get_notBefore(pCert), 0);
		X509_gmtime_adj(X509_get_notAfter(pCert), (long)60*60*24*days);
		X509_set_pubkey(pCert, pKey);

		pName = X509_get_subject_name(pCert);

		const char *pLogName = getenv("LOGNAME");
		const char *pHostName = NULL;

		if (!sHost.empty()) {
			pHostName = sHost.c_str();
		}

		if (!pHostName) {
			pHostName = getenv("HOSTNAME");
		}

		if (!pLogName) {
			pLogName = "Unknown";
		}

		if (!pHostName) {
			pHostName = "host.unknown";
		}

		CString sEmailAddr = pLogName;
		sEmailAddr += "@";
		sEmailAddr += pHostName;

		X509_NAME_add_entry_by_txt(pName, "C", MBSTRING_ASC, (unsigned char *)"US", -1, -1, 0);
		X509_NAME_add_entry_by_txt(pName, "ST", MBSTRING_ASC, (unsigned char *)"SomeState", -1, -1, 0);
		X509_NAME_add_entry_by_txt(pName, "L", MBSTRING_ASC, (unsigned char *)"SomeCity", -1, -1, 0);
		X509_NAME_add_entry_by_txt(pName, "O", MBSTRING_ASC, (unsigned char *)"SomeCompany", -1, -1, 0);
		X509_NAME_add_entry_by_txt(pName, "OU", MBSTRING_ASC, (unsigned char *)pLogName, -1, -1, 0);
		X509_NAME_add_entry_by_txt(pName, "CN", MBSTRING_ASC, (unsigned char *)pHostName, -1, -1, 0);
		X509_NAME_add_entry_by_txt(pName, "emailAddress", MBSTRING_ASC, (unsigned char *)sEmailAddr.c_str(), -1, -1, 0);

		X509_set_subject_name(pCert, pName);
		X509_set_issuer_name(pCert, pName);

		if (!X509_sign(pCert, pKey, EVP_sha1())) {
			X509_free(pCert);
			EVP_PKEY_free(pKey);
			return;
		}

		PEM_write_X509(pOut, pCert);
		X509_free(pCert);
		EVP_PKEY_free(pKey);
	}
}
#endif /* HAVE_LIBSSL */

CString CUtils::GetIP(unsigned long addr) {
	char szBuf[16];
	memset((char*) szBuf, 0, 16);

	if (addr >= (1 << 24)) {
		unsigned long ip[4];
		ip[0] = addr >> 24 & 255;
		ip[1] = addr >> 16 & 255;
		ip[2] = addr >> 8  & 255;
		ip[3] = addr       & 255;
		sprintf(szBuf, "%lu.%lu.%lu.%lu", ip[0], ip[1], ip[2], ip[3]);
	}

	return szBuf;
}

unsigned long CUtils::GetLongIP(const CString& sIP) {
	unsigned long ret;
	char ip[4][4];
	unsigned int i;

	i = sscanf(sIP.c_str(), "%3[0-9].%3[0-9].%3[0-9].%3[0-9]",
			ip[0], ip[1], ip[2], ip[3]);
	if (i != 4)
		return 0;

	// Beware that atoi("200") << 24 would overflow and turn negative!
	ret  = atol(ip[0]) << 24;
	ret += atol(ip[1]) << 16;
	ret += atol(ip[2]) << 8;
	ret += atol(ip[3]) << 0;

	return ret;
}

// If you change this here and in GetSaltedHashPass(),
// don't forget CUser::HASH_DEFAULT!
const CString CUtils::sDefaultHash = "sha256";
CString CUtils::GetSaltedHashPass(CString& sSalt) {
	sSalt = GetSalt();

	while (true) {
		CString pass1 = CUtils::GetPass("Enter Password");
		CString pass2 = CUtils::GetPass("Confirm Password");

		if (!pass1.Equals(pass2, true)) {
			CUtils::PrintError("The supplied passwords did not match");
		} else if (pass1.empty()) {
			CUtils::PrintError("You can not use an empty password");
		} else {
			// Construct the salted pass
			return SaltedSHA256Hash(pass1, sSalt);
		}
	}
}

CString CUtils::GetSalt() {
	return CString::RandomString(20);
}

CString CUtils::SaltedMD5Hash(const CString& sPass, const CString& sSalt) {
	return CString(sPass + sSalt).MD5();
}

CString CUtils::SaltedSHA256Hash(const CString& sPass, const CString& sSalt) {
	return CString(sPass + sSalt).SHA256();
}

CString CUtils::GetPass(const CString& sPrompt) {
	PrintPrompt(sPrompt);
#ifdef HAVE_GETPASSPHRASE
	return getpassphrase("");
#else
	return getpass("");
#endif
}

bool CUtils::GetBoolInput(const CString& sPrompt, bool bDefault) {
	return CUtils::GetBoolInput(sPrompt, &bDefault);
}

bool CUtils::GetBoolInput(const CString& sPrompt, bool *pbDefault) {
	CString sRet, sDefault;

	if (pbDefault) {
		sDefault = (*pbDefault) ? "yes" : "no";
	}

	while (true) {
		GetInput(sPrompt, sRet, sDefault, "yes/no");

		if (sRet.Equals("y") || sRet.Equals("yes")) {
			return true;
		} else if (sRet.Equals("n") || sRet.Equals("no")) {
			return false;
		}
	}
}

bool CUtils::GetNumInput(const CString& sPrompt, unsigned int& uRet, unsigned int uMin, unsigned int uMax, unsigned int uDefault) {
	if (uMin > uMax) {
		return false;
	}

	CString sDefault = (uDefault != (unsigned int) ~0) ? CString(uDefault) : "";
	CString sNum, sHint;

	if (uMax != (unsigned int) ~0) {
		sHint = CString(uMin) + " to " + CString(uMax);
	} else if (uMin > 0) {
		sHint = CString(uMin) + " and up";
	}

	while (true) {
		GetInput(sPrompt, sNum, sDefault, sHint);
		if (sNum.empty()) {
			return false;
		}

		uRet = sNum.ToUInt();

		if ((uRet >= uMin && uRet <= uMax)) {
			break;
		}

		CUtils::PrintError("Number must be " + sHint);
	}

	return true;
}

bool CUtils::GetInput(const CString& sPrompt, CString& sRet, const CString& sDefault, const CString& sHint) {
	CString sExtra;
	CString sInput;
	sExtra += (!sHint.empty()) ? (" (" + sHint + ")") : "";
	sExtra += (!sDefault.empty()) ? (" [" + sDefault + "]") : "";

	PrintPrompt(sPrompt + sExtra);
	char szBuf[1024];
	memset(szBuf, 0, 1024);
	if (fgets(szBuf, 1024, stdin) == NULL) {
		// Reading failed (Error? EOF?)
		PrintError("Error while reading from stdin. Exiting...");
		exit(-1);
	}
	sInput = szBuf;

	if (sInput.Right(1) == "\n") {
		sInput.RightChomp();
	}

	if (sInput.empty()) {
		sRet = sDefault;
	} else {
		sRet = sInput;
	}

	return !sRet.empty();
}

void CUtils::PrintError(const CString& sMessage) {
	if (CDebug::StdoutIsTTY())
		fprintf(stdout, "\033[1m\033[34m[\033[31m ** \033[34m]\033[39m\033[22m %s\n", sMessage.c_str());
	else
		fprintf(stdout, "%s\n", sMessage.c_str());
	fflush(stdout);
}

void CUtils::PrintPrompt(const CString& sMessage) {
	if (CDebug::StdoutIsTTY())
		fprintf(stdout, "\033[1m\033[34m[\033[33m ?? \033[34m]\033[39m\033[22m %s: ", sMessage.c_str());
	else
		fprintf(stdout, "[ ?? ] %s: ", sMessage.c_str());
	fflush(stdout);
}

void CUtils::PrintMessage(const CString& sMessage, bool bStrong) {
	if (CDebug::StdoutIsTTY()) {
		if (bStrong)
			fprintf(stdout, "\033[1m\033[34m[\033[33m ** \033[34m]\033[39m\033[22m \033[1m%s\033[22m\n",
					sMessage.c_str());
		else
			fprintf(stdout, "\033[1m\033[34m[\033[33m ** \033[34m]\033[39m\033[22m %s\n",
					sMessage.c_str());
	} else
		fprintf(stdout, "%s\n", sMessage.c_str());

	fflush(stdout);
}

void CUtils::PrintAction(const CString& sMessage) {
	if (CDebug::StdoutIsTTY())
		fprintf(stdout, "\033[1m\033[34m[\033[32m    \033[34m]\033[39m\033[22m %s... ", sMessage.c_str());
	else
		fprintf(stdout, "%s... ", sMessage.c_str());
	fflush(stdout);
}

void CUtils::PrintStatus(bool bSuccess, const CString& sMessage) {
	if (CDebug::StdoutIsTTY()) {
		if (!sMessage.empty()) {
			if (bSuccess) {
				fprintf(stdout, "%s", sMessage.c_str());
			} else {
				fprintf(stdout, "\033[1m\033[34m[\033[31m %s \033[34m]"
						"\033[39m\033[22m", sMessage.c_str());
			}
		}

		fprintf(stdout, "\r");

		if (bSuccess) {
			fprintf(stdout, "\033[1m\033[34m[\033[32m ok \033[34m]\033[39m\033[22m\n");
		} else {
			fprintf(stdout, "\033[1m\033[34m[\033[31m !! \033[34m]\033[39m\033[22m\n");
		}
	} else {
		if (bSuccess) {
			fprintf(stdout, "%s\n", sMessage.c_str());
		} else {
			if (!sMessage.empty()) {
				fprintf(stdout, "[ %s ]", sMessage.c_str());
			}

			fprintf(stdout, "\n");
		}
	}

	fflush(stdout);
}

bool CTable::AddColumn(const CString& sName) {
	for (unsigned int a = 0; a < m_vsHeaders.size(); a++) {
		if (m_vsHeaders[a].Equals(sName)) {
			return false;
		}
	}

	m_vsHeaders.push_back(sName);
	m_msuWidths[sName] = sName.size();

	return true;
}

unsigned int CTable::AddRow() {
	// Don't add a row if no headers are defined
	if (m_vsHeaders.empty()) {
		return (unsigned int) -1;
	}

	// Add a vector with enough space for each column
	push_back(vector<CString>(m_vsHeaders.size()));
	return size() - 1;
}

bool CTable::SetCell(const CString& sColumn, const CString& sValue, unsigned int uRowIdx) {
	if (uRowIdx == (unsigned int) ~0) {
		if (!size()) {
			return false;
		}

		uRowIdx = size() -1;
	}

	unsigned int uColIdx = GetColumnIndex(sColumn);

	if (uColIdx == (unsigned int) -1)
		return false;

	(*this)[uRowIdx][uColIdx] = sValue;

	if (m_msuWidths[sColumn] < sValue.size())
		m_msuWidths[sColumn] = sValue.size();

	return true;
}

bool CTable::GetLine(unsigned int uIdx, CString& sLine) const {
	stringstream ssRet;

	if (empty()) {
		return false;
	}

	if (uIdx == 1) {
		ssRet.fill(' ');
		ssRet << "| ";

		for (unsigned int a = 0; a < m_vsHeaders.size(); a++) {
			ssRet.width(GetColumnWidth(a));
			ssRet << std::left << m_vsHeaders[a];
			ssRet << ((a == m_vsHeaders.size() -1) ? " |" : " | ");
		}

		sLine = ssRet.str();
		return true;
	} else if ((uIdx == 0) || (uIdx == 2) || (uIdx == (size() +3))) {
		ssRet.fill('-');
		ssRet << "+-";

		for (unsigned int a = 0; a < m_vsHeaders.size(); a++) {
			ssRet.width(GetColumnWidth(a));
			ssRet << std::left << "-";
			ssRet << ((a == m_vsHeaders.size() -1) ? "-+" : "-+-");
		}

		sLine = ssRet.str();
		return true;
	} else {
		uIdx -= 3;

		if (uIdx < size()) {
			const vector<CString>& mRow = (*this)[uIdx];
			ssRet.fill(' ');
			ssRet << "| ";

			for (unsigned int c = 0; c < m_vsHeaders.size(); c++) {
				ssRet.width(GetColumnWidth(c));
				ssRet << std::left << mRow[c];
				ssRet << ((c == m_vsHeaders.size() -1) ? " |" : " | ");
			}

			sLine = ssRet.str();
			return true;
		}
	}

	return false;
}

unsigned int CTable::GetColumnIndex(const CString& sName) const {
	for (unsigned int i = 0; i < m_vsHeaders.size(); i++) {
		if (m_vsHeaders[i] == sName)
			return i;
	}

	DEBUG("CTable::GetColumnIndex(" + sName + ") failed");

	return (unsigned int) -1;
}

unsigned int CTable::GetColumnWidth(unsigned int uIdx) const {
	if (uIdx >= m_vsHeaders.size()) {
		return 0;
	}

	const CString& sColName = m_vsHeaders[uIdx];
	map<CString, unsigned int>::const_iterator it = m_msuWidths.find(sColName);

	if (it == m_msuWidths.end()) {
		// AddColumn() and SetCell() should make sure that we get a value :/
		return 0;
	}
	return it->second;
}

void CTable::Clear() {
	clear();
	m_vsHeaders.clear();
	m_msuWidths.clear();
}

#ifdef HAVE_LIBSSL
CBlowfish::CBlowfish(const CString & sPassword, int iEncrypt, const CString & sIvec) {
	m_iEncrypt = iEncrypt;
	m_ivec = (unsigned char *)calloc(sizeof(unsigned char), 8);
	m_num = 0;

	if (sIvec.length() >= 8) {
		memcpy(m_ivec, sIvec.data(), 8);
	}

	BF_set_key(&m_bkey, sPassword.length(), (unsigned char *)sPassword.data());
}

CBlowfish::~CBlowfish() {
	free(m_ivec);
}

//! output must be freed
unsigned char *CBlowfish::MD5(const unsigned char *input, u_int ilen) {
	unsigned char *output = (unsigned char *)malloc(MD5_DIGEST_LENGTH);
	::MD5(input, ilen, output);
	return output;
}

//! returns an md5 of the CString (not hex encoded)
CString CBlowfish::MD5(const CString & sInput, bool bHexEncode) {
	CString sRet;
	unsigned char *data = MD5((const unsigned char *)sInput.data(), sInput.length());

	if (!bHexEncode) {
		sRet.append((const char *)data, MD5_DIGEST_LENGTH);
	} else {
		for (int a = 0; a < MD5_DIGEST_LENGTH; a++) {
			sRet += g_HexDigits[data[a] >> 4];
			sRet += g_HexDigits[data[a] & 0xf];
		}
	}

	free(data);
	return sRet;
}

//! output must be the same size as input
void CBlowfish::Crypt(unsigned char *input, unsigned char *output, u_int ibytes) {
	BF_cfb64_encrypt(input, output, ibytes, &m_bkey, m_ivec, &m_num, m_iEncrypt);
}

//! must free result
unsigned char * CBlowfish::Crypt(unsigned char *input, u_int ibytes) {
	unsigned char *buff = (unsigned char *)malloc(ibytes);
	Crypt(input, buff, ibytes);
	return buff;
}

CString CBlowfish::Crypt(const CString & sData) {
	unsigned char *buff = Crypt((unsigned char *)sData.data(), sData.length());
	CString sOutput;
	sOutput.append((const char *)buff, sData.length());
	free(buff);
	return sOutput;
}

#endif // HAVE_LIBSSL
