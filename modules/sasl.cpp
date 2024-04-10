/*
 * Copyright (C) 2004-2024 ZNC, see the NOTICE file for details.
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

#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <algorithm>

#ifdef HAVE_LIBSSL
#include <openssl/hmac.h>
#include <openssl/rand.h>

#define NONCE_LENGTH 18
#define CLIENT_KEY "Client Key"
#define SERVER_KEY "Server Key"

// EVP_MD_CTX_create() and EVP_MD_CTX_destroy() were renamed in OpenSSL 1.1.0
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#define EVP_MD_CTX_new(ctx) EVP_MD_CTX_create(ctx)
#define EVP_MD_CTX_free(ctx) EVP_MD_CTX_destroy(ctx)
#endif
#define SUPPORTED_MECHANISMS_SIZE 5
#else
#define SUPPORTED_MECHANISMS_SIZE 2
#endif

#define NV_REQUIRE_AUTH "require_auth"
#define NV_MECHANISMS "mechanisms"

#ifdef HAVE_LIBSSL
class CSASLMod;
class Scram;

class Scram {
  public:
    enum ScramStatus {
        SCRAM_ERROR = 0,
        SCRAM_IN_PROGRESS,
        SCRAM_SUCCESS
    };

    Scram(CString sMechanism, CSASLMod* pMod);
    ~Scram();
    void Authenticate(CString sInput);

  private:
    const EVP_MD* m_pDigest;
    size_t m_uDigestSize;
    CString m_sClientNonceB64;
    CString m_sClientFirstMessageBare;
    CString m_sAuthMessage;
    unsigned char* m_pSaltedPassword;
    unsigned int m_uiStep = 0;
    CSASLMod* m_pModule;

    int CreateSHA(const unsigned char* pInput, size_t uInputLen,
                  unsigned char* pOutput, unsigned int* pOutputLen);
    ScramStatus ProcessClientFirst();
    ScramStatus ProcessServerFirst(CString& sInput);
    ScramStatus ProcessServerFinal(CString& sInput);
};
#endif

class Mechanisms : public VCString {
  public:
    void SetIndex(unsigned int uiIndex) { m_uiIndex = uiIndex; }

    unsigned int GetIndex() const { return m_uiIndex; }

    bool HasNext() const { return size() > (m_uiIndex + 1); }

    void IncrementIndex() { m_uiIndex++; }

    CString GetCurrent() const { return at(m_uiIndex); }

    CString GetNext() const {
        if (HasNext()) {
            return at(m_uiIndex + 1);
        }

        return "";
    }

  private:
    unsigned int m_uiIndex = 0;
};

class CSASLMod : public CModule {
    const struct {
        const char* szName;
        CDelayedTranslation sDescription;
        bool bDefault;
    } SupportedMechanisms[SUPPORTED_MECHANISMS_SIZE] = {
#ifdef HAVE_LIBSSL
        {"SCRAM-SHA-512", t_d("Salted Challenge Response Authentication "
                              "Mechanism (SCRAM) with SHA-512"),
         true},
        {"SCRAM-SHA-256", t_d("Salted Challenge Response Authentication "
                              "Mechanism (SCRAM) with SHA-256"),
         true},
        {"SCRAM-SHA-1", t_d("Salted Challenge Response Authentication "
                            "Mechanism (SCRAM) with SHA-1"),
         true},
#endif
        {"EXTERNAL", t_d("TLS certificate, for use with the *cert module"),
         true},
        {"PLAIN", t_d("Plain text negotiation, this should work always if the "
                      "network supports SASL"),
         true}};

  public:
    MODCONSTRUCTOR(CSASLMod) {
        AddCommand("Help", t_d("search"), t_d("Generate this output"),
                   [=](const CString& sLine) { PrintHelp(sLine); });
        AddCommand("Set", t_d("[<username> [<password>]]"),
                   t_d("Set username and password for the mechanisms that need "
                       "them. Password is optional. Without parameters, "
                       "returns information about current settings."),
                   [=](const CString& sLine) { Set(sLine); });
        AddCommand("Mechanism", t_d("[mechanism[ ...]]"),
                   t_d("Set the mechanisms to be attempted (in order)"),
                   [=](const CString& sLine) { SetMechanismCommand(sLine); });
        AddCommand("RequireAuth", t_d("[yes|no]"),
                   t_d("Don't connect unless SASL authentication succeeds"),
                   [=](const CString& sLine) { RequireAuthCommand(sLine); });
        AddCommand("Verbose", "yes|no", "Set verbosity level, useful to debug",
                   [&](const CString& sLine) {
                       m_bVerbose = sLine.Token(1, true).ToBool();
                       PutModule("Verbose: " + CString(m_bVerbose));
                   });

        m_bAuthenticated = false;
#ifdef HAVE_LIBSSL
        m_pScram = nullptr;
#endif
    }
#ifdef HAVE_LIBSSL
    ~CSASLMod() override {
        delete m_pScram;
    }
#endif

    void PrintHelp(const CString& sLine) {
        HandleHelpCommand(sLine);

        CTable Mechanisms;
        Mechanisms.AddColumn(t_s("Mechanism"));
        Mechanisms.AddColumn(t_s("Description"));
        Mechanisms.SetStyle(CTable::ListStyle);

        for (const auto& it : SupportedMechanisms) {
            Mechanisms.AddRow();
            Mechanisms.SetCell(t_s("Mechanism"), it.szName);
            Mechanisms.SetCell(t_s("Description"), it.sDescription.Resolve());
        }

        PutModule("");
        PutModule(t_s("The following mechanisms are available:"));
        PutModule(Mechanisms);
    }

    void Set(const CString& sLine) {
        if (sLine.Token(1).empty()) {
            CString sUsername = GetNV("username");
            CString sPassword = GetNV("password");

            if (sUsername.empty()) {
                PutModule(t_s("Username is currently not set"));
            } else {
                PutModule(t_f("Username is currently set to '{1}'")(sUsername));
            }
            if (sPassword.empty()) {
                PutModule(t_s("Password was not supplied"));
            } else {
                PutModule(t_s("Password was supplied"));
            }
            return;
        }

        SetNV("username", sLine.Token(1));
        SetNV("password", sLine.Token(2));

        PutModule(t_f("Username has been set to [{1}]")(GetNV("username")));
        PutModule(t_f("Password has been set to [{1}]")(GetNV("password")));
    }

    void SetMechanismCommand(const CString& sLine) {
        CString sMechanisms = sLine.Token(1, true).AsUpper();

        if (!sMechanisms.empty()) {
            VCString vsMechanisms;
            sMechanisms.Split(" ", vsMechanisms);

            for (const CString& sMechanism : vsMechanisms) {
                if (!SupportsMechanism(sMechanism)) {
                    PutModule("Unsupported mechanism: " + sMechanism);
                    return;
                }
            }

            SetNV(NV_MECHANISMS, sMechanisms);
        }

        PutModule(t_f("Current mechanisms set: {1}")(GetMechanismsString()));
    }

    void RequireAuthCommand(const CString& sLine) {
        if (!sLine.Token(1).empty()) {
            SetNV(NV_REQUIRE_AUTH, sLine.Token(1));
        }

        if (GetNV(NV_REQUIRE_AUTH).ToBool()) {
            PutModule(t_s("We require SASL negotiation to connect"));
        } else {
            PutModule(t_s("We will connect even if SASL fails"));
        }
    }

    bool SupportsMechanism(const CString& sMechanism) const {
        for (const auto& it : SupportedMechanisms) {
            if (sMechanism.Equals(it.szName)) {
                return true;
            }
        }

        return false;
    }

    CString GetMechanismsString() const {
        if (GetNV(NV_MECHANISMS).empty()) {
            CString sDefaults = "";

            for (const auto& it : SupportedMechanisms) {
                if (it.bDefault) {
                    if (!sDefaults.empty()) {
                        sDefaults += " ";
                    }

                    sDefaults += it.szName;
                }
            }

            return sDefaults;
        }

        return GetNV(NV_MECHANISMS);
    }

    void CheckRequireAuth() {
        if (!m_bAuthenticated && GetNV(NV_REQUIRE_AUTH).ToBool()) {
            GetNetwork()->SetIRCConnectEnabled(false);
            PutModule(t_s("Disabling network, we require authentication."));
            PutModule(t_s("Use 'RequireAuth no' to disable."));
        }
    }

    void Authenticate(const CString& sLine) {
        if (m_Mechanisms.empty()) return;
        /* Send blank authenticate for other mechanisms (like EXTERNAL). */
        CString sAuthLine;
        if (m_Mechanisms.GetCurrent().Equals("PLAIN") && sLine.Equals("+")) {
            sAuthLine = GetNV("username") + '\0' + GetNV("username") +
                                '\0' + GetNV("password");
            sAuthLine.Base64Encode();
            SendAuthentication(sAuthLine);
        } else if (m_Mechanisms.GetCurrent().Equals("EXTERNAL") &&
                   sLine.Equals("+")) {
            SendAuthentication(sAuthLine);
        }
