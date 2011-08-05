/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "zncconfig.h"
#include "ZNCString.h"

class CFile;

class CConfig {
public:
	struct CConfigEntry {
		CConfigEntry();
		CConfigEntry(const CConfig& Config);
		CConfigEntry(const CConfigEntry& other);
		~CConfigEntry();
		CConfigEntry& operator=(const CConfigEntry& other);

		CConfig* m_pSubConfig;
	};

	typedef map<CString, VCString> EntryMap;
	typedef map<CString, CConfigEntry> SubConfig;
	typedef map<CString, SubConfig> SubConfigMap;

	typedef EntryMap::const_iterator EntryMapIterator;
	typedef SubConfigMap::const_iterator SubConfigMapIterator;

	EntryMapIterator BeginEntries() const {
		return m_ConfigEntries.begin();
	}
	EntryMapIterator EndEntries() const {
		return m_ConfigEntries.end();
	}

	SubConfigMapIterator BeginSubConfigs() const {
		return m_SubConfigs.begin();
	}
	SubConfigMapIterator EndSubConfigs() const {
		return m_SubConfigs.end();
	}

	bool FindStringVector(const CString& sName, VCString& vsList) {
		EntryMap::iterator it = m_ConfigEntries.find(sName);
		vsList.clear();
		if (it == m_ConfigEntries.end())
			return false;
		vsList = it->second;
		m_ConfigEntries.erase(it);
		return true;
	}

	bool FindStringEntry(const CString& sName, CString& sRes) {
		EntryMap::iterator it = m_ConfigEntries.find(sName);
		sRes.clear();
		if (it == m_ConfigEntries.end() || it->second.empty())
			return false;
		sRes = it->second.front();
		it->second.erase(it->second.begin());
		if (it->second.empty())
			m_ConfigEntries.erase(it);
		return true;
	}

	bool FindSubConfig(const CString& sName, SubConfig& Config) {
		SubConfigMap::iterator it = m_SubConfigs.find(sName);
		if (it == m_SubConfigs.end()) {
			Config.clear();
			return false;
		}
		Config = it->second;
		m_SubConfigs.erase(it);
		return true;
	}

	bool empty() const {
		return m_ConfigEntries.empty() && m_SubConfigs.empty();
	}

	bool Parse(CFile& file, CString& sErrorMsg);

private:
	EntryMap m_ConfigEntries;
	SubConfigMap m_SubConfigs;
};

#endif // !CONFIG_H
