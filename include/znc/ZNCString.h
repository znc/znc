/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ZNCSTRING_H
#define ZNCSTRING_H

#include <znc/zncconfig.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <sstream>
#include <sys/types.h>

#define _SQL(s) CString("'" + CString(s).Escape_n(CString::ESQL) + "'")
#define _URL(s) CString(s).Escape_n(CString::EURL)
#define _HTML(s) CString(s).Escape_n(CString::EHTML)
#define _NAMEDFMT(s) CString(s).Escape_n(CString::ENAMEDFMT)

class CString;
class MCString;

typedef std::set<CString> SCString;
typedef std::vector<CString>                 VCString;
typedef std::vector<std::pair<CString, CString> > VPair;

static const unsigned char XX = 0xff;
static const unsigned char base64_table[256] = {
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
	52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,XX,XX,XX,
	XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
	15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
	XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
	41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
	XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};

enum class CaseSensitivity {
	CaseInsensitive,
	CaseSensitive
};

/**
 * @brief String class that is used inside ZNC.
 *
 * All strings that are used in ZNC and its modules should use instances of this
 * class. It provides helpful functions for parsing input like Token() and
 * Split().
 */
class CString : public std::string {
public:
	typedef enum {
		EASCII,
		EURL,
		EHTML,
		ESQL,
		ENAMEDFMT,
		EDEBUG,
		EMSGTAG,
		EHEXCOLON,
	} EEscape;

	static const CaseSensitivity CaseSensitive = CaseSensitivity::CaseSensitive;
	static const CaseSensitivity CaseInsensitive = CaseSensitivity::CaseInsensitive;

	explicit CString(bool b) : std::string(b ? "true" : "false") {}
	explicit CString(char c);
	explicit CString(unsigned char c);
	explicit CString(short i);
	explicit CString(unsigned short i);
	explicit CString(int i);
	explicit CString(unsigned int i);
	explicit CString(long i);
	explicit CString(unsigned long i);
	explicit CString(long long i);
	explicit CString(unsigned long long i);
	explicit CString(double i, int precision = 2);
	explicit CString(float i, int precision = 2);

	CString() : std::string() {}
	CString(const char* c) : std::string(c) {}
	CString(const char* c, size_t l) : std::string(c, l) {}
	CString(const std::string& s) : std::string(s) {}
	CString(size_t n, char c) : std::string(n, c) {}
	~CString() {}
	
	/**
	 * Casts a CString to another type.  Implemented via std::stringstream, you use this
	 * for any class that has an operator<<(std::ostream, YourClass).
	 * @param target The object to cast into. If the cast fails, its state is unspecified.
	 * @return True if the cast succeeds, and false if it fails.
	 */
	template <typename T> bool Convert(T *target) const
	{
		std::stringstream ss(*this);
		ss >> *target;
		return (bool) ss; // we don't care why it failed, only whether it failed
	}
	
	/**
	 * Joins a collection of objects together, using 'this' as a delimiter.
	 * You can pass either pointers to arrays, or iterators to collections.
	 * @param i_begin An iterator pointing to the beginning of a group of objects.
	 * @param i_end An iterator pointing past the end of a group of objects.
	 * @return The joined string
	 */
	template <typename Iterator> CString Join(Iterator i_start, const Iterator &i_end) const
	{
		if (i_start == i_end) return CString("");
		std::ostringstream output;
		output << *i_start;
		while (true)
		{
			++i_start;
			if (i_start == i_end) return CString(output.str());
			output << *this;
			output << *i_start;
		}
	}
	
