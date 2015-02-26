/*
 * Copyright (C) 2004-2015 ZNC, see the NOTICE file for details.
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

#ifndef _UTILS_H
#define _UTILS_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <assert.h>
#include <cstdio>
#include <fcntl.h>
#include <map>
#include <sys/file.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

static inline void SetFdCloseOnExec(int fd)
{
	int flags = fcntl(fd, F_GETFD, 0);
	if (flags < 0)
		return; // Ignore errors
	// When we execve() a new process this fd is now automatically closed.
	fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static const char g_HexDigits[] = "0123456789abcdef";

class CUtils {
public:
	CUtils();
	~CUtils();

	static CString GetIP(unsigned long addr);
	static unsigned long GetLongIP(const CString& sIP);

	static void PrintError(const CString& sMessage);
	static void PrintMessage(const CString& sMessage, bool bStrong = false);
	static void PrintPrompt(const CString& sMessage);
	static void PrintAction(const CString& sMessage);
	static void PrintStatus(bool bSuccess, const CString& sMessage = "");

#ifndef SWIGPERL
	// TODO refactor this
	static const CString sDefaultHash;
#endif

	static CString GetSaltedHashPass(CString& sSalt);
	static CString GetSalt();
	static CString SaltedMD5Hash(const CString& sPass, const CString& sSalt);
	static CString SaltedSHA256Hash(const CString& sPass, const CString& sSalt);
	static CString GetPass(const CString& sPrompt);
	static bool GetInput(const CString& sPrompt, CString& sRet, const CString& sDefault = "", const CString& sHint = "");
	static bool GetBoolInput(const CString& sPrompt, bool bDefault);
	static bool GetBoolInput(const CString& sPrompt, bool *pbDefault = nullptr);
	static bool GetNumInput(const CString& sPrompt, unsigned int& uRet, unsigned int uMin = 0, unsigned int uMax = ~0, unsigned int uDefault = ~0);

	static unsigned long long GetMillTime() {
		struct timeval tv;
		unsigned long long iTime = 0;
		gettimeofday(&tv, nullptr);
		iTime = (unsigned long long) tv.tv_sec * 1000;
		iTime += ((unsigned long long) tv.tv_usec / 1000);
		return iTime;
	}
#ifdef HAVE_LIBSSL
	static void GenerateCert(FILE *pOut, const CString& sHost = "");
#endif /* HAVE_LIBSSL */

	static CString CTime(time_t t, const CString& sTZ);
	static CString FormatTime(time_t t, const CString& sFormat, const CString& sTZ);
	static CString FormatServerTime(const timeval& tv);
	static SCString GetTimezones();
	static SCString GetEncodings();

	static MCString GetMessageTags(const CString& sLine);
	static void SetMessageTags(CString& sLine, const MCString& mssTags);

private:
protected:
};

class CException {
public:
	typedef enum {
		EX_Shutdown,
		EX_Restart
	} EType;

	CException(EType e) {
		m_eType = e;
	}
	virtual ~CException() {}

	EType GetType() const { return m_eType; }
private:
protected:
	EType  m_eType;
};


/** Generate a grid-like output from a given input.
 *
 *  @code
 *  CTable table;
 *  table.AddColumn("a");
 *  table.AddColumn("b");
 *  table.AddRow();
 *  table.SetCell("a", "hello");
 *  table.SetCell("b", "world");
 *
 *  unsigned int idx = 0;
 *  CString tmp;
 *  while (table.GetLine(idx++, tmp)) {
 *      // Output tmp somehow
 *  }
 *  @endcode
 *
 *  The above code would generate the following output:
 *  @verbatim
+-------+-------+
| a     | b     |
+-------+-------+
| hello | world |
+-------+-------+@endverbatim
 */
class CTable : protected std::vector<std::vector<CString> > {
public:
	/** Constructor
	 *
	 *  @param uPreferredWidth If width of table is bigger than this, text in cells will be wrapped to several lines, if possible
	 */
	explicit CTable(size_type uPreferredWidth = 110) : m_uPreferredWidth(uPreferredWidth) {}
	virtual ~CTable() {}

	/** Adds a new column to the table.
	 *  Please note that you should add all columns before starting to fill
	 *  the table!
	 *  @param sName The name of the column.
	 *  @param bWrappable True if long lines can be wrapped in the same cell.
	 *  @return false if a column by that name already existed.
	 */
	bool AddColumn(const CString& sName, bool bWrappable = true);

	/** Adds a new row to the table.
	 *  After calling this you can fill the row with content.
	 *  @return The index of this row
	 */
	size_type AddRow();

	/** Sets a given cell in the table to a value.
	 *  @param sColumn The name of the column you want to fill.
	 *  @param sValue The value to write into that column.
	 *  @param uRowIdx The index of the row to use as returned by AddRow().
	 *                 If this is not given, the last row will be used.
	 *  @return True if setting the cell was successful.
	 */
	bool SetCell(const CString& sColumn, const CString& sValue, size_type uRowIdx = ~0);

	/** Get a line of the table's output
	 *  @param uIdx The index of the line you want.
	 *  @param sLine This string will receive the output.
	 *  @return True unless uIdx is past the end of the table.
	 */
	bool GetLine(unsigned int uIdx, CString& sLine) const;

	/** Return the width of the given column.
	 *  Please note that adding and filling new rows might change the
	 *  result of this function!
	 *  @param uIdx The index of the column you are interested in.
	 *  @return The width of the column.
	 */
	CString::size_type GetColumnWidth(unsigned int uIdx) const;

	/// Completely clear the table.
	void Clear();

