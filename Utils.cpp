#include "Utils.h"
#include "md5.h"
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sstream>
#include <errno.h>
#ifdef HAVE_LIBSSL
#include <openssl/ssl.h>
#endif /* HAVE_LIBSSL */

using std::stringstream;

CUtils::CUtils() {}
CUtils::~CUtils() {}

#ifdef __sun
char *strcasestr(const char *big, const char *little) {
	int len;

	if (!little || !big || !little[0]) {
		return (char *) big;
	}

	len = strlen(little);
	while (*big) {
		if (tolower(*big) == tolower(*little)) {
			if (strncasecmp(big, little, len) == 0) {
				return (char *) big;
			}
		}

		big++;
	}

	return NULL;
}
#endif /* __sun */

#ifdef HAVE_LIBSSL
void CUtils::GenerateCert(FILE *pOut, bool bEncPrivKey) {
	RSA *pRSA = RSA_generate_key(1024, 17, NULL, NULL);
	PEM_write_RSAPrivateKey(pOut, pRSA, (bEncPrivKey ? EVP_des_ede3_cbc() : NULL), NULL, 0, NULL, NULL);

	X509_REQ *pReq = X509_REQ_new();
	EVP_PKEY *pKey = EVP_PKEY_new();
	X509_NAME *pName = X509_NAME_new();

	EVP_PKEY_assign(pKey, EVP_PKEY_RSA, (char *)pRSA);
	X509_REQ_set_pubkey(pReq, pKey);

	char *pLogName = getenv("LOGNAME");
	char *pHostName = getenv("HOSTNAME");

	if (!pLogName) {
		pLogName = "Unknown";
	}

	if (!pHostName) {
		pHostName = "unknown.com";
	}

	string sEmailAddr = pLogName;
	sEmailAddr += "@";
	sEmailAddr += pHostName;

	X509_NAME_add_entry_by_txt(pName, "C", MBSTRING_ASC, (unsigned char *)"SomeCountry", -1, -1, 0);
	X509_NAME_add_entry_by_txt(pName, "ST", MBSTRING_ASC, (unsigned char *)"SomeState", -1, -1, 0);
	X509_NAME_add_entry_by_txt(pName, "L", MBSTRING_ASC, (unsigned char *)"SomeCity", -1, -1, 0);
	X509_NAME_add_entry_by_txt(pName, "O", MBSTRING_ASC, (unsigned char *)"SomeCompany", -1, -1, 0);
	X509_NAME_add_entry_by_txt(pName, "OU", MBSTRING_ASC, (unsigned char *)pLogName, -1, -1, 0);
	X509_NAME_add_entry_by_txt(pName, "CN", MBSTRING_ASC, (unsigned char *)pHostName, -1, -1, 0);
	X509_NAME_add_entry_by_txt(pName, "emailAddress", MBSTRING_ASC, (unsigned char *)sEmailAddr.c_str(), -1, -1, 0);
	X509_REQ_set_subject_name(pReq, pName);
	X509_REQ_sign(pReq, pKey, EVP_md5());

	X509 *pX509 = X509_REQ_to_X509(pReq, 365, pKey);
	if (pX509) {
		PEM_write_X509(pOut, pX509);
		X509_free(pX509);
	}

	X509_REQ_free(pReq);
	EVP_PKEY_free(pKey);
};
#endif /* HAVE_LIBSSL */