#ifdef HAVE_LIBSSL
        else if (m_pScram != nullptr &&
                 (m_Mechanisms.GetCurrent().Equals("SCRAM-SHA-1") ||
                  m_Mechanisms.GetCurrent().Equals("SCRAM-SHA-256") ||
                  m_Mechanisms.GetCurrent().Equals("SCRAM-SHA-512"))) {
            m_pScram->Authenticate(!sLine.Equals("+") ? sLine : "");
        }
#endif
    }

    void SendAuthentication(CString sAuthLine) {
        /* The spec requires authentication data to be sent in chunks */
        const size_t chunkSize = 400;
        for (size_t offset = 0; offset < sAuthLine.length(); offset += chunkSize) {
            size_t size = std::min(chunkSize, sAuthLine.length() - offset);
            PutIRC("AUTHENTICATE " + sAuthLine.substr(offset, size));
        }
        if (sAuthLine.length() % chunkSize == 0) {
            /* Signal end if we have a multiple of the chunk size */
            PutIRC("AUTHENTICATE +");
        }
    }

    bool OnServerCapAvailable(const CString& sCap) override {
        return sCap.Equals("sasl");
    }

    void OnServerCapResult(const CString& sCap, bool bSuccess) override {
        if (sCap.Equals("sasl")) {
            if (bSuccess) {
                GetMechanismsString().Split(" ", m_Mechanisms);

                if (m_Mechanisms.empty()) {
                    CheckRequireAuth();
                    return;
                }

                GetNetwork()->GetIRCSock()->PauseCap();

                m_Mechanisms.SetIndex(0);
#ifdef HAVE_LIBSSL
                if (m_Mechanisms.GetCurrent().Equals("SCRAM-SHA-1") ||
                    m_Mechanisms.GetCurrent().Equals("SCRAM-SHA-256") ||
                    m_Mechanisms.GetCurrent().Equals("SCRAM-SHA-512")) {
                    delete m_pScram;

                    try {
                        m_pScram = new Scram(m_Mechanisms.GetCurrent(), this);
                    } catch (const CString& s) {
                        m_pScram = nullptr;
                        PutModule(
                            t_f("Could not create SCRAM session: {1}")(s));
                        OnAuthenticationFailed();
                    }
                }
#endif
                PutIRC("AUTHENTICATE " + m_Mechanisms.GetCurrent());
            } else {
                CheckRequireAuth();
            }
        }
    }

    EModRet OnRawMessage(CMessage& msg) override {
        if (msg.GetCommand().Equals("AUTHENTICATE")) {
            Authenticate(msg.GetParam(0));
            return HALT;
        }
        return CONTINUE;
    }

    void OnAuthenticationFailed() {
        DEBUG("sasl: Mechanism [" << m_Mechanisms.GetCurrent()
                                  << "] failed.");
        if (m_bVerbose) {
            PutModule(
                t_f("{1} mechanism failed.")(m_Mechanisms.GetCurrent()));
        }

        if (m_Mechanisms.HasNext()) {
            m_Mechanisms.IncrementIndex();
            PutIRC("AUTHENTICATE " + m_Mechanisms.GetCurrent());
        } else {
            CheckRequireAuth();
#ifdef HAVE_LIBSSL
            delete m_pScram;
            m_pScram = nullptr;
#endif
            GetNetwork()->GetIRCSock()->ResumeCap();
        }
    }

    EModRet OnNumericMessage(CNumericMessage& msg) override {
        if (m_Mechanisms.empty()) return CONTINUE;
        if (msg.GetCode() == 903) {
            /* SASL success! */
            if (m_bVerbose) {
                PutModule(
                    t_f("{1} mechanism succeeded.")(m_Mechanisms.GetCurrent()));
            }
            GetNetwork()->GetIRCSock()->ResumeCap();
            m_bAuthenticated = true;
            DEBUG("sasl: Authenticated with mechanism ["
                  << m_Mechanisms.GetCurrent() << "]");
#ifdef HAVE_LIBSSL
            delete m_pScram;
            m_pScram = nullptr;
#endif
        } else if (msg.GetCode() == 904 ||
                   msg.GetCode() == 905) {
            OnAuthenticationFailed();
        } else if (msg.GetCode() == 906) {
            /* CAP wasn't paused? */
            DEBUG("sasl: Reached 906.");
            CheckRequireAuth();
#ifdef HAVE_LIBSSL
            delete m_pScram;
            m_pScram = nullptr;
#endif
        } else if (msg.GetCode() == 907) {
            m_bAuthenticated = true;
            GetNetwork()->GetIRCSock()->ResumeCap();
            DEBUG("sasl: Received 907 -- We are already registered");
#ifdef HAVE_LIBSSL
            delete m_pScram;
            m_pScram = nullptr;
#endif
        } else if (msg.GetCode() == 908) {
            return HALT;
        } else {
            return CONTINUE;
        }
        return HALT;
    }

    void OnIRCConnected() override {
        /* Just incase something slipped through, perhaps the server doesn't
         * respond to our CAP negotiation. */

        CheckRequireAuth();
#ifdef HAVE_LIBSSL
        delete m_pScram;
        m_pScram = nullptr;
#endif
    }

    void OnIRCDisconnected() override { m_bAuthenticated = false; }

    CString GetWebMenuTitle() override { return t_s("SASL"); }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        if (sPageName != "index") {
            // only accept requests to index
            return false;
        }

        if (WebSock.IsPost()) {
            SetNV("username", WebSock.GetParam("username"));
            CString sPassword = WebSock.GetParam("password");
            if (!sPassword.empty()) {
                SetNV("password", sPassword);
            }
            SetNV(NV_REQUIRE_AUTH, WebSock.GetParam("require_auth"));
            SetNV(NV_MECHANISMS, WebSock.GetParam("mechanisms"));
        }

        Tmpl["Username"] = GetNV("username");
        Tmpl["Password"] = GetNV("password");
        Tmpl["RequireAuth"] = GetNV(NV_REQUIRE_AUTH);
        Tmpl["Mechanisms"] = GetMechanismsString();

        for (const auto& it : SupportedMechanisms) {
            CTemplate& Row = Tmpl.AddRow("MechanismLoop");
            CString sName(it.szName);
            Row["Name"] = sName;
            Row["Description"] = it.sDescription.Resolve();
        }

        return true;
    }

  private:
    Mechanisms m_Mechanisms;
