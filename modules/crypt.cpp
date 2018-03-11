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

//! @author prozac@rottenboy.com
//
// The encryption here was designed to be compatible with mircryption's CBC
// mode.
//
// Latest tested against:
// MircryptionSuite - Mircryption ver 1.19.01 (dll v1.15.01 , mirc v7.32) CBC
// loaded and ready.
//
// TODO:
//
// 1) Encrypt key storage file
// 2) Some way of notifying the user that the current channel is in "encryption
// mode" verses plain text
// 3) Temporarily disable a target (nick/chan)
//
// NOTE: This module is currently NOT intended to secure you from your shell
// admin.
//       The keys are currently stored in plain text, so anyone with access to
//       your account (or root) can obtain them.
//       It is strongly suggested that you enable SSL between ZNC and your
//       client otherwise the encryption stops at ZNC and gets sent to your
//       client in plain text.
//

#include <openssl/bn.h>
#include <openssl/dh.h>
#include <znc/Chan.h>
#include <znc/IRCNetwork.h>
#include <znc/SHA256.h>
#include <znc/User.h>

#define REQUIRESSL 1
// To be removed in future versions
#define NICK_PREFIX_OLD_KEY "[nick-prefix]"
#define NICK_PREFIX_KEY "@nick-prefix@"

class CCryptMod : public CModule {
  private:
    /*
     * As used in other implementations like KVIrc, fish10, Quassel, FiSH-irssi,
     * ... all the way back to the original located at
     * http://mircryption.sourceforge.net/Extras/McpsFishDH.zip
     */
    static constexpr const char* m_sPrime1080 =
        "FBE1022E23D213E8ACFA9AE8B9DFADA3EA6B7AC7A7B7E95AB5EB2DF858921FEADE95E6"
        "AC7BE7DE6ADBAB8A783E7AF7A7FA6A2B7BEB1E72EAE2B72F9FA2BFB2A2EFBEFAC868BA"
        "DB3E828FA8BADFADA3E4CC1BE7E8AFE85E9698A783EB68FA07A77AB6AD7BEB618ACF9C"
        "A2897EB28A6189EFA07AB99A8A7FA9AE299EFA7BA66DEAFEFBEFBF0B7D8B";
    /* Generate our keys once and reuse, just like ssh keys */
    std::unique_ptr<DH, decltype(&DH_free)> m_pDH;
    CString m_sPrivKey;
    CString m_sPubKey;

#if OPENSSL_VERSION_NUMBER < 0X10100000L || defined(LIBRESSL_VERSION_NUMBER)
    static int DH_set0_pqg(DH* dh, BIGNUM* p, BIGNUM* q, BIGNUM* g) {
        /* If the fields p and g in dh are nullptr, the corresponding input
         * parameters MUST be non-nullptr.  q may remain nullptr.
         */
        if (dh == nullptr || (dh->p == nullptr && p == nullptr) ||
            (dh->g == nullptr && g == nullptr))
            return 0;

        if (p != nullptr) {
            BN_free(dh->p);
            dh->p = p;
        }
        if (g != nullptr) {
            BN_free(dh->g);
            dh->g = g;
        }
        if (q != nullptr) {
            BN_free(dh->q);
            dh->q = q;
            dh->length = BN_num_bits(q);
        }

        return 1;
    }

    static void DH_get0_key(const DH* dh, const BIGNUM** pub_key,
                            const BIGNUM** priv_key) {
        if (dh != nullptr) {
            if (pub_key != nullptr) *pub_key = dh->pub_key;
            if (priv_key != nullptr) *priv_key = dh->priv_key;
        }
    }

#endif

    bool DH1080_gen() {
        /* Generate our keys on first call */
        if (m_sPrivKey.empty() || m_sPubKey.empty()) {
            int len;
            const BIGNUM* bPrivKey = nullptr;
            const BIGNUM* bPubKey = nullptr;
            BIGNUM* bPrime = nullptr;
            BIGNUM* bGen = nullptr;

            if (!BN_hex2bn(&bPrime, m_sPrime1080) || !BN_dec2bn(&bGen, "2") ||
                !DH_set0_pqg(m_pDH.get(), bPrime, nullptr, bGen) ||
                !DH_generate_key(m_pDH.get())) {
                /* one of them failed */
                if (bPrime != nullptr) BN_clear_free(bPrime);
                if (bGen != nullptr) BN_clear_free(bGen);
                return false;
            }

            /* Get our keys */
            DH_get0_key(m_pDH.get(), &bPubKey, &bPrivKey);

            /* Get our private key */
            len = BN_num_bytes(bPrivKey);
            m_sPrivKey.resize(len);
            BN_bn2bin(bPrivKey, (unsigned char*)m_sPrivKey.data());
            m_sPrivKey.Base64Encode();

            /* Get our public key */
            len = BN_num_bytes(bPubKey);
            m_sPubKey.resize(len);
            BN_bn2bin(bPubKey, (unsigned char*)m_sPubKey.data());
            m_sPubKey.Base64Encode();
        }

        return true;
    }

