#include <string.h>
#include "String.h"

int CString::CaseCmp(const CString& s) const {
	return strcasecmp(c_str(), s.c_str());
}

int CString::StrCmp(const CString& s) const {
	return strcmp(c_str(), s.c_str());
}

bool CString::WildCmp(const CString& sWild, const CString& sString) {
	return true; // @todo
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

CString CString::ToString(short i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(unsigned short i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(int i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(unsigned int i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(long i) { stringstream s; s << i; return s.str(); }
CString CString::ToString(unsigned long i) { stringstream s; s << i; return s.str(); }
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

bool CString::Trim(const CString& s) {
	bool bLeft = LeftTrim(s);
	return (RightTrim(s) || bLeft);
}

bool CString::LeftTrim(const CString& s) {
	bool bRet = false;

	while (s.find(Left(1)) != CString::npos) {
		LeftChomp();
		bRet = true;
	}

	return bRet;
}

bool CString::RightTrim(const CString& s) {
	bool bRet = false;

	while (s.find(Right(1)) != CString::npos) {
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