#ifdef HAVE_LIBSSL
    Scram* m_pScram;
#endif
    bool m_bAuthenticated;
    bool m_bVerbose = false;
};

#ifdef HAVE_LIBSSL
Scram::Scram(CString sMechanism, CSASLMod* pMod) {
    const EVP_MD* pDigest;
    CString pDigestName;
    m_pModule = pMod;
    m_pSaltedPassword = nullptr;

    if (sMechanism.Equals("SCRAM-SHA-1")) {
        pDigestName = "SHA1";
    } else if (sMechanism.Equals("SCRAM-SHA-256")) {
        pDigestName = "SHA256";
    } else if (sMechanism.Equals("SCRAM-SHA-512")) {
        pDigestName = "SHA512";
    }

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    OpenSSL_add_all_algorithms();
#endif
    pDigest = EVP_get_digestbyname(pDigestName.c_str());

    if (pDigest == nullptr) {
        throw m_pModule->t_f("Unknown message digest: {1}")(pDigestName);
    }

    m_pDigest = pDigest;
    m_uDigestSize = EVP_MD_size(pDigest);
}

Scram::~Scram() {
    if (m_pSaltedPassword != nullptr) {
        free(m_pSaltedPassword);
    }
}

int Scram::CreateSHA(const unsigned char* pInput, size_t uInputLen,
                     unsigned char* pOutput, unsigned int* pOutputLen) {
    EVP_MD_CTX* pMdCtx = EVP_MD_CTX_new();

    if (!EVP_DigestInit_ex(pMdCtx, m_pDigest, NULL)) {
        m_pModule->PutModule("Message digest initialization failed");
        EVP_MD_CTX_free(pMdCtx);
        return SCRAM_ERROR;
    }

    if (!EVP_DigestUpdate(pMdCtx, pInput, uInputLen)) {
        m_pModule->PutModule("Message digest update failed");
        EVP_MD_CTX_free(pMdCtx);
        return SCRAM_ERROR;
    }

    if (!EVP_DigestFinal_ex(pMdCtx, pOutput, pOutputLen)) {
        m_pModule->PutModule("Message digest finalization failed");
        EVP_MD_CTX_free(pMdCtx);
        return SCRAM_ERROR;
    }

    EVP_MD_CTX_free(pMdCtx);
    return SCRAM_IN_PROGRESS;
}

