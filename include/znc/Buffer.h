/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _BUFFER_H
#define _BUFFER_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <deque>

using std::deque;

// Forward Declarations
class CClient;
// !Forward Declarations

class CBufLine {
public:
	CBufLine(const CString& sFormat, const CString& sText = "", time_t tm = 0);
	~CBufLine();
	CString GetLine(const CClient& Client, const MCString& msParams) const;
	void UpdateTime() { time(&m_tm); }

	// Setters
	void SetFormat(const CString& sFormat) { m_sFormat = sFormat; }
	void SetText(const CString& sText) { m_sText = sText; }
	void SetTime(time_t tm) { m_tm = tm; }
	// !Setters

	// Getters
	const CString& GetFormat() const { return m_sFormat; }
	const CString& GetText() const { return m_sText; }
	time_t GetTime() const { return m_tm; }
	// !Getters

private:
protected:
	CString m_sFormat;
	CString m_sText;
	time_t m_tm;
};

class CBuffer : private deque<CBufLine> {
public:
	CBuffer(unsigned int uLineCount = 100);
	~CBuffer();

	int AddLine(const CString& sFormat, const CString& sText = "", time_t tm = 0);
	/// Same as AddLine, but replaces a line whose format string starts with sMatch if there is one.
	int UpdateLine(const CString& sMatch, const CString& sFormat, const CString& sText = "");
	/// Same as UpdateLine, but does nothing if this exact line already exists.
	/// We need this because "/version" sends us the 005 raws again
	int UpdateExactLine(const CString& sFormat, const CString& sText = "");
	const CBufLine& GetBufLine(unsigned int uIdx) const;
	CString GetLine(unsigned int uIdx, const CClient& Client, const MCString& msParams = MCString::EmptyMap) const;
	unsigned int Size() const { return size(); }
	bool IsEmpty() const { return empty(); }
	void Clear() { clear(); }

	// Setters
	bool SetLineCount(unsigned int u, bool bForce = false);
	// !Setters

	// Getters
	unsigned int GetLineCount() const { return m_uLineCount; }
	// !Getters
private:
protected:
	unsigned int m_uLineCount;
};

#endif // !_BUFFER_H