string CUtils::GetIP(unsigned long addr) {
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

unsigned long CUtils::GetLongIP(const string& sIP) {
	register int i;
	char *addr = (char *) malloc(sIP.length() +1);
	char ip[4][4], n;

	strcpy(addr, sIP.c_str());

	for (i=0; i<4; ip[0][i]=ip[1][i]=ip[2][i]=ip[3][i]='\0', i++);

	if (sscanf(addr, "%3[0-9].%3[0-9].%3[0-9].%3[0-9]%[^\n]", ip[0], ip[1], ip[2], ip[3], &n) != 4) {
		free(addr);
		return 0;
	}

	free(addr);
	return (unsigned long) ((atoi(ip[0]) << 24) + (atoi(ip[1]) << 16) + (atoi(ip[2]) << 8) + atoi(ip[3]));
}

CString CUtils::ChangeDir(const CString& sPath, const CString& sAdd, const CString& sHomeDir) {
	if (sAdd == "~") {
		return sHomeDir;
	}

	CString sAddDir = sAdd;

	if (CUtils::Left(sAddDir, 2) == "~/") {
		CUtils::LeftChomp(sAddDir);
		sAddDir = sHomeDir + sAddDir;
	}

	CString sRet = ((sAddDir.size()) && (sAddDir[0] == '/')) ? "" : sPath;
	sAddDir += "/";
	CString sCurDir;

	if (CUtils::Right(sRet, 1) == "/") {
		CUtils::RightChomp(sRet);
	}

	for (unsigned int a = 0; a < sAddDir.size(); a++) {
		switch (sAddDir[a]) {
			case '/':
				if (sCurDir == "..") {
					sRet = sRet.substr(0, sRet.rfind('/'));
				} else if ((sCurDir != "") && (sCurDir != ".")) {
					sRet += "/" + sCurDir;
				}

				sCurDir = "";
				break;
			default:
				sCurDir += sAddDir[a];
				break;
		}
	}

	return (sRet.empty()) ? "/" : sRet;
}

int CUtils::MakeDir(const string& sPath, mode_t iMode) {
	string sDir = sPath;
	string::size_type iFind = sDir.find("/");

	if (iFind == string::npos) {
		return mkdir(sDir.c_str(), iMode);
	}
	iFind++;

	while ((iFind < sDir.length()) && (sDir[iFind] == '/')) {
		iFind++; // eat up extra /'s
	}

	if (iFind >= sDir.length()) {
		return mkdir(sDir.c_str(), iMode);
	}

	string sWorkDir = sDir.substr(0, iFind);  // include the trailing slash
	string sNewDir = sDir.erase(0, iFind);

	struct stat st;

	if (sWorkDir.length() > 1) {
		sWorkDir = sWorkDir.erase(sWorkDir.length() - 1, 1);  // trim off the trailing slash
	}

	if (stat(sWorkDir.c_str(), &st) == 0) {
		int iChdir = chdir(sWorkDir.c_str());
		if (iChdir != 0) {
			return iChdir;   // could not change to dir
		}

		// go ahead and call the next step
		return MakeDir(sNewDir.c_str(), iMode);
	}

	switch(errno) {
		case ENOENT: {
			// ok, file doesn't exists, lets create it and cd into it
			int iMkdir = mkdir(sWorkDir.c_str(), iMode);
			if (iMkdir != 0) {
				return iMkdir; // could not create dir
			}

			int iChdir = chdir(sWorkDir.c_str());
			if (iChdir != 0) {
				return iChdir;       // could not change to dir
			}

			return MakeDir(sNewDir.c_str(), iMode);
		}
		default:
			break;
	}

	return -1;
}

string CUtils::GetHashPass() {
	while (true) {
		char* pass = CUtils::GetPass("Enter Password");
		char* pass1 = (char*) malloc(strlen(pass) +1);
		strcpy(pass1, pass);	// Make a copy of this since it is stored in a static buffer and will be overwritten when we fill pass2 below
		memset((char*) pass, 0, strlen(pass));	// null out our pass so it doesn't sit in memory
		char* pass2 = CUtils::GetPass("Confirm Password");
		int iLen = strlen(pass1);

		if (strcmp(pass1, pass2) != 0) {
			CUtils::PrintError("The supplied passwords did not match");
		} else if (!iLen) {
			CUtils::PrintError("You can not use an empty password");
		} else {
			string sRet((const char*) CMD5(pass1, iLen));
			memset((char*) pass1, 0, iLen);	// null out our pass so it doesn't sit in memory
			memset((char*) pass2, 0, strlen(pass2));	// null out our pass so it doesn't sit in memory
			free(pass1);

			return sRet;
		}

		memset((char*) pass1, 0, iLen);	// null out our pass so it doesn't sit in memory
		memset((char*) pass2, 0, strlen(pass2));	// null out our pass so it doesn't sit in memory
		free(pass1);
	}

	return "";
}

char* CUtils::GetPass(const string& sPrompt) {
	PrintPrompt(sPrompt);
	return getpass("");
}

bool CUtils::GetBoolInput(const string& sPrompt, bool bDefault) {
	return CUtils::GetBoolInput(sPrompt, &bDefault);
}

bool CUtils::GetBoolInput(const string& sPrompt, bool *pbDefault) {
	string sRet, sDefault;

	if (pbDefault) {
		sDefault = (*pbDefault) ? "yes" : "no";
	}

	GetInput(sPrompt, sRet, sDefault, "yes/no");

	if (strcasecmp(sRet.c_str(), "yes") == 0) {
		return true;
	} else if (strcasecmp(sRet.c_str(), "no") == 0) {
		return false;
	}

	return GetBoolInput(sPrompt, pbDefault);
}

bool CUtils::GetNumInput(const string& sPrompt, unsigned int& uRet, unsigned int uMin, unsigned int uMax, unsigned int uDefault) {
	if (uMin > uMax) {
		return false;
	}

	string sDefault = (uDefault != (unsigned int) ~0) ? CUtils::ToString(uDefault) : "";
	string sNum, sHint;

	if (uMax != (unsigned int) ~0) {
		sHint = CUtils::ToString(uMin) + " to " + CUtils::ToString(uMax);
	} else if (uMin > 0) {
		sHint = CUtils::ToString(uMin) + " and up";
	}

	while (true) {
		GetInput(sPrompt, sNum, sDefault, sHint);
		if (sNum.empty()) {
			return false;
		}

		uRet = atoi(sNum.c_str());

		if ((uRet >= uMin && uRet <= uMax)) {
			break;
		}

		CUtils::PrintError("Number must be " + sHint);
	}

	return true;
}

bool CUtils::GetInput(const string& sPrompt, string& sRet, const string& sDefault, const string& sHint) {
	string sExtra;
	sExtra += (!sHint.empty()) ? (" (" + sHint + ")") : "";
	sExtra += (!sDefault.empty()) ? (" [" + sDefault + "]") : "";

	PrintPrompt(sPrompt + sExtra);
	char szBuf[1024];
	memset(szBuf, 0, 1024);
	fgets(szBuf, 1024, stdin);
	sRet = szBuf;

	if (CUtils::Right(sRet, 1) == "\n") {
		CUtils::RightChomp(sRet);
	}

	if (sRet.empty()) {
		sRet = sDefault;
	}

	return !sRet.empty();
}

void CUtils::PrintError(const string& sMessage) {
	fprintf(stdout, "\033[1m\033[34m[\033[31m ** \033[34m]\033[39m\033[22m %s\n", sMessage.c_str());
}

void CUtils::PrintPrompt(const string& sMessage) {
	fprintf(stdout, "\033[1m\033[34m[\033[33m ?? \033[34m]\033[39m\033[22m %s: ", sMessage.c_str());
}

void CUtils::PrintMessage(const string& sMessage, bool bStrong) {
	fprintf(stdout, "\033[1m\033[34m[\033[33m ** \033[34m]\033[39m\033[22m %s%s%s\n",
		((bStrong) ? "\033[1m" : ""),
		sMessage.c_str(),
		((bStrong) ? "\033[22m" : "")
	);
}

void CUtils::PrintAction(const string& sMessage) {
	fprintf(stdout, "\033[1m\033[34m[\033[32m    \033[34m]\033[39m\033[22m %s... ", sMessage.c_str());
	fflush(stdout);
}

void CUtils::PrintStatus(bool bSuccess, const string& sMessage) {
	if (!sMessage.empty()) {
		if (bSuccess) {
			fprintf(stdout, "%s", sMessage.c_str());
		} else {
			fprintf(stdout, "\033[1m\033[34m[\033[31m %s \033[34m]\033[39m\033[22m", sMessage.c_str());
		}
	}

	fprintf(stdout, "\r");

	if (bSuccess) {
		fprintf(stdout, "\033[1m\033[34m[\033[32m ok \033[34m]\033[39m\033[22m\n");
	} else {
		fprintf(stdout, "\033[1m\033[34m[\033[31m !! \033[34m]\033[39m\033[22m\n");
	}
}

string CUtils::ToString(short i) { stringstream s; s << i; return s.str(); }
string CUtils::ToString(unsigned short i) { stringstream s; s << i; return s.str(); }
string CUtils::ToString(int i) { stringstream s; s << i; return s.str(); }
string CUtils::ToString(unsigned int i) { stringstream s; s << i; return s.str(); }
string CUtils::ToString(long i) { stringstream s; s << i; return s.str(); }
string CUtils::ToString(unsigned long i) { stringstream s; s << i; return s.str(); }
string CUtils::ToString(unsigned long long i) { stringstream s; s << i; return s.str(); }
string CUtils::ToString(double i) { stringstream s; s << i; return s.str(); }
string CUtils::ToString(float i) { stringstream s; s << i; return s.str(); }

string CUtils::ToPercent(double d) {
	char szRet[32];
	snprintf(szRet, 32, "%.02f%%", d);
	return szRet;
}

string CUtils::ToKBytes(double d) {
	char szRet[32];
	snprintf(szRet, 32, "%.0f K/s", d);
	return szRet;
}

string CUtils::Left(const string& s, unsigned int u) {
	u = (u > s.length()) ? s.length() : u;
	return s.substr(0, u);
}

string CUtils::Right(const string& s, unsigned int u) {
	u = (u > s.length()) ? s.length() : u;
	return s.substr(s.length() - u, u);
}

string& CUtils::Trim(string& s) {
	while ((Right(s, 1) == " ") || (Right(s, 1) == "\t") || (Right(s, 1) == "\r") || (Right(s, 1) == "\n")) {
		RightChomp(s);
	}

	while ((Left(s, 1) == " ") || (Left(s, 1) == "\t") || (Left(s, 1) == "\r") || (Left(s, 1) == "\n")) {
		LeftChomp(s);
	}

	return s;
}

string& CUtils::LeftChomp(string& s, unsigned int uLen) {
	while ((uLen--) && (s.length())) {
		s.erase(0, 1);
	}

	return s;
}

string& CUtils::RightChomp(string& s, unsigned int uLen) {
	while ((uLen--) && (s.length())) {
		s.erase(s.length() -1);
	}

	return s;
}

bool CUtils::wildcmp(const string& sWild, const string& sString) {
	// Written by Jack Handy - jakkhandy@hotmail.com
	const char *wild = sWild.c_str(), *string = sString.c_str();
	const char *cp = NULL, *mp = NULL;

	while ((*string) && (*wild != '*')) {
		if ((*wild != *string) && (*wild != '?')) {
			return false;
		}

		wild++;
		string++;
	}

	while (*string) {
		if (*wild == '*') {
			if (!*++wild) {
				return true;
			}

			mp = wild;
			cp = string+1;
		} else if ((*wild == *string) || (*wild == '?')) {
			wild++;
			string++;
		} else {
			wild = mp;
			string = cp++;
		}
	}

	while (*wild == '*') {
		wild++;
	}

	return (*wild == 0);
}

CTable::CTable() {}
CTable::~CTable() {
	for (unsigned int a = 0; a < size(); a++) {
		delete (*this)[a];
	}

	clear();
}

bool CTable::AddColumn(const string& sName) {
	for (unsigned int a = 0; a < m_vsHeaders.size(); a++) {
		if (strcasecmp(m_vsHeaders[a].c_str(), sName.c_str()) == 0) {
			return false;
		}
	}

	m_vsHeaders.push_back(sName);
	return true;
}

unsigned int CTable::AddRow() {
	push_back(new map<string, string>);
	return size() -1;
}

bool CTable::SetCell(const string& sColumn, const string& sValue, unsigned int uRowIdx) {
	if (uRowIdx == (unsigned int) ~0) {
		if (!size()) {
			return false;
		}

		uRowIdx = size() -1;
	}

	(*(*this)[uRowIdx])[sColumn] = sValue;
	return true;
}

bool CTable::GetLine(unsigned int uIdx, string& sLine) {
	stringstream ssRet;

	if (!size()) {
		return false;
	}

	if (uIdx == 1) {
		m_msuWidths.clear();	// Clear out the width cache
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
			map<string, string>* pRow = (*this)[uIdx];
			ssRet.fill(' ');
			ssRet << "| ";

			for (unsigned int c = 0; c < m_vsHeaders.size(); c++) {
				ssRet.width(GetColumnWidth(c));
				ssRet << std::left << (*pRow)[m_vsHeaders[c]];
				ssRet << ((c == m_vsHeaders.size() -1) ? " |" : " | ");
			}

			sLine = ssRet.str();
			return true;
		}
	}

	return false;
}