Scram::ScramStatus Scram::ProcessClientFirst() {
    CString sOutput;
    m_sClientNonceB64 = CString::RandomString(NONCE_LENGTH);
    m_sClientNonceB64.Base64Encode();
    sOutput =
        "n,,n=" + m_pModule->GetNV("username") + ",r=" + m_sClientNonceB64;
    m_sClientFirstMessageBare = sOutput.substr(3);
    sOutput.Base64Encode();
    m_pModule->SendAuthentication(sOutput);
    m_uiStep++;
    return SCRAM_IN_PROGRESS;
}

Scram::ScramStatus Scram::ProcessServerFirst(CString& sInput) {
    CString sClientFinalMessageWithoutProof, sServerNonceB64, sSalt,
        sClientProofB64;
    VCString vsParams;
    unsigned char *pClientKey, sStoredKey[EVP_MAX_MD_SIZE], *pClientSignature,
        *pClientProof;
    unsigned int uiIndex, uiIterCount = 0, uiClientKeyLen, uiStoredKeyLen;
    unsigned long ulSaltLen;
    size_t uClientNonceLen;
    CString sPassword = m_pModule->GetNV("password");
    CString sOutput;

    sInput.Split(",", vsParams, false);

    if (vsParams.size() < 3) {
        m_pModule->PutModule(
            m_pModule->t_f("Invalid server-first-message: {1}")(sInput));
        return SCRAM_ERROR;
    }

    for (const CString& sParam : vsParams) {
        if (!strncmp(sParam.c_str(), "r=", 2)) {
            sServerNonceB64 = sParam.substr(2);
        } else if (!strncmp(sParam.c_str(), "s=", 2)) {
            sSalt = sParam.substr(2);
        } else if (!strncmp(sParam.c_str(), "i=", 2)) {
            uiIterCount = strtoul(sParam.substr(2).c_str(), NULL, 10);
        }
    }

    if (sServerNonceB64.empty() || sSalt.empty() || uiIterCount == 0) {
        m_pModule->PutModule(
            m_pModule->t_f("Invalid server-first-message: {1}")(sInput));
        return SCRAM_ERROR;
    }
    uClientNonceLen = m_sClientNonceB64.length();

    // The server can append his nonce to the client's nonce
    if (sServerNonceB64.length() < uClientNonceLen ||
        strncmp(sServerNonceB64.c_str(), m_sClientNonceB64.c_str(),
                uClientNonceLen)) {
        m_pModule->PutModule(
            m_pModule->t_f("Invalid server nonce: {1}")(sServerNonceB64));
        return SCRAM_ERROR;
    }
    ulSaltLen = sSalt.Base64Decode();

    // SaltedPassword := Hi(Normalize(password), sSalt, i)
    m_pSaltedPassword = static_cast<unsigned char*>(malloc(m_uDigestSize));
    PKCS5_PBKDF2_HMAC(sPassword.c_str(), sPassword.length(),
                      (unsigned char*)sSalt.c_str(), ulSaltLen, uiIterCount,
                      m_pDigest, m_uDigestSize, m_pSaltedPassword);

    // AuthMessage := client-first-message-bare + "," +
    //                server-first-message + "," +
    //                client-final-message-without-proof
    sClientFinalMessageWithoutProof = "c=biws,r=" + sServerNonceB64;

    m_sAuthMessage = m_sClientFirstMessageBare + "," + sInput + "," +
                     sClientFinalMessageWithoutProof;

    // ClientKey := HMAC(SaltedPassword, "Client Key")
    pClientKey = static_cast<unsigned char*>(malloc(m_uDigestSize));
    HMAC(m_pDigest, m_pSaltedPassword, m_uDigestSize,
         (unsigned char*)CLIENT_KEY, strlen(CLIENT_KEY), pClientKey,
         &uiClientKeyLen);

    // StoredKey := H(ClientKey)
    if (!CreateSHA(pClientKey, m_uDigestSize, sStoredKey, &uiStoredKeyLen)) {
        free(pClientKey);
        return SCRAM_ERROR;
    }

    // ClientSignature := HMAC(StoredKey, AuthMessage)
    pClientSignature = static_cast<unsigned char*>(malloc(m_uDigestSize));
    memset(pClientSignature, 0, m_uDigestSize);
    HMAC(m_pDigest, sStoredKey, uiStoredKeyLen,
         (unsigned char*)m_sAuthMessage.c_str(), m_sAuthMessage.length(),
         pClientSignature, NULL);

    // ClientProof := ClientKey XOR ClientSignature
    pClientProof = static_cast<unsigned char*>(malloc(uiClientKeyLen));
    memset(pClientProof, 0, uiClientKeyLen);
    for (uiIndex = 0; uiIndex < uiClientKeyLen; uiIndex++) {
        pClientProof[uiIndex] = pClientKey[uiIndex] ^ pClientSignature[uiIndex];
    }
    sClientProofB64 = CString((const char*)pClientProof, uiClientKeyLen);
    sClientProofB64.Base64Encode();

    sOutput = sClientFinalMessageWithoutProof + ",p=" + sClientProofB64;
    sOutput.Base64Encode();
    m_pModule->SendAuthentication(sOutput);

    free(pClientKey);
    free(pClientSignature);
    free(pClientProof);
    m_uiStep++;
    return SCRAM_IN_PROGRESS;
}