	/**
	 * Compare this string caselessly to some other string.
	 * @param s The string to compare to.
	 * @param uLen The number of characters to compare.
	 * @return An integer less than, equal to, or greater than zero if this
	 *         string smaller, equal.... to the given string.
	 */
	int CaseCmp(const CString& s, CString::size_type uLen = CString::npos) const;
	/**
	 * Compare this string case sensitively to some other string.
	 * @param s The string to compare to.
	 * @param uLen The number of characters to compare.
	 * @return An integer less than, equal to, or greater than zero if this
	 *         string smaller, equal.... to the given string.
	 */
	int StrCmp(const CString& s, CString::size_type uLen = CString::npos) const;
	/**
	 * Check if this string is equal to some other string.
	 * @param s The string to compare to.
	 * @param cs CaseSensitive if you want the comparison to be case
	 *                       sensitive, CaseInsensitive (default) otherwise.
	 * @return True if the strings are equal.
	 */
	bool Equals(const CString& s, CaseSensitivity cs = CaseInsensitive) const;
	/**
	 * @deprecated
	 */
	bool Equals(const CString& s, bool bCaseSensitive, CString::size_type uLen = CString::npos) const;
	/**
	 * Do a wildcard comparison between two strings.
	 * For example, the following returns true:
	 * <code>WildCmp("*!?bar@foo", "I_am!~bar@foo");</code>
	 * @param sWild The wildcards used for the comparison.
	 * @param sString The string that is used for comparing.
	 * @param cs CaseSensitive (default) if you want the comparison
	 *           to be case sensitive, CaseInsensitive otherwise.
	 * @todo Make cs CaseInsensitive by default.
	 * @return true if the wildcard matches.
	 */
	static bool WildCmp(const CString& sWild, const CString& sString, CaseSensitivity cs = CaseSensitive);
	/**
	 * Do a wild compare on this string.
	 * @param sWild The wildcards used to for the comparison.
	 * @param cs CaseSensitive (default) if you want the comparison
	 *           to be case sensitive, CaseInsensitive otherwise.
	 * @todo Make cs CaseInsensitive by default.
	 * @return The result of <code>this->WildCmp(sWild, *this);</code>.
	 */
	bool WildCmp(const CString& sWild, CaseSensitivity cs = CaseSensitive) const;

	/**
	 * Turn all characters in this string into their upper-case equivalent.
	 * @returns A reference to *this.
	 */
	CString& MakeUpper();
	/**
	 * Turn all characters in this string into their lower-case equivalent.
	 * @returns A reference to *this.
	 */
	CString& MakeLower();
	/**
	 * Return a copy of this string with all characters turned into
	 * upper-case.
	 * @return The new string.
	 */
	CString AsUpper() const;
	/**
	 * Return a copy of this string with all characters turned into
	 * lower-case.
	 * @return The new string.
	 */
	CString AsLower() const;

	static EEscape ToEscape(const CString& sEsc);
	CString Escape_n(EEscape eFrom, EEscape eTo) const;
	CString Escape_n(EEscape eTo) const;
	CString& Escape(EEscape eFrom, EEscape eTo);
	CString& Escape(EEscape eTo);

	/** Replace all occurrences in a string.
	 *
	 * You can specify a "safe zone" via sLeft and sRight. Anything inside
	 * of such a zone will not be replaced. This does not do recursion, so
	 * e.g. with <code>Replace("(a()a)", "a", "b", "(", ")", true)</code>
	 * you would get "a(b)" as result. The second opening brace and the
	 * second closing brace would not be seen as a delimitered and thus
	 * wouldn't be removed. The first a is inside a "safe zone" and thus is
	 * left alone, too.
	 *
	 * @param sStr The string to do the replacing on. This will also contain
	 *             the result when this function returns.
	 * @param sReplace The string that should be replaced.
	 * @param sWith The replacement to use.
	 * @param sLeft The string that marks the begin of the "safe zone".
	 * @param sRight The string that marks the end of the "safe zone".
	 * @param bRemoveDelims If this is true, all matches for sLeft and
	 *                      sRight are removed.
	 * @returns The number of replacements done.
	 */
	static unsigned int Replace(CString& sStr, const CString& sReplace, const CString& sWith, const CString& sLeft = "", const CString& sRight = "", bool bRemoveDelims = false);

