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

#define REQUIRESSL

#include <znc/User.h>
#include <znc/znc.h>

using std::map;
using std::vector;
using std::set;
using std::pair;

class CSSLClientCertMod : public CModule {
  public:
    MODCONSTRUCTOR(CSSLClientCertMod) {
        AddHelpCommand();
        AddCommand("Add", t_d("[pubkey]"),
                   t_d("Add a public key. If key is not provided "
                       "will use the current key"),
                   [=](const CString& sLine) { HandleAddCommand(sLine); });
        AddCommand("Del", t_d("id"), t_d("Delete a key by its number in List"),
                   [=](const CString& sLine) { HandleDelCommand(sLine); });
        AddCommand("List", "", t_d("List your public keys"),
                   [=](const CString& sLine) { HandleListCommand(sLine); });
        AddCommand("Show", "", t_d("Print your current key"),
                   [=](const CString& sLine) { HandleShowCommand(sLine); });
    }

    ~CSSLClientCertMod() override {}

    bool OnBoot() override {
        const vector<CListener*>& vListeners = CZNC::Get().GetListeners();

        // We need the SSL_VERIFY_PEER flag on all listeners, or else
        // the client doesn't send a ssl cert
        for (CListener* pListener : vListeners)
            pListener->GetRealListener()->SetRequireClientCertFlags(
                SSL_VERIFY_PEER);

        for (MCString::const_iterator it = BeginNV(); it != EndNV(); ++it) {
            VCString vsKeys;
            const CString& sUser = it->first;

            if (CZNC::Get().FindUser(sUser) == nullptr) {
                DEBUG("Unknown user in saved data [" + sUser + "]");
                continue;
            }

            it->second.Split(" ", vsKeys, false);
            for (CString& sKey : vsKeys) {
                sKey.MakeLower();
                m_PubKeys[sUser].insert(sKey);
                m_KeyToUser[sKey].insert(sUser);
            }
        }

        return true;
    }

    void OnPostRehash() override { OnBoot(); }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        OnBoot();