Scram::ScramStatus Scram::ProcessServerFinal(CString& sInput) {
    CString sVerifier;
    unsigned char *pServerKey, *pServerSignature;
    unsigned int uiServerKeyLen = 0, uiServerSignatureLen = 0;
    unsigned long ulVerifierLen;

    if (sInput.length() < 3 || (sInput[0] != 'v' && sInput[1] != '=')) {
        return SCRAM_ERROR;
    }

    sVerifier = sInput.substr(2);
    ulVerifierLen = sVerifier.Base64Decode();

    // ServerKey := HMAC(SaltedPassword, "Server Key")
    pServerKey = static_cast<unsigned char*>(malloc(m_uDigestSize));
    HMAC(m_pDigest, m_pSaltedPassword, m_uDigestSize,
         (unsigned char*)SERVER_KEY, strlen(SERVER_KEY), pServerKey,
         &uiServerKeyLen);

    // ServerSignature := HMAC(ServerKey, AuthMessage)
    pServerSignature = static_cast<unsigned char*>(malloc(m_uDigestSize));
    HMAC(m_pDigest, pServerKey, m_uDigestSize,
         (unsigned char*)m_sAuthMessage.c_str(), m_sAuthMessage.length(),
         pServerSignature, &uiServerSignatureLen);

    if (ulVerifierLen == uiServerSignatureLen &&
        std::memcmp(sVerifier.c_str(), pServerSignature, ulVerifierLen) == 0) {
        free(pServerKey);
        free(pServerSignature);
        return SCRAM_SUCCESS;
    } else {
        free(pServerKey);
        free(pServerSignature);
        return SCRAM_ERROR;
    }
}

void Scram::Authenticate(CString sInput) {
    ScramStatus status;

    if (sInput != "+") {
        sInput.Base64Decode();
    }

    switch (m_uiStep) {
        case 0:
            status = ProcessClientFirst();
            break;
        case 1:
            status = ProcessServerFirst(sInput);
            break;
        case 2:
            status = ProcessServerFinal(sInput);
            break;
        default:
            status = SCRAM_ERROR;
            break;
    }

    if (status == Scram::SCRAM_SUCCESS) {
        DEBUG("sasl: SCRAM authentication succeeded");
        m_pModule->SendAuthentication("+");
    } else if (status == Scram::SCRAM_ERROR) {
        DEBUG("sasl: SCRAM authentication failed");
        m_pModule->CheckRequireAuth();
        m_pModule->GetNetwork()->GetIRCSock()->ResumeCap();
    }
}
#endif

template <>
void TModInfo<CSASLMod>(CModInfo& Info) {
    Info.SetWikiPage("sasl");
}

NETWORKMODULEDEFS(CSASLMod, t_s("Adds support for sasl authentication "
                                "capability to authenticate to an IRC server"))