	/** Replace all occurrences in the current string.
	 * @see CString::Replace
	 * @param sReplace The string to look for.
	 * @param sWith The replacement to use.
	 * @param sLeft The delimiter at the beginning of a safe zone.
	 * @param sRight The delimiter at the end of a safe zone.
	 * @param bRemoveDelims If true, all matching delimiters are removed.
	 * @return The result of the replacing. The current string is left
	 *         unchanged.
	 */
	CString Replace_n(const CString& sReplace, const CString& sWith, const CString& sLeft = "", const CString& sRight = "", bool bRemoveDelims = false) const;
	/** Replace all occurrences in the current string.
	 * @see CString::Replace
	 * @param sReplace The string to look for.
	 * @param sWith The replacement to use.
	 * @param sLeft The delimiter at the beginning of a safe zone.
	 * @param sRight The delimiter at the end of a safe zone.
	 * @param bRemoveDelims If true, all matching delimiters are removed.
	 * @returns The number of replacements done.
	 */
	unsigned int Replace(const CString& sReplace, const CString& sWith, const CString& sLeft = "", const CString& sRight = "", bool bRemoveDelims = false);
	/** Ellipsize the current string.
	 * For example, ellipsizing "Hello, I'm Bob" to the length 9 would
	 * result in "Hello,...".
	 * @param uLen The length to ellipsize to.
	 * @return The ellipsized string.
	 */
	CString Ellipsize(unsigned int uLen) const;
	/** Return the left part of the string.
	 * @param uCount The number of characters to keep.
	 * @return The resulting string.
	 */
	CString Left(size_type uCount) const;
	/** Return the right part of the string.
	 * @param uCount The number of characters to keep.
	 * @return The resulting string.
	 */
	CString Right(size_type uCount) const;

	/** Get the first line of this string.
	 * @return The first line of text.
	 */
	CString FirstLine() const { return Token(0, false, "\n"); }

	/** Get a token out of this string. For example in the string "a bc d  e",
	 *  each of "a", "bc", "d" and "e" are tokens.
	 * @param uPos The number of the token you are interested. The first
	 *             token has a position of 0.
	 * @param bRest If false, only the token you asked for is returned. Else
	 *              you get the substring starting from the beginning of
	 *              your token.
	 * @param sSep Seperator between tokens.
	 * @param bAllowEmpty If this is true, empty tokens are allowed. In the
	 *                    example from above this means that there is a
	 *                    token "" before the "e" token.
	 * @return The token you asked for and, if bRest is true, everything
	 *         after it.
	 * @see Split() if you need a string split into all of its tokens.
	 */
	CString Token(size_t uPos, bool bRest = false, const CString& sSep = " ", bool bAllowEmpty = false) const;

	/** Get a token out of this string. This function behaves much like the
	 *  other Token() function in this class. The extra arguments are
	 *  handled similarly to Split().
	 */
	CString Token(size_t uPos, bool bRest, const CString& sSep, bool bAllowEmpty, const CString& sLeft, const CString& sRight, bool bTrimQuotes = true) const;

	size_type URLSplit(MCString& msRet) const;
	size_type OptionSplit(MCString& msRet, bool bUpperKeys = false) const;
	size_type QuoteSplit(VCString& vsRet) const;

	/** Split up this string into tokens.
	 * Via sLeft and sRight you can define "markers" like with Replace().
	 * Anything in such a marked section is treated as a single token. All
	 * occurences of sDelim in such a block are ignored.
	 * @param sDelim Delimiter between tokens.
	 * @param vsRet Vector for returning the result.
	 * @param bAllowEmpty Do empty tokens count as a valid token?
	 * @param sLeft Left delimiter like with Replace().
	 * @param sRight Right delimiter like with Replace().
	 * @param bTrimQuotes Should sLeft and sRight be removed from the token
	 *                    they mark?
	 * @param bTrimWhiteSpace If this is true, CString::Trim() is called on
	 *                        each token.
	 * @return The number of tokens found.
	 */
	size_type Split(const CString& sDelim, VCString& vsRet, bool bAllowEmpty = true,
					   const CString& sLeft = "", const CString& sRight = "", bool bTrimQuotes = true,
					   bool bTrimWhiteSpace = false) const;