    bool DH1080_comp(CString& sOtherPubKey, CString& sSecretKey) {
        long len;
        unsigned char* key = nullptr;
        BIGNUM* bOtherPubKey = nullptr;

        /* Prepare other public key */
        len = sOtherPubKey.Base64Decode();
        bOtherPubKey =
            BN_bin2bn((unsigned char*)sOtherPubKey.data(), len, nullptr);

        /* Generate secret key */
        key = (unsigned char*)calloc(DH_size(m_pDH.get()), 1);
        if ((len = DH_compute_key(key, bOtherPubKey, m_pDH.get())) == -1) {
            sSecretKey = "";
            if (bOtherPubKey != nullptr) BN_clear_free(bOtherPubKey);
            if (key != nullptr) free(key);
            return false;
        }

        /* Get our secret key */
        sSecretKey.resize(SHA256_DIGEST_SIZE);
        sha256(key, len, (unsigned char*)sSecretKey.data());
        sSecretKey.Base64Encode();
        sSecretKey.TrimRight("=");

        if (bOtherPubKey != nullptr) BN_clear_free(bOtherPubKey);
        if (key != nullptr) free(key);

        return true;
    }

    CString NickPrefix() {
        MCString::iterator it = FindNV(NICK_PREFIX_KEY);
        /*
         * Check for different Prefixes to not confuse modules with nicknames
         * Also check for overlap for rare cases like:
         * SP = "*"; NP = "*s"; "tatus" sends an encrypted message appearing at
         * "*status"
         */
        CString sStatusPrefix = GetUser()->GetStatusPrefix();
        if (it != EndNV()) {
            size_t sp = sStatusPrefix.size();
            size_t np = it->second.size();
            int min = std::min(sp, np);
            if (min == 0 || sStatusPrefix.CaseCmp(it->second, min) != 0)
                return it->second;
        }
        return sStatusPrefix.StartsWith("*") ? "." : "*";
    }

  public:
    /* MODCONSTRUCTOR(CLASS) is of form "CLASS(...) : CModule(...)" */
    MODCONSTRUCTOR(CCryptMod), m_pDH(DH_new(), DH_free) {
        AddHelpCommand();
        AddCommand("DelKey", t_d("<#chan|Nick>"),
                   t_d("Remove a key for nick or channel"),
                   [=](const CString& sLine) { OnDelKeyCommand(sLine); });
        AddCommand("SetKey", t_d("<#chan|Nick> <Key>"),
                   t_d("Set a key for nick or channel"),
                   [=](const CString& sLine) { OnSetKeyCommand(sLine); });
        AddCommand("ListKeys", "", t_d("List all keys"),
                   [=](const CString& sLine) { OnListKeysCommand(sLine); });
        AddCommand("KeyX", t_d("<Nick>"),
                   t_d("Start a DH1080 key exchange with nick"),
                   [=](const CString& sLine) { OnKeyXCommand(sLine); });
        AddCommand(
            "GetNickPrefix", "", t_d("Get the nick prefix"),
            [=](const CString& sLine) { OnGetNickPrefixCommand(sLine); });
        AddCommand(
            "SetNickPrefix", t_d("[Prefix]"),
            t_d("Set the nick prefix, with no argument it's disabled."),
            [=](const CString& sLine) { OnSetNickPrefixCommand(sLine); });
    }

    bool OnLoad(const CString& sArgsi, CString& sMessage) override {
        MCString::iterator it = FindNV(NICK_PREFIX_KEY);
        if (it == EndNV()) {
            /* Don't have the new prefix key yet */
            it = FindNV(NICK_PREFIX_OLD_KEY);
            if (it != EndNV()) {
                SetNV(NICK_PREFIX_KEY, it->second);
                DelNV(NICK_PREFIX_OLD_KEY);
            }
        }
        return true;
    }

    EModRet OnUserTextMessage(CTextMessage& Message) override {
        FilterOutgoing(Message);
        return CONTINUE;
    }

