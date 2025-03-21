/*
 * Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
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

#ifndef ZNC_UTILS_H
#define ZNC_UTILS_H

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

static inline void SetFdCloseOnExec(int fd) {
    int flags = fcntl(fd, F_GETFD, 0);
    if (flags < 0) return;  // Ignore errors
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
    static CString GetHostName();

    static void PrintError(const CString& sMessage);
    static void PrintMessage(const CString& sMessage, bool bStrong = false);
    static void PrintPrompt(const CString& sMessage);
    static void PrintAction(const CString& sMessage);
    static void PrintStatus(bool bSuccess, const CString& sMessage = "");

	/** Asks password from stdin, with confirmation.
	 *
	 * @returns Piece of znc.conf with <Pass> block
	 * */
    static CString AskSaltedHashPassForConfig();

    static CString GetSalt();
    static CString SaltedMD5Hash(const CString& sPass, const CString& sSalt);
    static CString SaltedSHA256Hash(const CString& sPass, const CString& sSalt);
    static CString SaltedHash(const CString& sPass, const CString& sSalt);
    static CString GetPass(const CString& sPrompt);
    static bool GetInput(const CString& sPrompt, CString& sRet,
                         const CString& sDefault = "",
                         const CString& sHint = "");
    static bool GetBoolInput(const CString& sPrompt, bool bDefault);
    static bool GetBoolInput(const CString& sPrompt, bool* pbDefault = nullptr);
    static bool GetNumInput(const CString& sPrompt, unsigned int& uRet,
                            unsigned int uMin = 0, unsigned int uMax = ~0,
                            unsigned int uDefault = ~0);

    static timeval GetTime();
    static unsigned long long GetMillTime();
#ifdef HAVE_LIBSSL
    static void GenerateCert(FILE* pOut);
#endif /* HAVE_LIBSSL */

    static CString CTime(time_t t, const CString& sTZ);
    static CString FormatTime(time_t t, const CString& sFormat,
                              const CString& sTZ);
    /** Supports an additional format specifier for formatting sub-second values:
     *
     * - %f - sub-second fraction
     *   - %3f - millisecond (default, if no width is specified)
     *   - %6f - microsecond
     *
     * However, note that timeval only supports microsecond precision
     * (thus, formatting with higher-than-microsecond precision will
     * always result in trailing zeroes), and IRC server-time is specified
     * in millisecond precision (thus formatting received timestamps with
     * higher-than-millisecond precision will always result in trailing
     * zeroes).
     */
    static CString FormatTime(const timeval& tv, const CString& sFormat,
                              const CString& sTZ);
    static CString FormatServerTime(const timeval& tv);
    static timeval ParseServerTime(const CString& sTime);
    static SCString GetTimezones();
    static SCString GetEncodings();
    /** CIDR notation checker, e.g. "192.0.2.0/24" or "2001:db8::/32"
     *
     *  For historical reasons also allows wildcards, e.g. "192.168.*"
     */
    static bool CheckCIDR(const CString& sIP, const CString& sRange);

    /// @deprecated Use CMessage instead
    static MCString GetMessageTags(const CString& sLine);
    /// @deprecated Use CMessage instead
    static void SetMessageTags(CString& sLine, const MCString& mssTags);

  private:
  protected:
};

class CException {
  public:
    typedef enum { EX_Shutdown, EX_Restart } EType;

    CException(EType e) : m_eType(e) {}
    virtual ~CException() {}

    EType GetType() const { return m_eType; }

  private:
  protected:
    EType m_eType;
};


/** Generate a grid-like or list-like output from a given input.
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
 *
 *  If the table has at most two columns, one can switch to ListStyle output
 *  like so:
 *  @code
 *  CTable table;
 *  table.AddColumn("a");
 *  table.AddColumn("b");
 *  table.SetStyle(CTable::ListStyle);
 *  // ...
 *  @endcode
 *
 *  This will yield the following (Note that the header is omitted; asterisks
 *  denote bold text):
 *  @verbatim
*hello*: world@endverbatim
 */
class CTable : protected std::vector<std::vector<CString>> {
  public:
    enum EStyle { GridStyle, ListStyle };
    CTable() {}
    virtual ~CTable() {}

    /** Adds a new column to the table.
     *  Please note that you should add all columns before starting to fill
     *  the table!
     *  @param sName The name of the column.
     *  @return false if a column by that name already existed or the current 
     *  style does not allow this many columns.
     */
    bool AddColumn(const CString& sName);