	/** Split up this string into tokens.
	 * This function is identical to the other CString::Split(), except that
	 * the result is returned as a SCString instead of a VCString.
	 */
	size_type Split(const CString& sDelim, SCString& ssRet, bool bAllowEmpty = true,
					   const CString& sLeft = "", const CString& sRight = "", bool bTrimQuotes = true,
					   bool bTrimWhiteSpace = false) const;

	/** Build a string from a format string, replacing values from a map.
	 * The format specification can contain simple named parameters that match
	 * keys in the given map. For example in the string "a {b} c", the key "b"
	 * is looked up in the map, and inserted for "{b}".
	 * @param sFormat The format specification.
	 * @param msValues A map of named parameters to their values.
	 * @return The string with named parameters replaced.
	 */
	static CString NamedFormat(const CString& sFormat, const MCString& msValues);

	/** Produces a random string.
	 * @param uLength The length of the resulting string.
	 * @return A random string.
	 */
	static CString RandomString(unsigned int uLength);

	/** @return The MD5 hash of this string. */
	CString MD5() const;
	/** @return The SHA256 hash of this string. */
	CString SHA256() const;

	/** Treat this string as base64-encoded data and decode it.
	 * @param sRet String to which the result of the decode is safed.
	 * @return The length of the resulting string.
	 */
	unsigned long Base64Decode(CString& sRet) const;
	/** Treat this string as base64-encoded data and decode it.
	 *  The result is saved in this CString instance.
	 * @return The length of the resulting string.
	 */
	unsigned long Base64Decode();
	/** Treat this string as base64-encoded data and decode it.
	 * @return The decoded string.
	 */
	CString Base64Decode_n() const;
	/** Base64-encode the current string.
	 * @param sRet String where the result is saved.
	 * @param uWrap A boolean(!?!) that decides if the result should be
	 *              wrapped after everywhere 57 characters.
	 * @return true unless this code is buggy.
	 * @todo WTF @ uWrap.
	 * @todo This only returns false if some formula we use was wrong?!
	 */
	bool Base64Encode(CString& sRet, unsigned int uWrap = 0) const;
	/** Base64-encode the current string.
	 *  This string is overwritten with the result of the encode.
	 *  @todo return value and param are as with Base64Encode() from above.
	 */
	bool Base64Encode(unsigned int uWrap = 0);
	/** Base64-encode the current string
	 * @todo uWrap is as broken as Base64Encode()'s uWrap.
	 * @return The encoded string.
	 */
	CString Base64Encode_n(unsigned int uWrap = 0) const;

#ifdef HAVE_LIBSSL
	CString Encrypt_n(const CString& sPass, const CString& sIvec = "") const;
	CString Decrypt_n(const CString& sPass, const CString& sIvec = "") const;
	void Encrypt(const CString& sPass, const CString& sIvec = "");
	void Decrypt(const CString& sPass, const CString& sIvec = "");
	void Crypt(const CString& sPass, bool bEncrypt, const CString& sIvec = "");
#endif

	/** Pretty-print a percent value.
	 * @param d The percent value. This should be in range 0-100.
	 * @return The "pretty" string.
	 */
	static CString ToPercent(double d);
	/** Pretty-print a number of bytes.
	 * @param d The number of bytes.
	 * @return A string describing the number of bytes.
	 */
	static CString ToByteStr(unsigned long long d);
	/** Pretty-print a time span.
	 * @param s Number of seconds to print.
	 * @return A string like "4w 6d 4h 3m 58s".
	 */
	static CString ToTimeStr(unsigned long s);

	/** @return True if this string is not "false". */
	bool ToBool() const;
	/** @return The numerical value of this string similar to atoi(). */
	short ToShort() const;
	/** @return The numerical value of this string similar to atoi(). */
	unsigned short ToUShort() const;
	/** @return The numerical value of this string similar to atoi(). */
	int ToInt() const;
	/** @return The numerical value of this string similar to atoi(). */
	long ToLong() const;
	/** @return The numerical value of this string similar to atoi(). */
	unsigned int ToUInt() const;
	/** @return The numerical value of this string similar to atoi(). */
	unsigned long ToULong() const;
	/** @return The numerical value of this string similar to atoi(). */
	unsigned long long ToULongLong() const;
	/** @return The numerical value of this string similar to atoi(). */
	long long ToLongLong() const;
	/** @return The numerical value of this string similar to atoi(). */
	double ToDouble() const;

