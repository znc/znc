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

#ifndef ZNC_CONFIG_H
#define ZNC_CONFIG_H

#include <znc/zncconfig.h>
#include <znc/ZNCString.h>

class CFile;
class CConfig;

struct CConfigEntry {
    CConfigEntry();
    CConfigEntry(const CConfig& Config);
    CConfigEntry(const CConfigEntry& other);
    ~CConfigEntry();
    CConfigEntry& operator=(const CConfigEntry& other);

    CConfig* m_pSubConfig;
};

class CConfig {
  public:
    CConfig() : m_ConfigEntries(), m_SubConfigs(), m_SubConfigNameSets() {}

    typedef std::map<CString, VCString> EntryMap;
    typedef std::pair<CString, CConfigEntry> SubConfigEntry;
    typedef std::vector<SubConfigEntry> SubConfig;
    typedef std::map<CString, SubConfig> SubConfigMap;

    typedef EntryMap::const_iterator EntryMapIterator;
    typedef SubConfigMap::const_iterator SubConfigMapIterator;

    EntryMapIterator BeginEntries() const { return m_ConfigEntries.begin(); }
    EntryMapIterator EndEntries() const { return m_ConfigEntries.end(); }

    SubConfigMapIterator BeginSubConfigs() const {
        return m_SubConfigs.begin();
    }
    SubConfigMapIterator EndSubConfigs() const { return m_SubConfigs.end(); }

    void AddKeyValuePair(const CString& sName, const CString& sValue) {
        if (sName.empty() || sValue.empty()) {
            return;
        }

        m_ConfigEntries[sName].push_back(sValue);
    }

    bool AddSubConfig(const CString& sTag, const CString& sName,
                      CConfig Config) {
        auto& nameset = m_SubConfigNameSets[sTag];

        if (nameset.find(sName) != nameset.end()) return false;

        nameset.insert(sName);
        m_SubConfigs[sTag].emplace_back(sName, Config);

        return true;
    }

    bool FindStringVector(const CString& sName, VCString& vsList,
                          bool bErase = true) {
        EntryMap::iterator it = m_ConfigEntries.find(sName);
        vsList.clear();
        if (it == m_ConfigEntries.end()) return false;
        vsList = it->second;

        if (bErase) {
            m_ConfigEntries.erase(it);
        }

        return true;
    }

    bool FindStringEntry(const CString& sName, CString& sRes,
                         const CString& sDefault = "") {
        EntryMap::iterator it = m_ConfigEntries.find(sName);
        sRes = sDefault;
        if (it == m_ConfigEntries.end() || it->second.empty()) return false;
        sRes = it->second.front();
        it->second.erase(it->second.begin());
        if (it->second.empty()) m_ConfigEntries.erase(it);
        return true;
    }

    bool FindBoolEntry(const CString& sName, bool& bRes,
                       bool bDefault = false) {
        CString s;
        if (FindStringEntry(sName, s)) {
            bRes = s.ToBool();
            return true;
        }
        bRes = bDefault;
        return false;
    }

    bool FindUIntEntry(const CString& sName, unsigned int& uRes,
                       unsigned int uDefault = 0) {
        CString s;
        if (FindStringEntry(sName, s)) {
            uRes = s.ToUInt();
            return true;
        }
        uRes = uDefault;
        return false;
    }

    bool FindUShortEntry(const CString& sName, unsigned short& uRes,
                         unsigned short uDefault = 0) {
        CString s;
        if (FindStringEntry(sName, s)) {
            uRes = s.ToUShort();
            return true;
        }
        uRes = uDefault;
        return false;
    }

    bool FindDoubleEntry(const CString& sName, double& fRes,
                         double fDefault = 0) {
        CString s;
        if (FindStringEntry(sName, s)) {
            fRes = s.ToDouble();
            return true;
        }
        fRes = fDefault;
        return false;
    }

    bool FindSubConfig(const CString& sTag, SubConfig& Config,
                       bool bErase = true) {
        auto it = m_SubConfigs.find(sTag);
        if (it == m_SubConfigs.end()) {
            Config.clear();
            return false;
        }
        Config = it->second;

        if (bErase) {
            m_SubConfigs.erase(it);
            m_SubConfigNameSets.erase(sTag);
        }

        return true;
    }

    bool empty() const {
        return m_ConfigEntries.empty() && m_SubConfigs.empty();
    }

    bool Parse(CFile& file, CString& sErrorMsg);
    void Write(CFile& file, unsigned int iIndentation = 0);

  private:
    typedef SCString SubConfigNameSet;
    typedef std::map<CString, SubConfigNameSet> SubConfigNameSetMap;

    EntryMap m_ConfigEntries;
    SubConfigMap m_SubConfigs;
    SubConfigNameSetMap m_SubConfigNameSets;
};

#endif  // !ZNC_CONFIG_H
