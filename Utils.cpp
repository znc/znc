#include "Utils.h"
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sstream>
using std::stringstream;

CUtils::CUtils() {}
CUtils::~CUtils() {}

#ifdef __sun
char *strcasestr(const char *big, const char *little)
{
	int len;

	if (!little || !big || !little[0])
		return (char *) big;

	len = strlen(little);
	while (*big)
	{
		if (tolower(*big) == tolower(*little))
		{
			if (strncasecmp(big, little, len) == 0)
				return (char *) big;
		}

		big++;
	}

	return NULL;
}
#endif /* __sun */

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

string CUtils::ChangeDir(const string& sPath, const string& sAdd, const string& sHomeDir) {
	if (sAdd == "~") {
		return sHomeDir;
	}

	string sAddDir = sAdd;

	if (CUtils::Left(sAddDir, 2) == "~/") {
		CUtils::LeftChomp(sAddDir);
		sAddDir = sHomeDir + sAddDir;
	}

	string sRet = ((sAddDir.size()) && (sAddDir[0] == '/')) ? "" : sPath;
	sAddDir += "/";
	string sCurDir;

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

	string sWorkDir = sDir.substr(0, iFind + 1);  // include the trailing slash
	string sNewDir = sDir.erase(0, iFind + 1);

	struct stat st;

	if (sWorkDir.length() > 1) {
		sWorkDir = sWorkDir.erase( sWorkDir.length() - 1, 1 );  // trim off the trailing slash
	}

	if (stat(sWorkDir.c_str(), &st) == 0)
	{ 
		int iChdir = chdir(sWorkDir.c_str());
		if (iChdir != 0) {
			return iChdir;   // could not change to dir
		}

		// go ahead and call the next step
		return MakeDir(sNewDir.c_str(), iMode);
	}

	switch(errno)
	{
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

string CUtils::Token(const string& s, unsigned int uPos, bool bRest, char cSep) {
	string sRet;
	const char* p = s.c_str();

	while (*p) {
		if (uPos) {
			if (*p == cSep) {
				uPos--;
			}
		} else {
			if ((*p == cSep) && (!bRest)) {
				return sRet;
			}

			sRet += *p;
		}

		p++;
	}

	return sRet;
}

string CUtils::Ellipsize(const string& s, unsigned int uLen) {
	if (uLen >= s.size()) {
		return s;
	}

	string sRet;

	if (uLen < 4) {
		for (unsigned int a = 0; a < uLen; a++) {
			sRet += ".";
		}

		return sRet;
	}

	sRet = s.substr(0, uLen -3) + "...";

	return sRet;
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


CFile::CFile(const string& sLongName) {
	m_sLongName = sLongName;
	m_iFD = -1;

	m_sShortName = sLongName;

	while (CUtils::Left(m_sShortName, 1) == "/") {
		CUtils::LeftChomp(m_sShortName);
	}

	unsigned int uPos = m_sShortName.rfind('/');
	if (uPos != string::npos) {
		m_sShortName = m_sShortName.substr(uPos +1);
	}
}	
CFile::~CFile() {
	if (m_iFD != -1) {
		Close();
	}
}

bool CFile::IsReg(const string& sLongName, bool bUseLstat) { return CFile::FType(sLongName, FT_REGULAR, bUseLstat); }
bool CFile::IsDir(const string& sLongName, bool bUseLstat) { return CFile::FType(sLongName, FT_DIRECTORY, bUseLstat); }
bool CFile::IsChr(const string& sLongName, bool bUseLstat)  { return CFile::FType(sLongName, FT_CHARACTER, bUseLstat); }
bool CFile::IsBlk(const string& sLongName, bool bUseLstat)  { return CFile::FType(sLongName, FT_BLOCK, bUseLstat); }
bool CFile::IsFifo(const string& sLongName, bool bUseLstat)  { return CFile::FType(sLongName, FT_FIFO, bUseLstat); }
bool CFile::IsLnk(const string& sLongName, bool bUseLstat)  { return CFile::FType(sLongName, FT_LINK, bUseLstat); }
bool CFile::IsSock(const string& sLongName, bool bUseLstat)  { return CFile::FType(sLongName, FT_SOCK, bUseLstat); }

bool CFile::IsReg(bool bUseLstat) { return CFile::IsReg(m_sLongName, bUseLstat); }
bool CFile::IsDir(bool bUseLstat) { return CFile::IsDir(m_sLongName, bUseLstat); }
bool CFile::IsChr(bool bUseLstat)  { return CFile::IsChr(m_sLongName, bUseLstat); }
bool CFile::IsBlk(bool bUseLstat)  { return CFile::IsBlk(m_sLongName, bUseLstat); }
bool CFile::IsFifo(bool bUseLstat)  { return CFile::IsFifo(m_sLongName, bUseLstat); }
bool CFile::IsLnk(bool bUseLstat)  { return CFile::IsLnk(m_sLongName, bUseLstat); }
bool CFile::IsSock(bool bUseLstat)  { return CFile::IsSock(m_sLongName, bUseLstat); }

bool CFile::access(int mode) { return (::access(m_sLongName.c_str(), mode) == 0); }

// for gettin file types, using fstat instead
bool CFile::FType(const string sFileName, EFileTypes eType, bool bUseLstat) {
	struct stat st;

	if (!bUseLstat) {
		if (stat(sFileName.c_str(), &st) != 0) {
			return false;
		}
	} else {
		if (lstat(sFileName.c_str(), &st) != 0) {
			return false;
		}
	}

	switch (eType) {
		case FT_REGULAR:
			return S_ISREG(st.st_mode);
		case FT_DIRECTORY:
			return S_ISDIR(st.st_mode);
		case FT_CHARACTER:
			return S_ISCHR(st.st_mode);
		case FT_BLOCK:
			return S_ISBLK(st.st_mode);
		case FT_FIFO:
			return S_ISFIFO(st.st_mode);
		case FT_LINK:
			return S_ISLNK(st.st_mode);
		case FT_SOCK:
			return S_ISSOCK(st.st_mode);
		default:
			return false;
	}
	return false;
}

//
// Functions to retrieve file information
//
bool CFile::Exists() const { return CFile::Exists(m_sLongName); }
unsigned long long CFile::GetSize() const { return CFile::GetSize(m_sLongName); }
unsigned int CFile::GetATime() const { return CFile::GetATime(m_sLongName); }
unsigned int CFile::GetMTime() const { return CFile::GetMTime(m_sLongName); }
unsigned int CFile::GetCTime() const { return CFile::GetCTime(m_sLongName); }
int CFile::GetUID() const { return CFile::GetUID(m_sLongName); }
int CFile::GetGID() const { return CFile::GetGID(m_sLongName); }
bool CFile::Exists(const string& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) == 0);
}

unsigned long long CFile::GetSize(const string& sFile) {
	struct stat st;
	if(stat(sFile.c_str(), &st) != 0) {
		return 0;
	}

	return (S_ISREG(st.st_mode)) ? st.st_size : 0;
}

unsigned int CFile::GetATime(const string& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? 0 : st.st_atime;
}

unsigned int CFile::GetMTime(const string& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? 0 : st.st_mtime;
}

unsigned int CFile::GetCTime(const string& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? 0 : st.st_ctime;
}

int CFile::GetUID(const string& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? -1 : (int) st.st_uid;
}

int CFile::GetGID(const string& sFile) {
	struct stat st;
	return (stat(sFile.c_str(), &st) != 0) ? -1 : (int) st.st_gid;
}
int CFile::GetInfo(const string& sFile, struct stat& st) {
	return stat(sFile.c_str(), &st);
}

//
// Functions to manipulate the file on the filesystem
//
int CFile::Delete() { return CFile::Delete(m_sLongName); }
int CFile::Move(const string& sNewFileName, bool bOverwrite) {
	return CFile::Move(m_sLongName, sNewFileName, bOverwrite);
}

bool CFile::Delete(const string& sFileName) {
	if(!CFile::Exists(sFileName)) {
		return false;
	}

	return (unlink(sFileName.c_str()) == 0) ? true : false;
}

bool CFile::Move(const string& sOldFileName, const string& sNewFileName, bool bOverwrite) {
	if((!bOverwrite) && (CFile::Exists(sNewFileName))) {
		return false;
	}

	//string sNewLongName = (sNewFileName[0] == '/') ? sNewFileName : m_sPath + "/" + sNewFileName;
	return (rename(sOldFileName.c_str(), sNewFileName.c_str()) == 0) ? true : false;
}

bool CFile::Chmod(mode_t mode) {
	return CFile::Chmod(m_sLongName, mode);
}

bool CFile::Chmod(const string& sFile, mode_t mode) {
	return (chmod(sFile.c_str(), mode) == 0);
}

bool CFile::Seek(unsigned long uPos) {
	return (m_iFD == -1) ? false : ((unsigned int) lseek(m_iFD, uPos, SEEK_SET) == uPos);
}

bool CFile::Open(int iFlags, mode_t iMode) {
	if (m_iFD != -1) {
		return false;
	}

	m_iFD = open(m_sLongName.c_str(), iFlags, iMode);
	return  (m_iFD > -1);
}

int CFile::Read(char *pszBuffer, int iBytes) {
	if (m_iFD == -1) {
		return -1;
	}

	return read(m_iFD, pszBuffer, iBytes);
}

bool CFile::ReadLine(string & sData) {
	char buff[64];
	sData.clear();
	if (m_iFD == -1) {
		return false;
	}

	bool bEOF = false;

	while(true) {
		u_int iFind = m_sBuffer.find("\n");
		if (iFind != string::npos) {
			sData = m_sBuffer.substr(0, (iFind + 1));
			m_sBuffer.erase(0, (iFind + 1));
			break;
		}

		memset((char *)buff, '\0', 64);
		int iBytes = read(m_iFD, buff, 64);
		switch(iBytes) {
			case -1: {
				bEOF = true;
				break;
			}
			case 0: {
				bEOF = true;
				break;
			}
			default: {
				m_sBuffer.append(buff, iBytes);
				break;
			}
		}

		if (bEOF) {
			break;
		}
	}

	u_int iFind = m_sBuffer.find("\n");
	if (iFind != string::npos) {
		return true;
	}

	return !bEOF;
}

int CFile::Write(const char *pszBuffer, u_int iBytes) {
	if (m_iFD == -1) {
		return -1;
	}

	return write(m_iFD, pszBuffer, iBytes);
}

int CFile::Write(const string & sData) {
	return Write(sData.data(), sData.size());
}
void CFile::Close() { close(m_iFD); m_iFD = -1; }

string CFile::GetLongName() const { return m_sLongName; }
string CFile::GetShortName() const { return m_sShortName; }
void CFile::SetFD(int iFD) { m_iFD = iFD; }


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