	/// @return The number of rows in this table, not counting the header.
	using std::vector<std::vector<CString> >::size;

	/// @return True if this table doesn't contain any rows.
	using std::vector<std::vector<CString> >::empty;
private:
	unsigned int GetColumnIndex(const CString& sName) const;
	VCString Render() const;
	static VCString WrapWords(const CString& s, size_type uWidth);

protected:
	VCString m_vsHeaders;
	std::vector<CString::size_type> m_vuMaxWidths;  // Column don't need to be bigger than this
	std::vector<CString::size_type> m_vuMinWidths;  // Column can't be thiner than this
	std::vector<bool> m_vbWrappable;
	size_type m_uPreferredWidth;
	mutable VCString m_vsOutput;  // Rendered table
};


#ifdef HAVE_LIBSSL
#include <openssl/aes.h>
#include <openssl/blowfish.h>
#include <openssl/md5.h>
//! does Blowfish w/64 bit feedback, no padding
class CBlowfish {
public:
	/**
	 * @param sPassword key to encrypt with
	 * @param iEncrypt encrypt method (BF_DECRYPT or BF_ENCRYPT)
	 * @param sIvec what to set the ivector to start with, default sets it all 0's
	 */
	CBlowfish(const CString & sPassword, int iEncrypt, const CString & sIvec = "");
	~CBlowfish();

	//! output must be freed
	static unsigned char *MD5(const unsigned char *input, u_int ilen);

	//! returns an md5 of the CString (not hex encoded)
	static CString MD5(const CString & sInput, bool bHexEncode = false);

	//! output must be the same size as input
	void Crypt(unsigned char *input, unsigned char *output, u_int ibytes);

	//! must free result
	unsigned char * Crypt(unsigned char *input, u_int ibytes);
	CString Crypt(const CString & sData);

private:
	unsigned char  *m_ivec;
	BF_KEY          m_bkey;
	int             m_iEncrypt, m_num;
};

#endif /* HAVE_LIBSSL */

/**
 * @class TCacheMap
 * @author prozac <prozac@rottenboy.com>
 * @brief Insert an object with a time-to-live and check later if it still exists
 */
template<typename K, typename V = bool>
class TCacheMap {
public:
	TCacheMap(unsigned int uTTL = 5000) {
		m_uTTL = uTTL;
	}

	virtual ~TCacheMap() {}

	/**
	 * @brief This function adds an item to the cache using the default time-to-live value
	 * @param Item the item to add to the cache
	 */
	void AddItem(const K& Item) {
		AddItem(Item, m_uTTL);
	}

	/**
	 * @brief This function adds an item to the cache using a custom time-to-live value
	 * @param Item the item to add to the cache
	 * @param uTTL the time-to-live for this specific item
	 */
	void AddItem(const K& Item, unsigned int uTTL) {
		AddItem(Item, V(), uTTL);
	}

	/**
	 * @brief This function adds an item to the cache using the default time-to-live value
	 * @param Item the item to add to the cache
	 * @param Val The value associated with the key Item
	 */
	void AddItem(const K& Item, const V& Val) {
		AddItem(Item, Val, m_uTTL);
	}

	/**
	 * @brief This function adds an item to the cache using a custom time-to-live value
	 * @param Item the item to add to the cache
	 * @param Val The value associated with the key Item
	 * @param uTTL the time-to-live for this specific item
	 */
	void AddItem(const K& Item, const V& Val, unsigned int uTTL) {
		if (!uTTL) {             // If time-to-live is zero we don't want to waste our time adding it
			RemItem(Item);   // Remove the item incase it already exists
			return;
		}

		m_mItems[Item] = value(CUtils::GetMillTime() + uTTL, Val);
	}

	/**
	 * @brief Performs a Cleanup() and then checks to see if your item exists
	 * @param Item The item to check for
	 * @return true if item exists
	 */
	bool HasItem(const K& Item) {
		Cleanup();
		return (m_mItems.find(Item) != m_mItems.end());
	}

	/**
	 * @brief Performs a Cleanup() and returns a pointer to the object, or NULL
	 * @param Item The item to check for
	 * @return Pointer to the item or NULL if there is no suitable one
	 */
	V* GetItem(const K& Item) {
		Cleanup();
		iterator it = m_mItems.find(Item);
		if (it == m_mItems.end())
			return nullptr;
		return &it->second.second;
	}

	/**
	 * @brief Removes a specific item from the cache
	 * @param Item The item to be removed
	 * @return true if item existed and was removed, false if it never existed
	 */
	bool RemItem(const K& Item) {
		return (m_mItems.erase(Item) != 0);
	}

	/**
	 * @brief Cycles through the queue removing all of the stale entries
	 */
	void Cleanup() {
		iterator it = m_mItems.begin();

		while (it != m_mItems.end()) {
			if (CUtils::GetMillTime() > (it->second.first)) {
				m_mItems.erase(it++);
			} else {
				++it;
			}
		}
	}

	/**
	 * @brief Clear all entries
	 */
	void Clear() {
		m_mItems.clear();
	}

	// Setters
	void SetTTL(unsigned int u) { m_uTTL = u; }
	// !Setters
	// Getters
	unsigned int GetTTL() const { return m_uTTL; }
	// !Getters
protected:
	typedef std::pair<unsigned long long, V> value;
	typedef typename std::map<K, value>::iterator iterator;
	std::map<K, value>   m_mItems;   //!< Map of cached items.  The value portion of the map is for the expire time
	unsigned int         m_uTTL;     //!< Default time-to-live duration
};

#endif // !_UTILS_H
