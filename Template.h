/*
 * Copyright (C) 2004-2007  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _TEMPLATE_H
#define _TEMPLATE_H

#include "FileUtils.h"
#include "Utils.h"
#include <iostream>

using std::ostream;
using std::cout;
using std::endl;

class CTemplate;

class CTemplateOptions {
public:
	CTemplateOptions() {
		m_eEscapeFrom = CString::EASCII;
		m_eEscapeTo = CString::EASCII;
	}

	virtual ~CTemplateOptions() {}

	void Parse(const CString& sLine);

	// Getters
	CString::EEscape GetEscapeFrom() const { return m_eEscapeFrom; }
	CString::EEscape GetEscapeTo() const { return m_eEscapeTo; }
	// !Getters
private:
	CString::EEscape	m_eEscapeFrom;
	CString::EEscape	m_eEscapeTo;
};


class CTemplateLoopContext {
public:
	CTemplateLoopContext(unsigned long uFilePos, const CString& sLoopName, vector<CTemplate*>* pRows) {
		m_uFilePosition = uFilePos;
		m_sName = sLoopName;
		m_uRowIndex = 0;
		m_pvRows = pRows;
	}

	virtual ~CTemplateLoopContext() {}

	// Setters
	void SetName(const CString& s) { m_sName = s; }
	void SetRowIndex(unsigned int u) { m_uRowIndex = u; }
	unsigned int IncRowIndex() { return ++m_uRowIndex; }
	unsigned int DecRowIndex() { if (m_uRowIndex == 0) { return 0; } return --m_uRowIndex; }
	void SetFilePosition(unsigned int u) { m_uFilePosition = u; }
	// !Setters

	// Getters
	const CString& GetName() const { return m_sName; }
	unsigned long GetFilePosition() const { return m_uFilePosition; }
	unsigned int GetRowIndex() const { return m_uRowIndex; }
	const unsigned int GetRowCount() { return m_pvRows->size(); }
	vector<CTemplate*>* GetRows() { return m_pvRows; }
	CTemplate* GetNextRow() { return GetRow(IncRowIndex()); }
	CTemplate* GetCurRow() { return GetRow(m_uRowIndex); }

	CTemplate* GetRow(unsigned int uIndex);
	CString GetValue(const CString& sName);
	// !Getters
private:
protected:
	CString				m_sName;			//! The name portion of the <?LOOP name?> tag
	unsigned int		m_uRowIndex;		//! The index of the current row we're on
	unsigned long		m_uFilePosition;	//! The file position of the opening <?LOOP?> tag
	vector<CTemplate*>*	m_pvRows;			//! This holds pointers to the templates associated with this loop
};


class CTemplate : public MCString {
public:
	CTemplate() : MCString(), m_spOptions(new CTemplateOptions) {}
	CTemplate(const CString& sFileName) : MCString(), m_sFileName(sFileName), m_spOptions(new CTemplateOptions) {}
	CTemplate(const CSmartPtr<CTemplateOptions>& Options) : MCString(), m_spOptions(Options) {}
	virtual ~CTemplate();

	bool SetFile(const CString& sFileName);
	bool Print(ostream& oOut = cout);
	bool Print(const CString& sFileName, ostream& oOut = cout);
	bool ValidIf(const CString& sArgs);
	bool ValidExpr(const CString& sExpr);
	bool IsTrue(const CString& sName);
	bool HasLoop(const CString& sName);
	CString GetValue(const CString& sName);
	CTemplate& AddRow(const CString& sName);
	CTemplate* GetRow(const CString& sName, unsigned int uIndex);
	vector<CTemplate*>* GetLoop(const CString& sName);
	void DelCurLoopContext();
	CTemplateLoopContext* GetCurLoopContext();
	CTemplate* GetCurTemplate();

	// Getters
	const CString& GetFileName() const { return m_sFileName; }
	// !Getters
private:
protected:
	CString								m_sFileName;
	map<CString, vector<CTemplate*> >	m_mvLoops;
	vector<CTemplateLoopContext*>		m_vLoopContexts;
	CSmartPtr<CTemplateOptions>			m_spOptions;
};

#endif // !_TEMPLATE_H

