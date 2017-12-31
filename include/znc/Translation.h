/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
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

#ifndef ZNC_TRANSLATION_H
#define ZNC_TRANSLATION_H

#include <znc/ZNCString.h>
#include <unordered_map>

struct CTranslationInfo {
    static std::map<CString, CTranslationInfo> GetTranslations();

    CString sSelfName;
};

// All instances of modules share single message map using this class stored in
// CZNC.
class CTranslation {
  public:
    static CTranslation& Get();
    CString Singular(const CString& sDomain, const CString& sContext,
                     const CString& sEnglish);
    CString Plural(const CString& sDomain, const CString& sContext,
                   const CString& sEnglish, const CString& sEnglishes,
                   int iNum);

    void PushLanguage(const CString& sLanguage);
    void PopLanguage();

    void NewReference(const CString& sDomain);
    void DelReference(const CString& sDomain);

  private:
    // Domain is either "znc" or "znc-foo" where foo is a module name
    const std::locale& LoadTranslation(const CString& sDomain);
    std::unordered_map<CString /* domain */,
                       std::unordered_map<CString /* language */, std::locale>>
        m_Translations;
    VCString m_sLanguageStack;
    std::unordered_map<CString /* domain */, int> m_miReferences;
};

struct CLanguageScope {
    explicit CLanguageScope(const CString& sLanguage);
    ~CLanguageScope();
};

struct CTranslationDomainRefHolder {
    explicit CTranslationDomainRefHolder(const CString& sDomain);
    ~CTranslationDomainRefHolder();

  private:
    const CString m_sDomain;
};

// This is inspired by boost::locale::message, but without boost
class CDelayedTranslation {
  public:
    CDelayedTranslation() = default;
    CDelayedTranslation(const CString& sDomain, const CString& sContext,
                        const CString& sEnglish)
        : m_sDomain(sDomain), m_sContext(sContext), m_sEnglish(sEnglish) {}
    CString Resolve() const;

  private:
    CString m_sDomain;
    CString m_sContext;
    CString m_sEnglish;
};

class COptionalTranslation {
  public:
    COptionalTranslation(const CString& sText)
        : m_bTranslating(false), m_sText(sText) {}
    COptionalTranslation(const char* s) : COptionalTranslation(CString(s)) {}
    COptionalTranslation(const CDelayedTranslation& dTranslation)
        : m_bTranslating(true), m_dTranslation(dTranslation) {}
    CString Resolve() const {
        return m_bTranslating ? m_dTranslation.Resolve() : m_sText;
    }

  private:
    bool m_bTranslating;
    // TODO switch to std::variant<CString, CDelayedTranslation> after C++17
    CString m_sText;
    CDelayedTranslation m_dTranslation;
};

// Used by everything except modules.
// CModule defines its own version of these functions.
class CCoreTranslationMixin {
  protected:
    static CString t_s(const CString& sEnglish, const CString& sContext = "");
    static CInlineFormatMessage t_f(const CString& sEnglish,
                                    const CString& sContext = "");
    static CInlineFormatMessage t_p(const CString& sEnglish,
                                    const CString& sEnglishes, int iNum,
                                    const CString& sContext = "");
    static CDelayedTranslation t_d(const CString& sEnglish,
                                   const CString& sContext = "");
};

#endif