        return true;
    }

    bool Save() {
        ClearNV(false);
        for (const auto& it : m_PubKeys) {
            CString sVal;
            for (const CString& sKey : it.second) {
                sVal += sKey + " ";
            }

            if (!sVal.empty()) SetNV(it.first, sVal, false);
        }

        return SaveRegistry();
    }

    bool AddKey(CUser& User, CString sKey) {
        sKey.MakeLower();
        const pair<SCString::const_iterator, bool> pair =
            m_PubKeys[User.GetUsername()].insert(sKey);

        if (pair.second) {
            Save();
            m_KeyToUser[sKey].insert(User.GetUsername());
        }

        return pair.second;
    }

    void DelKey(CUser& User, CString sKey) {
        sKey.MakeLower();
        MSCString::iterator it = m_PubKeys.find(User.GetUsername());
        if (it != m_PubKeys.end()) {
            if (it->second.erase(sKey)) {
                if (it->second.size() == 0) {
                    m_PubKeys.erase(it);
                }

                it = m_KeyToUser.find(sKey);
                if (it != m_KeyToUser.end()) {
                    it->second.erase(User.GetUsername());
                    if (it->second.empty()) m_KeyToUser.erase(it);
                }

                Save();
            }
        }
    }

    EModRet OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) override {
        const CString sUser = Auth->GetUsername();
        CZNCSock* pSock = dynamic_cast<CZNCSock*>(Auth->GetSocket());
        CUser* pUser = CZNC::Get().FindUser(sUser);

        if (pSock == nullptr || pUser == nullptr) return CONTINUE;

        const MultiKey PubKey = GetKey(pSock);
        DEBUG("User: " << sUser << " Key: " << PubKey.ToString());

        if (PubKey.sSHA1.empty()) {
            DEBUG("Peer got no public key, ignoring");
            return CONTINUE;
        }

        MSCString::const_iterator it = m_PubKeys.find(sUser);
        if (it == m_PubKeys.end()) {
            DEBUG("No saved pubkeys for this user");
            return CONTINUE;
        }

        SCString::const_iterator it2 = it->second.find(PubKey.sSHA1);
        if (it2 != it->second.end()) {
            DEBUG("Found SHA-1 pubkey, replacing with SHA-256");
            AddKey(*pUser, PubKey.sSHA256);
            DelKey(*pUser, PubKey.sSHA1);
            Auth->AcceptLogin(*pUser);
            return HALT;
        }

        it2 = it->second.find(PubKey.sSHA256);
        if (it2 == it->second.end()) {
            DEBUG("Invalid pubkey");
            return CONTINUE;
        }

        // This client uses a valid pubkey for this user, let them in
        DEBUG("Accepted pubkey auth");
        Auth->AcceptLogin(*pUser);

        return HALT;
    }

    void OnClientGetSASLMechanisms(SCString& ssMechanisms) override {
        ssMechanisms.insert("EXTERNAL");
    }

    EModRet OnClientSASLAuthenticate(const CString& sMechanism,
                                     const CString& sMessage) override {
        if (sMechanism != "EXTERNAL") {
            return CONTINUE;
        }
        CString sUser = GetClient()->ParseUser(sMessage);
        const MultiKey Key = GetKey(GetClient());
        DEBUG("Key: " << Key.ToString());

        if (Key.sSHA1.empty()) {
            GetClient()->RefuseSASLLogin("No client cert presented");
            return HALT;
        }

        auto it = m_KeyToUser.find(Key.sSHA1);
        if (it != m_KeyToUser.end()) {
            DEBUG("Found SHA-1 pubkey for SASL, replacing with SHA-256");
            SCString ssUsers = it->second;
            for (const CString& sUser2 : ssUsers) {
                CUser* pUser = CZNC::Get().FindUser(sUser2);
                if (pUser) {
                    AddKey(*pUser, Key.sSHA256);
                    DelKey(*pUser, Key.sSHA1);
                }
            }
        }

        it = m_KeyToUser.find(Key.sSHA256);
        if (it == m_KeyToUser.end()) {
            GetClient()->RefuseSASLLogin("Client cert not recognized");
            return HALT;
        }

        const SCString& ssUsers = it->second;

        if (ssUsers.empty()) {
            GetClient()->RefuseSASLLogin("Key found, but list of users is empty, please report bug");
            return HALT;
        }

        if (sUser.empty()) {
            sUser = *ssUsers.begin();
        } else if (ssUsers.count(sUser) == 0) {
            GetClient()->RefuseSASLLogin(
                "The specified user doesn't have this key");
            return HALT;
        }

        CUser* pUser = CZNC::Get().FindUser(sUser);
        if (!pUser) {
            GetClient()->RefuseSASLLogin("User not found");
            return HALT;
        }

        DEBUG("Accepted cert auth for " << sUser);
        GetClient()->AcceptSASLLogin(*pUser);
        return HALT;
    }

    void HandleShowCommand(const CString& sLine) {
        const CString sPubKey = GetKey(GetClient()).sSHA256;

        if (sPubKey.empty()) {
            PutModule(t_s("You are not connected with any valid public key"));
        } else {
            PutModule(t_f("Your current public key is: {1}")(sPubKey));
        }
    }

    void HandleAddCommand(const CString& sLine) {
        CString sPubKey = sLine.Token(1);

        if (sPubKey.empty()) {
            sPubKey = GetKey(GetClient()).sSHA256;
        }

        if (sPubKey.empty()) {
            PutModule(
                t_s("You did not supply a public key or connect with one."));
        } else {
            if (AddKey(*GetUser(), sPubKey)) {
                PutModule(t_f("Key '{1}' added.")(sPubKey));
            } else {
                PutModule(t_f("The key '{1}' is already added.")(sPubKey));
            }
        }
    }

    void HandleListCommand(const CString& sLine) {
        CTable Table;

        Table.AddColumn(t_s("Id", "list"));
        Table.AddColumn(t_s("Key", "list"));
        Table.SetStyle(CTable::ListStyle);

        MSCString::const_iterator it = m_PubKeys.find(GetUser()->GetUsername());
        if (it == m_PubKeys.end()) {
            PutModule(t_s("No keys set for your user"));
            return;
        }

        unsigned int id = 1;
        for (const CString& sKey : it->second) {
            Table.AddRow();
            Table.SetCell(t_s("Id", "list"), CString(id++));
            Table.SetCell(t_s("Key", "list"), sKey);
        }

        if (PutModule(Table) == 0) {
            // This double check is necessary, because the
            // set could be empty.
            PutModule(t_s("No keys set for your user"));
        }
    }

    void HandleDelCommand(const CString& sLine) {
        unsigned int id = sLine.Token(1, true).ToUInt();
        MSCString::iterator it = m_PubKeys.find(GetUser()->GetUsername());

        if (it == m_PubKeys.end()) {
            PutModule(t_s("No keys set for your user"));
            return;
        }

        if (id == 0 || id > it->second.size()) {
            PutModule(t_s("Invalid #, check \"list\""));
            return;
        }

        SCString::const_iterator it2 = it->second.begin();
        while (id > 1) {
            ++it2;
            id--;
        }

        CString sKey = *it2;
        it->second.erase(it2);
        if (it->second.size() == 0) m_PubKeys.erase(it);

        it = m_KeyToUser.find(sKey);
        if (it != m_KeyToUser.end()) {
            it->second.erase(GetUser()->GetUsername());
            if (it->second.empty()) m_KeyToUser.erase(it);
        }

        PutModule(t_s("Removed"));

        Save();
    }

    // Struct to allow transparent migration from SHA-1 to SHA-256
    struct MultiKey {
        CString sSHA1;
        CString sSHA256;

        CString ToString() const {
            return "sha-1: " + sSHA1 + ", sha-256: " + sSHA256;
        }
    };
    MultiKey GetKey(CZNCSock* pSock) {
        MultiKey Res;
        long int res = pSock->GetPeerFingerprint(Res.sSHA1);
        X509* pCert = pSock->GetX509();
        Res.sSHA256 = pSock->GetSSLPeerFingerprint(pCert);
        X509_free(pCert);

        DEBUG("GetKey() returned status " << res << " with key " << Res.ToString());

        // This is 'inspired' by charybdis' libratbox
        switch (res) {
            case X509_V_OK:
            case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
            case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
            case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
                return Res;
            default:
                return {};
        }
    }

    CString GetWebMenuTitle() override { return "certauth"; }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        CUser* pUser = WebSock.GetSession()->GetUser();
        if (!pUser) return false;

        if (sPageName == "index") {
            MSCString::const_iterator it = m_PubKeys.find(pUser->GetUsername());
            if (it != m_PubKeys.end()) {
                for (const CString& sKey : it->second) {
                    CTemplate& row = Tmpl.AddRow("KeyLoop");
                    row["Key"] = sKey;
                }
            }

            return true;
        } else if (sPageName == "add") {
            AddKey(*pUser, WebSock.GetParam("key"));
            WebSock.Redirect(GetWebPath());
            return true;
        } else if (sPageName == "delete") {
            DelKey(*pUser, WebSock.GetParam("key", false));
            WebSock.Redirect(GetWebPath());
            return true;
        }

        return false;
    }

  private:
    // Maps user names to a list of allowed pubkeys
    typedef map<CString, set<CString>> MSCString;
    MSCString m_PubKeys;
    MSCString m_KeyToUser;
};

template <>
void TModInfo<CSSLClientCertMod>(CModInfo& Info) {
    Info.SetWikiPage("certauth");
}

GLOBALMODULEDEFS(
    CSSLClientCertMod,
    t_s("Allows users to authenticate via SSL client certificates."))
