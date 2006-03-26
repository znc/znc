#include <string.h>
#include "String.h"
#include "FileUtils.h"
#include "MD5.h"

const char* g_szHTMLescapes[256] = {
	"&#0;", 0, 0, 0, 0, 0, 0, 0, 0, 0,               // 0-9
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 10-19
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 20-29
	0, 0, 0, 0, "&quot;", 0, 0, 0, "&amp;", "&#39;", // 30-39
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 40-49
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 50-59
	"&lt;", 0, "&gt;", 0, 0, 0, 0, 0, 0, 0,          // 60-69
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 70-79
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 80-89
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 90-99
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 100-109
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 110-119
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 120-129
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 130-139
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                    // 140-149
	0, 0, 0, "&trade;", 0, 0, 0, 0, 0, 0,            // 150-159
	"&nbsp;",   // 160
	"&iexcl;",  // 161
	"&cent;",   // 162
	"&pound;",  // 163
	"&curren;", // 164
	"&yen;",    // 165
	"&brvbar;", // 166
	"&sect;",   // 167
	"&uml;",    // 168
	"&copy;",   // 169
	"&ordf;",   // 170
	"&laquo;",  // 171
	"&not;",    // 172
	"&shy;",    // 173
	"&reg;",    // 174
	"&macr;",   // 175
	"&deg;",    // 176
	"&plusmn;", // 177
	"&sup2;",   // 178
	"&sup3;",   // 179
	"&acute;",  // 180
	"&micro;",  // 181
	"&para;",   // 182
	"&middot;", // 183
	"&cedil;",  // 184
	"&sup1;",   // 185
	"&ordm;",   // 186
	"&raquo;",  // 187
	"&frac14;", // 188
	"&frac12;", // 189
	"&frac34;", // 190
	"&iquest;", // 191
	"&Agrave;", // 192
	"&Aacute;", // 193
	"&Acirc;",  // 194
	"&Atilde;", // 195
	"&Auml;",   // 196
	"&Aring;",  // 197
	"&AElig;",  // 198
	"&Ccedil;", // 199
	"&Egrave;", // 200
	"&Eacute;", // 201
	"&Ecirc;",  // 202
	"&Euml;",   // 203
	"&Igrave;", // 204
	"&Iacute;", // 205
	"&Icirc;",  // 206
	"&Iuml;",   // 207
	"&ETH;",    // 208
	"&Ntilde;", // 209
	"&Ograve;", // 210
	"&Oacute;", // 211
	"&Ocirc;",  // 212
	"&Otilde;", // 213
	"&Ouml;",   // 214
	"&times;",  // 215
	"&Oslash;", // 216
	"&Ugrave;", // 217
	"&Uacute;", // 218
	"&Ucirc;",  // 219
	"&Uuml;",   // 220
	"&Yacute;", // 221
	"&THORN;",  // 222
	"&szlig;",  // 223
	"&agrave;", // 224
	"&aacute;", // 225
	"&acirc;",  // 226
	"&atilde;", // 227
	"&auml;",   // 228
	"&aring;",  // 229
	"&aelig;",  // 230
	"&ccedil;", // 231
	"&egrave;", // 232
	"&eacute;", // 233
	"&ecirc;",  // 234
	"&euml;",   // 235
	"&igrave;", // 236
	"&iacute;", // 237
	"&icirc;",  // 238
	"&iuml;",   // 239
	"&eth;",    // 240
	"&ntilde;", // 241
	"&ograve;", // 242
	"&oacute;", // 243
	"&ocirc;",  // 244
	"&otilde;", // 245
	"&ouml;",   // 246
	"&divide;", // 247
	"&oslash;", // 248
	"&ugrave;", // 249
	"&uacute;", // 250
	"&ucirc;",  // 251
	"&uuml;",   // 252
	"&yacute;", // 253
	"&thorn;",  // 254
	"&yuml;",   // 255
};

