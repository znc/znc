/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _TEMPLATE_H
#define _TEMPLATE_H

#include "zncconfig.h"
#include "Utils.h"
#include <iostream>

using std::ostream;
using std::endl;

class CTemplate;

class CTemplateTagHandler {
public:
	CTemplateTagHandler() {}
	virtual ~CTemplateTagHandler() {}

	virtual bool HandleVar(CTemplate& Tmpl, const CString& sName, const CString& sArgs, CString& sOutput) {
		return false;
	}

	virtual bool HandleTag(CTemplate& Tmpl, const CString& sName, const CString& sArgs, CString& sOutput) {
		return false;
	}

	virtual bool HandleIf(CTemplate& Tmpl, const CString& sName, const CString& sArgs, CString& sOutput) {
		return HandleVar(Tmpl, sName, sArgs, sOutput);
	}

	virtual bool HandleValue(CTemplate& Tmpl, CString& sValue, const MCString& msOptions) {
		return false;
	}
private:
};
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
	CString::EEscape   m_eEscapeFrom;
	CString::EEscape   m_eEscapeTo;
};


class CTemplateLoopContext {
public:
	CTemplateLoopContext(unsigned long uFilePos, const CString& sLoopName, bool bReverse, vector<CTemplate*>* pRows) {
		m_uFilePosition = uFilePos;
		m_sName = sLoopName;
		m_uRowIndex = 0;
		m_bReverse = bReverse;
		m_pvRows = pRows;
		m_bHasData = false;
	}

	virtual ~CTemplateLoopContext() {}

	// Setters
	void SetHasData(bool b = true) { m_bHasData = b; }
	void SetName(const CString& s) { m_sName = s; }
	void SetRowIndex(unsigned int u) { m_uRowIndex = u; }
	unsigned int IncRowIndex() { return ++m_uRowIndex; }
	unsigned int DecRowIndex() { if (m_uRowIndex == 0) { return 0; } return --m_uRowIndex; }
	void SetFilePosition(unsigned int u) { m_uFilePosition = u; }
	// !Setters

	// Getters
	bool HasData() const { return m_bHasData; }
	const CString& GetName() const { return m_sName; }
	unsigned long GetFilePosition() const { return m_uFilePosition; }
	unsigned int GetRowIndex() const { return m_uRowIndex; }
	unsigned int GetRowCount() { return m_pvRows->size(); }
	vector<CTemplate*>* GetRows() { return m_pvRows; }
	CTemplate* GetNextRow() { return GetRow(IncRowIndex()); }
	CTemplate* GetCurRow() { return GetRow(m_uRowIndex); }

	CTemplate* GetRow(unsigned int uIndex);
	CString GetValue(const CString& sName, bool bFromIf = false);
	// !Getters
private:
	bool                  m_bReverse;       //!< Iterate through this loop in reverse order
	bool                  m_bHasData;       //!< Tells whether this loop has real data or not
	CString               m_sName;          //!< The name portion of the <?LOOP name?> tag
	unsigned int          m_uRowIndex;      //!< The index of the current row we're on
	unsigned long         m_uFilePosition;  //!< The file position of the opening <?LOOP?> tag
	vector<CTemplate*>*   m_pvRows;         //!< This holds pointers to the templates associated with this loop
};


class CTemplate : public MCString {
public:
	CTemplate() : MCString(), m_spOptions(new CTemplateOptions) {
		Init();
	}

	CTemplate(const CString& sFileName) : MCString(), m_sFileName(sFileName), m_spOptions(new CTemplateOptions) {
		Init();
	}

	CTemplate(const CSmartPtr<CTemplateOptions>& Options, CTemplate* pParent = NULL) : MCString(), m_spOptions(Options) {
		Init();
		m_pParent = pParent;
	}

	virtual ~CTemplate();

	//! Class for implementing custom tags in subclasses
	void AddTagHandler(CSmartPtr<CTemplateTagHandler> spTagHandler) {
		m_vspTagHandlers.push_back(spTagHandler);
	}

	vector<CSmartPtr<CTemplateTagHandler> >& GetTagHandlers() {
		if (m_pParent) {
			return m_pParent->GetTagHandlers();
		}

		return m_vspTagHandlers;
	}

	CString ResolveLiteral(const CString& sString);

	void Init();

	CTemplate* GetParent(bool bRoot);
	CString ExpandFile(const CString& sFilename, bool bFromInc = false);
	bool SetFile(const CString& sFileName);

	void SetPath(const CString& sPath);  // Sets the dir:dir:dir type path to look at for templates, as of right now no ../../.. protection
	CString MakePath(const CString& sPath) const;
	void PrependPath(const CString& sPath, bool bIncludesOnly = false);
	void AppendPath(const CString& sPath, bool bIncludesOnly = false);
	void RemovePath(const CString& sPath);
	void ClearPaths();
	bool PrintString(CString& sRet);
	bool Print(ostream& oOut);
	bool Print(const CString& sFileName, ostream& oOut);
	bool ValidIf(const CString& sArgs);
	bool ValidExpr(const CString& sExpr);
	bool IsTrue(const CString& sName);
	bool HasLoop(const CString& sName);
	CString GetValue(const CString& sName, bool bFromIf = false);
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
	CTemplate*                               m_pParent;
	CString                                  m_sFileName;
	list<pair<CString, bool> >               m_lsbPaths;
	map<CString, vector<CTemplate*> >        m_mvLoops;
	vector<CTemplateLoopContext*>            m_vLoopContexts;
	CSmartPtr<CTemplateOptions>              m_spOptions;
	vector<CSmartPtr<CTemplateTagHandler> >  m_vspTagHandlers;
};

#endif // !_TEMPLATE_H