    EModRet OnUserNoticeMessage(CNoticeMessage& Message) override {
        FilterOutgoing(Message);
        return CONTINUE;
    }

    EModRet OnUserActionMessage(CActionMessage& Message) override {
        FilterOutgoing(Message);
        return CONTINUE;
    }

    EModRet OnUserTopicMessage(CTopicMessage& Message) override {
        FilterOutgoing(Message);
        return CONTINUE;
    }

    EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
        FilterIncoming(Nick.GetNick(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnPrivNotice(CNick& Nick, CString& sMessage) override {
        CString sCommand = sMessage.Token(0);
        CString sOtherPubKey = sMessage.Token(1);

        if ((sCommand.Equals("DH1080_INIT") ||
             sCommand.Equals("DH1080_INIT_CBC")) &&
            !sOtherPubKey.empty()) {
            CString sSecretKey;
            CString sTail = sMessage.Token(2); /* For fish10 */

            /* remove trailing A */
            if (sOtherPubKey.TrimSuffix("A") && DH1080_gen() &&
                DH1080_comp(sOtherPubKey, sSecretKey)) {
                PutModule(
                    t_f("Received DH1080 public key from {1}, sending mine...")(
                        Nick.GetNick()));
                PutIRC("NOTICE " + Nick.GetNick() + " :DH1080_FINISH " +
                       m_sPubKey + "A" + (sTail.empty() ? "" : (" " + sTail)));
                SetNV(Nick.GetNick().AsLower(), sSecretKey);
                PutModule(t_f("Key for {1} successfully set.")(Nick.GetNick()));
                return HALT;
            }
            PutModule(t_f("Error in {1} with {2}: {3}")(
                sCommand, Nick.GetNick(),
                (sSecretKey.empty() ? t_s("no secret key computed")
                                    : sSecretKey)));
            return CONTINUE;

        } else if (sCommand.Equals("DH1080_FINISH") && !sOtherPubKey.empty()) {
            /*
             * In theory we could get a DH1080_FINISH without us having sent a
             * DH1080_INIT first, but then to have any use for the other user,
             * they'd already have our pub key
             */
            CString sSecretKey;

            /* remove trailing A */
            if (sOtherPubKey.TrimSuffix("A") && DH1080_gen() &&
                DH1080_comp(sOtherPubKey, sSecretKey)) {
                SetNV(Nick.GetNick().AsLower(), sSecretKey);
                PutModule(t_f("Key for {1} successfully set.")(Nick.GetNick()));
                return HALT;
            }
            PutModule(t_f("Error in {1} with {2}: {3}")(
                sCommand, Nick.GetNick(),
                (sSecretKey.empty() ? t_s("no secret key computed")
                                    : sSecretKey)));
            return CONTINUE;
        }

        FilterIncoming(Nick.GetNick(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnPrivAction(CNick& Nick, CString& sMessage) override {
        FilterIncoming(Nick.GetNick(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
        FilterIncoming(Channel.GetName(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnChanNotice(CNick& Nick, CChan& Channel,
                         CString& sMessage) override {
        FilterIncoming(Channel.GetName(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnChanAction(CNick& Nick, CChan& Channel,
                         CString& sMessage) override {
        FilterIncoming(Channel.GetName(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sMessage) override {
        FilterIncoming(Channel.GetName(), Nick, sMessage);
        return CONTINUE;
    }

    EModRet OnNumericMessage(CNumericMessage& Message) override {
        if (Message.GetCode() != 332) {
            return CONTINUE;
        }

        CChan* pChan = GetNetwork()->FindChan(Message.GetParam(1));
        if (pChan) {
            CNick* Nick = pChan->FindNick(Message.GetParam(0));
            CString sTopic = Message.GetParam(2);

            FilterIncoming(pChan->GetName(), *Nick, sTopic);
            Message.SetParam(2, sTopic);
        }

        return CONTINUE;
    }

    template <typename T>
    void FilterOutgoing(T& Msg) {
        CString sTarget = Msg.GetTarget();
        sTarget.TrimPrefix(NickPrefix());
        Msg.SetTarget(sTarget);

        CString sMessage = Msg.GetText();

        if (sMessage.TrimPrefix("``")) {
            return;
        }

        MCString::iterator it = FindNV(sTarget.AsLower());
        if (it != EndNV()) {
            sMessage = MakeIvec() + sMessage;
            sMessage.Encrypt(it->second);
            sMessage.Base64Encode();
            Msg.SetText("+OK *" + sMessage);
        }
    }

    void FilterIncoming(const CString& sTarget, CNick& Nick,
                        CString& sMessage) {
        if (sMessage.TrimPrefix("+OK *")) {
            MCString::iterator it = FindNV(sTarget.AsLower());

            if (it != EndNV()) {
                sMessage.Base64Decode();
                sMessage.Decrypt(it->second);
                sMessage.LeftChomp(8);
                sMessage = sMessage.c_str();
                Nick.SetNick(NickPrefix() + Nick.GetNick());
            }
        }
    }

    void OnDelKeyCommand(const CString& sCommand) {
        CString sTarget = sCommand.Token(1);

        if (!sTarget.empty()) {
            if (DelNV(sTarget.AsLower())) {
                PutModule(t_f("Target [{1}] deleted")(sTarget));
            } else {
                PutModule(t_f("Target [{1}] not found")(sTarget));
            }
        } else {
            PutModule(t_s("Usage DelKey <#chan|Nick>"));
        }
    }

    void OnSetKeyCommand(const CString& sCommand) {
        CString sTarget = sCommand.Token(1);
        CString sKey = sCommand.Token(2, true);

        // Strip "cbc:" from beginning of string incase someone pastes directly
        // from mircryption
        sKey.TrimPrefix("cbc:");

        if (!sKey.empty()) {
            SetNV(sTarget.AsLower(), sKey);
            PutModule(
                t_f("Set encryption key for [{1}] to [{2}]")(sTarget, sKey));
        } else {
            PutModule(t_s("Usage: SetKey <#chan|Nick> <Key>"));
        }
    }

    void OnKeyXCommand(const CString& sCommand) {
        CString sTarget = sCommand.Token(1);

        if (!sTarget.empty()) {
            if (DH1080_gen()) {
                PutIRC("NOTICE " + sTarget + " :DH1080_INIT " + m_sPubKey +
                       "A");
                PutModule(t_f("Sent my DH1080 public key to {1}, waiting for reply ...")(sTarget));
            } else {
                PutModule(t_s("Error generating our keys, nothing sent."));
            }
        } else {
            PutModule(t_s("Usage: KeyX <Nick>"));
        }
    }

    void OnGetNickPrefixCommand(const CString& sCommand) {
        CString sPrefix = NickPrefix();
        if (sPrefix.empty()) {
            PutModule(t_s("Nick Prefix disabled."));
        } else {
            PutModule(t_f("Nick Prefix: {1}")(sPrefix));
        }
    }

    void OnSetNickPrefixCommand(const CString& sCommand) {
        CString sPrefix = sCommand.Token(1);

        if (sPrefix.StartsWith(":")) {
            PutModule(
                t_s("You cannot use :, even followed by other symbols, as Nick "
                    "Prefix."));
        } else {
            CString sStatusPrefix = GetUser()->GetStatusPrefix();
            size_t sp = sStatusPrefix.size();
            size_t np = sPrefix.size();
            int min = std::min(sp, np);
            if (min > 0 && sStatusPrefix.CaseCmp(sPrefix, min) == 0)
                PutModule(
                    t_f("Overlap with Status Prefix ({1}), this Nick Prefix "
                        "will not be used!")(sStatusPrefix));
            else {
                SetNV(NICK_PREFIX_KEY, sPrefix);
                if (sPrefix.empty())
                    PutModule(t_s("Disabling Nick Prefix."));
                else
                    PutModule(t_f("Setting Nick Prefix to {1}")(sPrefix));
            }
        }
    }

    void OnListKeysCommand(const CString& sCommand) {
        CTable Table;
        Table.AddColumn(t_s("Target", "listkeys"));
        Table.AddColumn(t_s("Key", "listkeys"));

        for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
            if (!it->first.Equals(NICK_PREFIX_KEY)) {
                Table.AddRow();
                Table.SetCell(t_s("Target", "listkeys"), it->first);
                Table.SetCell(t_s("Key", "listkeys"), it->second);
            }
        }
        if (Table.empty())
            PutModule(t_s("You have no encryption keys set."));
        else
            PutModule(Table);
    }

    CString MakeIvec() {
        CString sRet;
        time_t t;
        time(&t);
        int r = rand();
        sRet.append((char*)&t, 4);
        sRet.append((char*)&r, 4);

        return sRet;
    }
};

template <>
void TModInfo<CCryptMod>(CModInfo& Info) {
    Info.SetWikiPage("crypt");
}

NETWORKMODULEDEFS(CCryptMod, t_s("Encryption for channel/private messages"))