CString::CString(char c) : string() { stringstream s; s << c; *this = s.str(); }
CString::CString(unsigned char c) : string() { stringstream s; s << c; *this = s.str(); }
CString::CString(short i) : string() { stringstream s; s << i; *this = s.str(); }
CString::CString(unsigned short i) : string() { stringstream s; s << i; *this = s.str(); }
CString::CString(int i) : string() { stringstream s; s << i; *this = s.str(); }
CString::CString(unsigned int i) : string() { stringstream s; s << i; *this = s.str(); }
CString::CString(long i) : string() { stringstream s; s << i; *this = s.str(); }
CString::CString(unsigned long i) : string() { stringstream s; s << i; *this = s.str(); }
CString::CString(long long i) : string() { stringstream s; s << i; *this = s.str(); }
CString::CString(unsigned long long i) : string() { stringstream s; s << i; *this = s.str(); }
CString::CString(double i) : string() { stringstream s; s << i; *this = s.str(); }
CString::CString(float i) : string() { stringstream s; s << i; *this = s.str(); }

inline unsigned char* CString::strnchr(const unsigned char* src, unsigned char c, unsigned int iMaxBytes, unsigned char* pFill, unsigned int* piCount) const {
    for (unsigned int a = 0; a < iMaxBytes && *src; a++, src++) {
		if (pFill) {
			pFill[a] = *src;
		}

		if (*src == c) {
			if (pFill) {
				pFill[a +1] = 0;
			}

			if (piCount) {
				*piCount = a;
			}

			return (unsigned char*) src;
		}
	}

	if (pFill) {
		*pFill = 0;
	}

	if (piCount) {
		*piCount = 0;
	}

    return NULL;
}

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

CString::EEscape CString::ToEscape(const CString& sEsc) {
	if (sEsc.CaseCmp("ASCII") == 0) {
		return EASCII;
	} else if (sEsc.CaseCmp("HTML") == 0) {
		return EHTML;
	} else if (sEsc.CaseCmp("URL") == 0) {
		return EURL;
	} else if (sEsc.CaseCmp("SQL") == 0) {
		return ESQL;
	}

	return EASCII;
}

CString CString::Escape_n(EEscape eFrom, EEscape eTo) const {
	CString sRet;
	const char szHex[] = "0123456789ABCDEF";
	const unsigned char *pStart = (const unsigned char*) data();
	const unsigned char *p = (const unsigned char*) data();
	unsigned int iLength = length();
	sRet.reserve(iLength *3);
	unsigned int iMaxLen = (eFrom == EHTML) ? 20 : 0;
	unsigned char pTmp[iMaxLen +1];
	unsigned int iCounted = 0;

	for (unsigned int a = 0; a < iLength; a++, p = pStart + a) {
		unsigned char ch = 0;

		switch (eFrom) {
			case EHTML:
				if ((*p == '&') && (strnchr((unsigned char*) p, ';', iMaxLen, pTmp, &iCounted))) {
					if ((iCounted >= 3) && (pTmp[1] == '#')) {	// do XML and HTML &#97; &#x3c
						int base = 10;

						if ((pTmp[2] & 0xDF) == 'X') {
							base = 16;
						}

						char* endptr = NULL;
						unsigned int b = strtol((const char*) (pTmp +2 + (base == 16)), &endptr, base);

						if ((*endptr == ';') && (b <= 255)) { // incase they do something like &#7777777777;
							ch = b;
							a += iCounted;
							break;
						}
					}

					for (unsigned int c = 0; c < 256; c++) {
						if (g_szHTMLescapes[c] && strcmp(g_szHTMLescapes[c], (const char*) &pTmp) == 0) {
							ch = c;
							break;
						}
					}

					if (ch > 0) {
						a += iCounted;
					} else {
						ch = *p;	 // Not a valid escape, just record the &
					}
				} else {
					ch = *p;
				}
				break;
			case EASCII:
				ch = *p;
				break;
			case EURL:
				if (*p == '%' && (a +2) < iLength && isxdigit(*(p +1)) && isxdigit(*(p +2))) {
					p++;
					if (isdigit(*p)) {
						ch = (*p - '0') << 4;
					} else {
						ch = (tolower(*p) - 'a' +10) << 4;
					}

					p++;
					if (isdigit(*p)) {
						ch |= (*p - '0');
					} else {
						ch |= (tolower(*p) - 'a' +10);
					}

					a += 2;
				} else if (pStart[a] == '+') {
					ch = ' ';
				} else {
					ch = *p;
				}

				break;
			case ESQL:
				if (*p != '\\' || iLength < (a +1)) {
					ch = *p;
				} else {
					a++;
					p++;

					if (*p == 'n') {
						ch = '\n';
					} else if (*p == 'r') {
						ch = '\r';
					} else if (*p == '0') {
						ch = '\0';
					} else if (*p == 't') {
						ch = '\t';
					} else if (*p == 'b') {
						ch = '\b';
					} else {
						ch = *p;
					}
				}

				break;
		}

		switch (eTo) {
			case EHTML:
				if (g_szHTMLescapes[ch]) {
					sRet += g_szHTMLescapes[ch];
				} else {
					sRet += ch;
				}

				break;
			case EASCII:
				sRet += ch;
				break;
			case EURL:
				if (isalnum(ch) || ch == '_' || ch == '.' || ch == '-') {
					sRet += ch;
				} else if (ch == ' ') {
					sRet += '+';
				} else {
					sRet += '%';
					sRet += szHex[ch >> 4];
					sRet += szHex[ch & 0xf];
				}

				break;
			case ESQL:
				if (ch == '\0') { sRet += '\\'; sRet += '0';
				} else if (ch == '\n') { sRet += '\\'; sRet += 'n';
				} else if (ch == '\t') { sRet += '\\'; sRet += 't';
				} else if (ch == '\r') { sRet += '\\'; sRet += 'r';
				} else if (ch == '\b') { sRet += '\\'; sRet += 'b';
				} else if (ch == '\"') { sRet += '\\'; sRet += '\"';
				} else if (ch == '\'') { sRet += '\\'; sRet += '\'';
				} else if (ch == '\\') { sRet += '\\'; sRet += '\\';
				} else { sRet += ch; }

				break;
		}
	}

	sRet.reserve(0);
	return sRet;
}