	/** Trim this string. All leading/trailing occurences of characters from
	 *  s are removed.
	 * @param s A list of characters that should be trimmed.
	 * @return true if this string was modified.
	 */
	bool Trim(const CString& s = " \t\r\n");
	/** Trim this string. All leading occurences of characters from s are
	 *  removed.
	 * @param s A list of characters that should be trimmed.
	 * @return true if this string was modified.
	 */
	bool TrimLeft(const CString& s = " \t\r\n");
	/** Trim this string. All trailing occurences of characters from s are
	 *  removed.
	 * @param s A list of characters that should be trimmed.
	 * @return true if this string was modified.
	 */
	bool TrimRight(const CString& s = " \t\r\n");
	/** Trim this string. All leading/trailing occurences of characters from
	 *  s are removed. This CString instance is not modified.
	 * @param s A list of characters that should be trimmed.
	 * @return The trimmed string.
	 */
	CString Trim_n(const CString& s = " \t\r\n") const;
	/** Trim this string. All leading occurences of characters from s are
	 *  removed. This CString instance is not modified.
	 * @param s A list of characters that should be trimmed.
	 * @return The trimmed string.
	 */
	CString TrimLeft_n(const CString& s = " \t\r\n") const;
	/** Trim this string. All trailing occurences of characters from s are
	 *  removed. This CString instance is not modified.
	 * @param s A list of characters that should be trimmed.
	 * @return The trimmed string.
	 */
	CString TrimRight_n(const CString& s = " \t\r\n") const;

	/** Trim a given prefix.
	 * @param sPrefix The prefix that should be removed.
	 * @return True if this string was modified.
	 */
	bool TrimPrefix(const CString& sPrefix = ":");
	/** Trim a given suffix.
	 * @param sSuffix The suffix that should be removed.
	 * @return True if this string was modified.
	 */
	bool TrimSuffix(const CString& sSuffix);
	/** Trim a given prefix.
	 * @param sPrefix The prefix that should be removed.
	 * @return A copy of this string without the prefix.
	 */
	CString TrimPrefix_n(const CString& sPrefix = ":") const;
	/** Trim a given suffix.
	 * @param sSuffix The suffix that should be removed.
	 * @return A copy of this string without the prefix.
	 */
	CString TrimSuffix_n(const CString& sSuffix) const;

	/** Find the position of the given substring.
	 * @param s The substring to search for.
	 * @param cs CaseSensitive if you want the comparison to be case
	 *                       sensitive, CaseInsensitive (default) otherwise.
	 * @return The position of the substring if found, CString::npos otherwise.
	 */
	size_t Find(const CString& s, CaseSensitivity cs = CaseInsensitive) const;
	/** Check whether the string starts with a given prefix.
	 * @param sPrefix The prefix.
	 * @param cs CaseSensitive if you want the comparison to be case
	 *                       sensitive, CaseInsensitive (default) otherwise.
	 * @return True if the string starts with prefix, false otherwise.
	 */
	bool StartsWith(const CString& sPrefix, CaseSensitivity cs = CaseInsensitive) const;
	/** Check whether the string ends with a given suffix.
	 * @param sSuffix The suffix.
	 * @param cs CaseSensitive if you want the comparison to be case
	 *                       sensitive, CaseInsensitive (default) otherwise.
	 * @return True if the string ends with suffix, false otherwise.
	 */
	bool EndsWith(const CString& sSuffix, CaseSensitivity cs = CaseInsensitive) const;
	/**
	 * Check whether the string contains a given string.
	 * @param s The string to search.
	 * @param bCaseSensitive Whether the search is case sensitive.
	 * @return True if this string contains the other string, falser otherwise.
	 */
	bool Contains(const CString& s, CaseSensitivity cs = CaseInsensitive) const;