/*
bool CTable::Output(std::ostream oOut) {
	stringstream ssSep;

	ssSep << "-+-";

	oOut << endl << ssSep.str() << endl;

	for (unsigned int b = 0; b < size(); b++) {
		map<string, string>* pRow = (*this)[b];

		oOut << " | ";

		for (unsigned int c = 0; c < m_vsHeaders.size(); c++) {
			oOut.width(GetColumnWidth(c));
			oOut << (*pRow)[m_vsHeaders[c]];
			oOut << " | ";
		}

		oOut << endl;
	}

	oOut << ssSep.str() << endl;
	return true;
}
*/

unsigned int CTable::GetColumnWidth(unsigned int uIdx) {
	if (uIdx >= m_vsHeaders.size()) {
		return 0;
	}

	const string& sColName = m_vsHeaders[uIdx];
	unsigned int uRet = sColName.size();
	map<string, unsigned int>::iterator it = m_msuWidths.find(sColName);

	if (it != m_msuWidths.end()) {
		return it->second;
	}

	for (unsigned int a = 0; a < size(); a++) {
		map<string, string>* pRow = (*this)[a];
		uRet = uRet >? (*pRow)[m_vsHeaders[uIdx]].size();
	}

	return uRet;
}


#ifdef HAVE_LIBSSL
CBlowfish::CBlowfish(const string & sPassword, int iEncrypt, const string & sIvec) {
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

//! returns an md5 of the string (not hex encoded)
string CBlowfish::MD5(const string & sInput, bool bHexEncode) {
	string sRet;
	unsigned char *data = MD5((const unsigned char *)sInput.data(), sInput.length());

	if (!bHexEncode) {
		sRet.append((const char *)data, MD5_DIGEST_LENGTH);
	} else {
		for(int a = 0; a < MD5_DIGEST_LENGTH; a++) {
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

string CBlowfish::Crypt(const string & sData) {
	unsigned char *buff = Crypt((unsigned char *)sData.data(), sData.length());
	string sOutput;
	sOutput.append((const char *)buff, sData.length());
	free(buff);
	return sOutput;
}

#endif // HAVE_LIBSSL