CString CString::Escape_n(EEscape eTo) const {
	return Escape_n(EASCII, eTo);
}

CString& CString::Escape(EEscape eFrom, EEscape eTo) {
	return (*this = Escape_n(eFrom, eTo));
}

CString& CString::Escape(EEscape eTo) {
	return (*this = Escape_n(eTo));
}

CString CString::Replace_n(const CString& sReplace, const CString& sWith, const CString& sLeft, const CString& sRight, bool bRemoveDelims) const {
	CString sRet = *this;
	CString::Replace(sRet, sReplace, sWith, sLeft, sRight, bRemoveDelims);
	return sRet;
}

unsigned int CString::Replace(const CString& sReplace, const CString& sWith, const CString& sLeft, const CString& sRight, bool bRemoveDelims) {
	return CString::Replace(*this, sReplace, sWith, sLeft, sRight, bRemoveDelims);
}

unsigned int CString::Replace(CString& sStr, const CString& sReplace, const CString& sWith, const CString& sLeft, const CString& sRight, bool bRemoveDelims) {
	unsigned int uRet = 0;
	CString sCopy = sStr;
	sStr.clear();

	unsigned int uReplaceWidth = sReplace.length();
	unsigned int uLeftWidth = sLeft.length();
	unsigned int uRightWidth = sRight.length();
	const char* p = sCopy.c_str();
	bool bInside = false;

	while (*p) {
		if (!bInside && uLeftWidth && strncmp(p, sLeft.c_str(), uLeftWidth) == 0) {
			if (!bRemoveDelims) {
				sStr += sLeft;
			}

			p += uLeftWidth -1;
			bInside = true;
		} else if (bInside && uRightWidth && strncmp(p, sRight.c_str(), uRightWidth) == 0) {
			if (!bRemoveDelims) {
				sStr += sRight;
			}

			p += uRightWidth -1;
			bInside = false;
		} else if (!bInside && strncmp(p, sReplace.c_str(), uReplaceWidth) == 0) {
			sStr += sWith;
			p += uReplaceWidth -1;
			uRet++;
		} else {
			sStr.append(p, 1);
		}

		p++;
	}

	return uRet;
}

