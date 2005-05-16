#include <string.h>
#include "String.h"
#include "FileUtils.h"

int CString::CaseCmp(const CString& s) const {
	return strcasecmp(c_str(), s.c_str());
}

int CString::StrCmp(const CString& s) const {
	return strcmp(c_str(), s.c_str());
}

bool CString::WildCmp(const CString& sWild, const CString& sString) {
	// Written by Jack Handy - jakkhandy@hotmail.com
	const char *wild = sWild.c_str(), *CString = sString.c_str();
	const char *cp = NULL, *mp = NULL;

	while ((*CString) && (*wild != '*')) {
		if ((*wild != *CString) && (*wild != '?')) {
			return false;
		}

		wild++;
		CString++;
	}

	while (*CString) {
		if (*wild == '*') {
			if (!*++wild) {
				return true;
			}

			mp = wild;
			cp = CString+1;
		} else if ((*wild == *CString) || (*wild == '?')) {
			wild++;
			CString++;
		} else {
			wild = mp;
			CString = cp++;
		}
	}

	while (*wild == '*') {
		wild++;
	}

	return (*wild == 0);
}

bool CString::WildCmp(const CString& sWild) const {
	return CString::WildCmp(sWild, *this);
}

CString& CString::MakeUpper() {
	for (unsigned int a = 0; a < length(); a++) {
		char& c = (*this)[a];
		c = toupper(c);
	}

	return *this;
}

CString& CString::MakeLower() {
	for (unsigned int a = 0; a < length(); a++) {
		char& c = (*this)[a];
		c = tolower(c);
	}

	return *this;
}

CString CString::AsUpper() const {
	CString sRet = *this;

	for (unsigned int a = 0; a < length(); a++) {
		char& c = sRet[a];
		c = toupper(c);
	}

	return sRet;
}

CString CString::AsLower() const {
	CString sRet = *this;

	for (unsigned int a = 0; a < length(); a++) {
		char& c = sRet[a];
		c = tolower(c);
	}

	return sRet;
}

CString CString::Token(unsigned int uPos, bool bRest, char cSep) const {
	string sRet;
	const char* p = c_str();

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

CString CString::Ellipsize(unsigned int uLen) const {
	if (uLen >= size()) {
		return *this;
	}

	string sRet;

	// @todo this looks suspect
	if (uLen < 4) {
		for (unsigned int a = 0; a < uLen; a++) {
			sRet += ".";
		}

		return sRet;
	}

	sRet = substr(0, uLen -3) + "...";

	return sRet;
}

CString CString::Left(unsigned int uCount) const {
	uCount = (uCount > length()) ? length() : uCount;
	return substr(0, uCount);
}

CString CString::Right(unsigned int uCount) const {
	uCount = (uCount > length()) ? length() : uCount;
	return substr(length() - uCount, uCount);
}

CString CString::Format(const CString& sFormatStr, ...) {
	return "";
}

CString CString::ToString(char c) { stringstream s; s << c; return s.str(); }
CString CString::ToString(unsigned char c) { stringstream s; s << c; return s.str(); }
CString CString::ToString(short i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(unsigned short i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(int i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(unsigned int i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(long i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(unsigned long i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(long long i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(unsigned long long i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(double i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(float i) { stringstream s; s << i; return s.str(); }

CString CString::ToPercent(double d) {
	char szRet[32];
	snprintf(szRet, 32, "%.02f%%", d);
	return szRet;
}

CString CString::ToKBytes(double d) {
	char szRet[32];
	snprintf(szRet, 32, "%.0f K/s", d);
	return szRet;
}

unsigned int CString::ToUInt() const { return strtoul(this->c_str(), (char**) NULL, 10); }
int CString::ToInt() const { return strtoul(this->c_str(), (char**) NULL, 10); }
unsigned long long CString::ToULongLong() const { return strtoull( c_str(), NULL, 10); }
long long CString::ToLongLong() const { return strtoll(c_str(), NULL, 10); }
double CString::ToDouble() const { return strtod(c_str(), NULL); }


bool CString::Trim(const CString& s) {
	bool bLeft = LeftTrim(s);
	return (RightTrim(s) || bLeft);
}

bool CString::LeftTrim(const CString& s) {
	bool bRet = false;

	while (length() && s.find(Left(1)) != CString::npos) {
		LeftChomp();
		bRet = true;
	}

	return bRet;
}

bool CString::RightTrim(const CString& s) {
	bool bRet = false;

	while (length() && s.find(Right(1)) != CString::npos) {
		RightChomp();
		bRet = true;
	}

	return bRet;
}

bool CString::LeftChomp(unsigned int uLen) {
	bool bRet = false;

	while ((uLen--) && (length())) {
		erase(0, 1);
		bRet = true;
	}

	return bRet;
}

bool CString::RightChomp(unsigned int uLen) {
	bool bRet = false;

	while ((uLen--) && (length())) {
		erase(length() -1);
		bRet = true;
	}

	return bRet;
}

//////////////// MCString ////////////////
int MCString::WriteToDisk( const CString & sPath, mode_t iMode )
{
	CFile cFile( sPath );
	if ( !cFile.Open( O_WRONLY|O_CREAT|O_TRUNC, iMode ) )
		return( MCS_EOPEN );

	for( MCString::iterator it = this->begin(); it != this->end(); it++ )
	{
		CString sKey = it->first;
		CString sValue = it->second;
		if ( !WriteFilter( sKey, sValue ) )
			return( MCS_EWRITEFIL );

		if ( sKey.empty() )
			continue;

		if ( cFile.Write( Encode( sKey ) + " " +  Encode( sValue ) + "\n" ) <= 0 )
			return( MCS_EWRITE );
	}

	cFile.Close();

	return( MCS_SUCCESS );
}

int MCString::ReadFromDisk( const CString & sPath, mode_t iMode )
{
	clear();
	CFile cFile( sPath );
	if ( !cFile.Open( O_RDONLY|O_CREAT, iMode ) )
		return( MCS_EOPEN );

	CString sBuffer;

	while( cFile.ReadLine( sBuffer ) )
	{
		sBuffer.Trim();
		CString sKey = sBuffer.Token( 0 );
		CString sValue = sBuffer.Token( 1 );
		Decode( sKey );
		Decode( sValue );
		
		if ( !ReadFilter( sKey, sValue ) )
			return( MCS_EREADFIL );

		(*this)[sKey] = sValue;
	}
	cFile.Close();
	
	return( MCS_SUCCESS );
}


static const char hexdigits[] = "0123456789abcdef";

CString & MCString::Encode( CString & sValue )
{
	CString sTmp;
	for( CString::iterator it = sValue.begin(); it != sValue.end(); it++ )
	{
		if ( isalnum( *it ) )	
			sTmp += *it;
		else
		{
			sTmp += "%";
			sTmp += hexdigits[*it >> 4];
			sTmp += hexdigits[*it & 0xf];
			sTmp += ";";
		}
	}
	sValue = sTmp;
	return( sValue );
}

CString & MCString::Decode( CString & sValue )
{
	const char *pTmp = sValue.c_str();
	char *endptr;
	CString sTmp;
	while( *pTmp )
	{
		if ( *pTmp != '%' )
			sTmp += *pTmp++;
		else
		{
			char ch = (char )strtol( ((const char *)pTmp + 1), &endptr, 16 );
			if ( *endptr == ';' )
			{
				sTmp += ch;
				pTmp = ++endptr;
			}
			else
			{
				sTmp += *pTmp++;
			}
		}
	}

	sValue = sTmp;
	return( sValue );
}














