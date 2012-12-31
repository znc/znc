/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _BUFFER_H
#define _BUFFER_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>
#include <sys/time.h>
#include <deque>

// Forward Declarations
class CClient;
// !Forward Declarations

class CBufLine {
public:
	CBufLine() { throw 0; } // shouldn't be called, but is needed for compilation
	CBufLine(const CString& sFormat, const CString& sText = "", const timeval* ts = 0);
	~CBufLine();
	CString GetLine(const CClient& Client, const MCString& msParams) const;
	void UpdateTime();

	// Setters
	void SetFormat(const CString& sFormat) { m_sFormat = sFormat; }
	void SetText(const CString& sText) { m_sText = sText; }
	void SetTime(const timeval& ts) { m_time = ts; }
	// !Setters

	// Getters
	const CString& GetFormat() const { return m_sFormat; }
	const CString& GetText() const { return m_sText; }
	timeval GetTime() const { return m_time; }
	// !Getters

private:
protected:
	CString  m_sFormat;
	CString  m_sText;
	timeval  m_time;
};

class CBuffer : private std::deque<CBufLine> {
public:
	CBuffer(unsigned int uLineCount = 100);
	~CBuffer();

	size_type AddLine(const CString& sFormat, const CString& sText = "", const timeval* ts = 0);
	/// Same as AddLine, but replaces a line whose format string starts with sMatch if there is one.
	size_type UpdateLine(const CString& sMatch, const CString& sFormat, const CString& sText = "");
	/// Same as UpdateLine, but does nothing if this exact line already exists.
	/// We need this because "/version" sends us the 005 raws again
	size_type UpdateExactLine(const CString& sFormat, const CString& sText = "");
	const CBufLine& GetBufLine(unsigned int uIdx) const;
	CString GetLine(size_type uIdx, const CClient& Client, const MCString& msParams = MCString::EmptyMap) const;
	size_type Size() const { return size(); }
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