    /** Selects the output style of the table.
     *  Select between different styles for printing. Default is GridStyle.
     *  @param eNewStyle Table style type.
     *  @return false if the style cannot be applied (usually too many columns).
     */
    bool SetStyle(EStyle eNewStyle);

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
    bool SetCell(const CString& sColumn, const CString& sValue,
                 size_type uRowIdx = ~0);

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
    using std::vector<std::vector<CString>>::size;

    /// @return True if this table doesn't contain any rows.
    using std::vector<std::vector<CString>>::empty;

  private:
    unsigned int GetColumnIndex(const CString& sName) const;

  protected:
    std::vector<CString> m_vsHeaders;
    // Used to cache the width of a column
    std::map<CString, CString::size_type> m_msuWidths;
    EStyle eStyle = GridStyle;
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
    CBlowfish(const CString& sPassword, int iEncrypt,
              const CString& sIvec = "");
    ~CBlowfish();

    CBlowfish(const CBlowfish&) = default;
    CBlowfish& operator=(const CBlowfish&) = default;

    //! output must be freed
    static unsigned char* MD5(const unsigned char* input, unsigned int ilen);

    //! returns an md5 of the CString (not hex encoded)
    static CString MD5(const CString& sInput, bool bHexEncode = false);

    //! output must be the same size as input
    void Crypt(unsigned char* input, unsigned char* output,
               unsigned int ibytes);

    //! must free result
    unsigned char* Crypt(unsigned char* input, unsigned int ibytes);
    CString Crypt(const CString& sData);

  private:
    unsigned char* m_ivec;
    BF_KEY m_bkey;
    int m_iEncrypt, m_num;
};

#endif /* HAVE_LIBSSL */

/**
 * @class TCacheMap
 * @author prozac <prozac@rottenboy.com>
 * @brief Insert an object with a time-to-live and check later if it still exists
 */
template <typename K, typename V = bool>
class TCacheMap {
  public:
    TCacheMap(unsigned int uTTL = 5000) : m_mItems(), m_uTTL(uTTL) {}

    virtual ~TCacheMap() {}

    /**
     * @brief This function adds an item to the cache using the default time-to-live value
     * @param Item the item to add to the cache
     */
    void AddItem(const K& Item) { AddItem(Item, m_uTTL); }

    /**
     * @brief This function adds an item to the cache using a custom time-to-live value
     * @param Item the item to add to the cache
     * @param uTTL the time-to-live for this specific item
     */
    void AddItem(const K& Item, unsigned int uTTL) { AddItem(Item, V(), uTTL); }

    /**
     * @brief This function adds an item to the cache using the default time-to-live value
     * @param Item the item to add to the cache
     * @param Val The value associated with the key Item
     */
    void AddItem(const K& Item, const V& Val) { AddItem(Item, Val, m_uTTL); }

    /**
     * @brief This function adds an item to the cache using a custom time-to-live value
     * @param Item the item to add to the cache
     * @param Val The value associated with the key Item
     * @param uTTL the time-to-live for this specific item
     */
    void AddItem(const K& Item, const V& Val, unsigned int uTTL) {
        if (!uTTL) {
            // If time-to-live is zero we don't want to waste our time adding
            // it
            RemItem(Item);  // Remove the item incase it already exists
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
     * @brief Performs a Cleanup() and returns a pointer to the object, or nullptr
     * @param Item The item to check for
     * @return Pointer to the item or nullptr if there is no suitable one
     */
    V* GetItem(const K& Item) {
        Cleanup();
        iterator it = m_mItems.find(Item);
        if (it == m_mItems.end()) return nullptr;
        return &it->second.second;
    }

    /**
     * @brief Removes a specific item from the cache
     * @param Item The item to be removed
     * @return true if item existed and was removed, false if it never existed
     */
    bool RemItem(const K& Item) { return (m_mItems.erase(Item) != 0); }

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
    void Clear() { m_mItems.clear(); }

    /**
     * @brief Returns all entries
     */
    std::map<K, V> GetItems() {
        Cleanup();
        std::map<K, V> mItems;
        for (const auto& it : m_mItems) {
            mItems[it.first] = it.second.second;
        }
        return mItems;
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
    std::map<K, value>
        m_mItems;  //!< Map of cached items.  The value portion of the map is for the expire time
    unsigned int m_uTTL;  //!< Default time-to-live duration
};

#endif  // !ZNC_UTILS_H