	/** Remove characters from the beginning of this string.
	 * @param uLen The number of characters to remove.
	 * @return true if this string was modified.
	 */
	bool LeftChomp(size_type uLen = 1);
	/** Remove characters from the end of this string.
	 * @param uLen The number of characters to remove.
	 * @return true if this string was modified.
	 */
	bool RightChomp(size_type uLen = 1);
	/** Remove characters from the beginning of this string.
	 * This string object isn't modified.
	 * @param uLen The number of characters to remove.
	 * @return The result of the conversion.
	 */
	CString LeftChomp_n(size_type uLen = 1) const;
	/** Remove characters from the end of this string.
	 * This string object isn't modified.
	 * @param uLen The number of characters to remove.
	 * @return The result of the conversion.
	 */
	CString RightChomp_n(size_type uLen = 1) const;
	/** Remove controls characters from this string.
	 * Controls characters are color codes, and those in C0 set
	 * See https://en.wikipedia.org/wiki/C0_and_C1_control_codes
	 * @return The result of the conversion.
	 */
	CString& StripControls();
	/** Remove controls characters from this string.
	 * Controls characters are color codes, and those in C0 set
	 * See https://en.wikipedia.org/wiki/C0_and_C1_control_codes
	 * This string object isn't modified.
	 * @return The result of the conversion.
	 */
	CString StripControls_n() const;

private:
protected:
	unsigned char* strnchr(const unsigned char* src, unsigned char c, unsigned int iMaxBytes, unsigned char* pFill = nullptr, unsigned int* piCount = nullptr) const;
};

/**
 * @brief A dictionary for strings.
 *
 * This class maps strings to other strings.
 */
class MCString : public std::map<CString, CString> {
public:
	/** Construct an empty MCString. */
	MCString() : std::map<CString, CString>() {}
	/** Destruct this MCString. */
	virtual ~MCString() { clear(); }

	/** A static instance of an empty map. */
	static const MCString EmptyMap;

	/** Status codes that can be returned by WriteToDisk() and
	 * ReadFromDisk(). */
	enum status_t
	{
		/// No errors.
		MCS_SUCCESS   = 0,
		/// Opening the file failed.
		MCS_EOPEN     = 1,
		/// Writing to the file failed.
		MCS_EWRITE    = 2,
		/// WriteFilter() failed.
		MCS_EWRITEFIL = 3,
		/// ReadFilter() failed.
		MCS_EREADFIL  = 4
	};

	/** Write this map to a file.
	 * @param sPath The file name to write to.
	 * @param iMode The mode for the file.
	 * @return The result of the operation.
	 * @see WriteFilter.
	 */
	enum status_t WriteToDisk(const CString& sPath, mode_t iMode = 0644) const;
	/** Read a map from a file.
	 * @param sPath The file name to read from.
	 * @return The result of the operation.
	 * @see ReadFilter.
	 */
	enum status_t ReadFromDisk(const CString& sPath);

	/** Filter used while writing this map. This function is called by
	 * WriteToDisk() for each pair that is going to be written. This
	 * function has the chance to modify the data that will be written.
	 * @param sKey The key that will be written. Can be modified.
	 * @param sValue The value that will be written. Can be modified.
	 * @return true unless WriteToDisk() should fail with MCS_EWRITEFIL.
	 */
	virtual bool WriteFilter(CString& sKey, CString& sValue) const { return true; }
	/** Filter used while reading this map. This function is called by
	 * ReadFromDisk() for each pair that is beging read. This function has
	 * the chance to modify the data that is being read.
	 * @param sKey The key that was read. Can be modified.
	 * @param sValue The value that was read. Can be modified.
	 * @return true unless ReadFromDisk() should fail with MCS_EWRITEFIL.
	 */
	virtual bool ReadFilter(CString& sKey, CString& sValue) const { return true; }

	/** Encode a value so that it can safely be parsed by ReadFromDisk().
	 * This is an internal function.
	 */
	virtual CString& Encode(CString& sValue) const;
	/** Undo the effects of Encode(). This is an internal function. */
	virtual CString& Decode(CString& sValue) const;
};

#endif // !ZNCSTRING_H