CString CString::Token(unsigned int uPos, bool bRest, const CString& sSep) const {
	string sRet;
	const char* p = c_str();
	unsigned int uSepLen = sSep.length();

	if (uSepLen) {
		uSepLen--;
	}

	while (*p) {
		if (uPos) {
			if (strncmp(p, sSep.c_str(), sSep.length()) == 0) {
				uPos--;
				p += uSepLen;
			}
		} else {
			if (strncmp(p, sSep.c_str(), sSep.length()) == 0) {
				if (!bRest) {
					return sRet;
				}
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

unsigned int CString::URLSplit(MCString& msRet) const {
	msRet.clear();

	VCString vsPairs;
	Split("&", vsPairs);

	for (size_t a = 0; a < vsPairs.size(); a++) {
		const CString& sPair = vsPairs[a];

		msRet[sPair.Token(0, false, "=").Escape(CString::EURL, CString::EASCII)] = sPair.Token(1, true, "=").Escape(CString::EURL, CString::EASCII);
	}

	return msRet.size();
}

unsigned int CString::Split(const CString& sDelim, VCString& vsRet, bool bAllowEmpty, const CString& sLeft, const CString& sRight) const {
	vsRet.clear();

	if (empty()) {
		return 0;
	}

	CString sTmp;
	bool bInside = false;
	unsigned int uDelimLen = sDelim.length();
	unsigned int uLeftLen = sLeft.length();
	unsigned int uRightLen = sRight.length();
	const char* p = c_str();

	while (*p) {
		if (uLeftLen && uRightLen && !bInside && strncasecmp(p, sLeft.c_str(), uLeftLen) == 0) {
			p += uLeftLen;
			bInside = true;
			continue;
		}

		if (uLeftLen && uRightLen && bInside && strncasecmp(p, sRight.c_str(), uRightLen) == 0) {
			p += uRightLen;
			bInside = false;
			continue;
		}

		if (uDelimLen && !bInside && strncasecmp(p, sDelim.c_str(), uDelimLen) == 0) {
			vsRet.push_back(sTmp);
			sTmp.clear();
			p += uDelimLen;

			if (!bAllowEmpty) {
				while (strncasecmp(p, sDelim.c_str(), uDelimLen) == 0) {
					p += uDelimLen;
				}
			}

			bInside = false;
			continue;
		} else {
			sTmp += *p;
		}

		p++;
	}

	if (!sTmp.empty()) {
		vsRet.push_back(sTmp);
	}

	return vsRet.size();

	/*vsRet.clear();
	CString sTmp = *this;

	while (sTmp.size()) {
		CString sTok = sTmp.Token(0, false, sDelim);
		CString sRest = sTmp.Token(1, true, sDelim);

		if (bAllowEmpty || !sTok.empty()) {
			vsRet.push_back(sTok);
		}

		if (bAllowEmpty && sRest.empty() && sTok.size() < sTmp.size()) {
			vsRet.push_back("");
		}

		sTmp = sRest;
	}

	return vsRet.size();*/
}

unsigned int CString::Split(const CString& sDelim, SCString& ssRet, bool bAllowEmpty, const CString& sLeft, const CString& sRight) const {
	VCString vsTokens;

	Split(sDelim, vsTokens, bAllowEmpty, sLeft, sRight);

	ssRet.clear();

	for (size_t a = 0; a < vsTokens.size(); a++) {
		ssRet.insert(vsTokens[a]);
	}

	return ssRet.size();
}

CString CString::Format(const CString& sFormatStr, ...) {
	return "";
}

CString CString::RandomString(unsigned int uLength) {
	CString sRet;

	for (unsigned int a = 0; a < uLength; a++) {
		sRet += (char) (rand() % 26) + 65;
	}

	return sRet;
}

bool CString::Base64Encode(unsigned int uWrap) {
	CString sCopy(*this);
	return sCopy.Base64Encode(*this, uWrap);
}

unsigned long CString::Base64Decode() {
	CString sCopy(*this);
	return sCopy.Base64Decode(*this);
}

CString CString::Base64Encode_n(unsigned int uWrap) const {
	CString sRet;
	Base64Encode(sRet, uWrap);
	return sRet;
}

CString CString::Base64Decode_n() const {
	CString sRet;
	Base64Decode(sRet);
	return sRet;
}

bool CString::Base64Encode(CString& sRet, unsigned int uWrap) const {
	static char b64table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	sRet.clear();
	size_t len = size();
	const unsigned char* input = (const unsigned char*) c_str();
	unsigned char *output, *p;
	size_t        i = 0, mod = len % 3, toalloc;
	toalloc = (len / 3) * 4 + (3 - mod) % 3 + 1 + 8;

	if (uWrap) {
		toalloc += len / 57;
		if (len % 57) {
			toalloc++;
		}
	}

	if (toalloc < len) {
		return 0;
	}

	//p = output = (unsigned char *)malloc(toalloc);
	p = output = new unsigned char [toalloc];

	if (!p) {
		return false;
	}

	while (i < len - mod) {
		*p++ = b64table[input[i++] >> 2];
		*p++ = b64table[((input[i - 1] << 4) | (input[i] >> 4)) & 0x3f];
		*p++ = b64table[((input[i] << 2) | (input[i + 1] >> 6)) & 0x3f];
		*p++ = b64table[input[i + 1] & 0x3f];
		i += 2;

		if (uWrap && !(i % 57)) {
			*p++ = '\n';
		}
	}

	if (!mod) {
		if (uWrap && i % 57) {
			*p++ = '\n';
		}

		*p = 0;

		sRet = (char*) output;
		delete[] output;
		return true;
	} else {
		*p++ = b64table[input[i++] >> 2];
		*p++ = b64table[((input[i - 1] << 4) | (input[i] >> 4)) & 0x3f];
		if (mod == 1) {
			*p++ = '=';
			*p++ = '=';

			if (uWrap) {
				*p++ = '\n';
			}

			*p = 0;

			sRet = (char*) output;
			delete[] output;
			return true;
		} else {
			*p++ = b64table[(input[i] << 2) & 0x3f];
			*p++ = '=';

			if (uWrap) {
				*p++ = '\n';
			}

			*p = 0;
			sRet = (char*) output;
			delete[] output;
			return true;
		}
	}

	sRet = (char*) output;
	delete[] output;
	return true;
}

unsigned long CString::Base64Decode(CString& sRet) const {
	const char* in = c_str();
	char c, c1, *p;
	unsigned long i;
	unsigned long uLen = size();
	char* out = (char*) malloc(size() +1);

	for (i = 0, p = out; i < uLen; i++) {
		c = (char)base64_table[(unsigned char)in[i++]];
		c1 = (char)base64_table[(unsigned char)in[i++]];
		*p++ = (c << 2) | ((c1 >> 4) & 0x3);

		if (i < uLen) {
			if (in[i] == '=') {
				break;
			}
			c = (char)base64_table[(unsigned char)in[i]];
			*p++ = ((c1 << 4) & 0xf0) | ((c >> 2) & 0xf);
		}

		if (++i < uLen) {
			if (in[i] == '=') {
				break;
			}
			*p++ = ((c << 6) & 0xc0) | (char)base64_table[(unsigned char)in[i]];
		}
	}

	*p = '\0';
	unsigned long uRet = p - out;
	sRet.clear();
	sRet.append(out, uRet);
	free(out);

	return uRet;
}

CString CString::MD5() const {
	return (const char*) CMD5(*this);
}

#ifdef HAVE_LIBSSL
CString CString::Encrypt_n(const CString& sPass, const CString& sIvec) {
	CString sRet;
	sRet.Encrypt(sPass, sIvec);
	return sRet;
}

CString CString::Decrypt_n(const CString& sPass, const CString& sIvec) {
	CString sRet;
	sRet.Decrypt(sPass, sIvec);
	return sRet;
}

void CString::Encrypt(const CString& sPass, const CString& sIvec) {
	Crypt(sPass, true, sIvec);
}

void CString::Decrypt(const CString& sPass, const CString& sIvec) {
	Crypt(sPass, false, sIvec);
}

void CString::Crypt(const CString& sPass, bool bEncrypt, const CString& sIvec) {
	unsigned char szIvec[8] = {0,0,0,0,0,0,0,0};
	BF_KEY bKey;

	if (sIvec.length() >= 8) {
		memcpy(szIvec, sIvec.data(), 8);
	}

	BF_set_key(&bKey, sPass.length(), (unsigned char*) sPass.data());
	unsigned int uPad = (length() % 8);

	if (uPad) {
		uPad = 8 - uPad;
		append(uPad, '\0');
	}

	size_t uLen = length();
	unsigned char* szBuff = (unsigned char*) malloc(uLen);
	BF_cbc_encrypt((const unsigned char*) data(), szBuff, uLen, &bKey, szIvec, ((bEncrypt) ? BF_ENCRYPT : BF_DECRYPT));

	clear();
	append((const char*) szBuff, uLen);
	free(szBuff);
}
#endif	// HAVE_LIBSSL

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

bool CString::ToBool() const { return (!Trim_n().Trim_n("0").empty() && Trim_n().CaseCmp("false") != 0); }
short CString::ToShort() const { return strtoul(this->c_str(), (char**) NULL, 10); }
unsigned short CString::ToUShort() const { return strtoul(this->c_str(), (char**) NULL, 10); }
unsigned int CString::ToUInt() const { return strtoul(this->c_str(), (char**) NULL, 10); }
int CString::ToInt() const { return strtoul(this->c_str(), (char**) NULL, 10); }
unsigned long CString::ToULong() const { return strtoul(c_str(), NULL, 10); }
unsigned long long CString::ToULongLong() const { return strtoull(c_str(), NULL, 10); }
long long CString::ToLongLong() const { return strtoll(c_str(), NULL, 10); }
double CString::ToDouble() const { return strtod(c_str(), NULL); }


bool CString::Trim(const CString& s) {
	bool bLeft = TrimLeft(s);
	return (TrimRight(s) || bLeft);
}

bool CString::TrimLeft(const CString& s) {
	bool bRet = false;

	while (length() && s.find(Left(1)) != CString::npos) {
		LeftChomp();
		bRet = true;
	}

	return bRet;
}

bool CString::TrimRight(const CString& s) {
	bool bRet = false;

	while (length() && s.find(Right(1)) != CString::npos) {
		RightChomp();
		bRet = true;
	}

	return bRet;
}

CString CString::Trim_n(const CString& s) const {
	CString sRet = *this;
	sRet.Trim(s);
	return sRet;
}

CString CString::TrimLeft_n(const CString& s) const {
	CString sRet = *this;
	sRet.TrimLeft(s);
	return sRet;
}

CString CString::TrimRight_n(const CString& s) const {
	CString sRet = *this;
	sRet.TrimRight(s);
	return sRet;
}

CString CString::LeftChomp_n(unsigned int uLen) const {
	CString sRet = *this;
	sRet.LeftChomp(uLen);
	return sRet;
}

CString CString::RightChomp_n(unsigned int uLen) const {
	CString sRet = *this;
	sRet.RightChomp(uLen);
	return sRet;
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
int MCString::WriteToDisk(const CString& sPath, mode_t iMode) {
	CFile cFile(sPath);
	if (!cFile.Open(O_WRONLY|O_CREAT|O_TRUNC, iMode)) {
		return MCS_EOPEN;
	}

	for (MCString::iterator it = this->begin(); it != this->end(); it++) {
		CString sKey = it->first;
		CString sValue = it->second;
		if (!WriteFilter(sKey, sValue)) {
			return MCS_EWRITEFIL;
		}

		if (sKey.empty()) {
			continue;
		}

		if (cFile.Write(Encode(sKey) + " " +  Encode(sValue) + "\n") <= 0) {
			return MCS_EWRITE;
		}
	}

	cFile.Close();

	return MCS_SUCCESS;
}

int MCString::ReadFromDisk(const CString& sPath, mode_t iMode) {
	clear();
	CFile cFile(sPath);
	if (!cFile.Open(O_RDONLY|O_CREAT, iMode)) {
		return MCS_EOPEN;
	}

	CString sBuffer;

	while (cFile.ReadLine(sBuffer)) {
		sBuffer.Trim();
		CString sKey = sBuffer.Token(0);
		CString sValue = sBuffer.Token(1);
		Decode(sKey);
		Decode(sValue);

		if (!ReadFilter(sKey, sValue))
			return MCS_EREADFIL;

		(*this)[sKey] = sValue;
	}
	cFile.Close();

	return MCS_SUCCESS;
}


static const char hexdigits[] = "0123456789abcdef";

CString& MCString::Encode(CString& sValue) {
	CString sTmp;
	for (CString::iterator it = sValue.begin(); it != sValue.end(); it++) {
		if (isalnum(*it)) {
			sTmp += *it;
		} else {
			sTmp += "%";
			sTmp += hexdigits[*it >> 4];
			sTmp += hexdigits[*it & 0xf];
			sTmp += ";";
		}
	}
	sValue = sTmp;
	return sValue;
}

CString& MCString::Decode(CString& sValue) {
	const char *pTmp = sValue.c_str();
	char *endptr;
	CString sTmp;

	while(*pTmp) {
		if (*pTmp != '%') {
			sTmp += *pTmp++;
		} else {
			char ch = (char )strtol(((const char *)pTmp + 1), &endptr, 16);
			if (*endptr == ';') {
				sTmp += ch;
				pTmp = ++endptr;
			} else {
				sTmp += *pTmp++;
			}
		}
	}

	sValue = sTmp;
	return sValue;
}














