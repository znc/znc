/*
 * Copyright (C) 2004-2013 ZNC, see the NOTICE file for details.
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

#ifdef HAVE_LIBSSL
#define HAVE_SASL_MECHANISM
#endif

static const struct {
	const char *szName;
	const char *szDescription;
	const bool  bDefault;
} SupportedMechanisms[] = {
	{ "EXTERNAL",           "TLS certificate, for use with the *cert module", false },
#ifdef HAVE_SASL_MECHANISM
	{ "DH-BLOWFISH",        "Secure negotiation using the DH-BLOWFISH mechanism", true },
	{ "DH-AES",		"More secure negotiation using the DH-AES mechanism", true },
#endif
	{ "PLAIN",              "Plain text negotiation", true },
	{ NULL, NULL, false }
};

#define NV_REQUIRE_AUTH     "require_auth"
#define NV_MECHANISMS       "mechanisms"

class Mechanisms : public VCString {
public:
	void SetIndex(unsigned int uiIndex) {
		m_uiIndex = uiIndex;
	}

	unsigned int GetIndex() const {
		return m_uiIndex;
	}

	bool HasNext() const {
		return size() > (m_uiIndex + 1);
	}

	void IncrementIndex() {
		m_uiIndex++;
	}

	CString GetCurrent() const {
		return at(m_uiIndex);
	}

	CString GetNext() const {
		if (HasNext()) {
			return at(m_uiIndex + 1);
		}

		return "";
	}

private:
	unsigned int m_uiIndex;
};

#ifdef HAVE_SASL_MECHANISM
class DHCommon {
public:
	DH *dh;
	unsigned char *secret;
	int key_size;

	DHCommon() {
		dh = DH_new();
		secret = NULL;
		key_size = 0;
	}

	~DHCommon() {
		if (dh)
			DH_free(dh);
		if (secret)
			free(secret);
	}

	bool ParseDH(const CString &sLine) {
		/*
		 * sLine contains the prime, generator and public key of the server.
		 * We first extract this information and then we pass this to OpenSSL.
		 * OpenSSL will generate our own public and private key. Which we then
		 * use to encrypt our password
		 *
		 * sLine will look something like:
		 *
		 *   base64(
		 *     prime length (2 bytes)
		 *     prime
		 *     generator length (2 bytes)
		 *     generator
		 *     servers public key length (2 bytes)
		 *     servers public key
		 *   )
		 */

		/* Decode base64 into (data, length) */
		CString sData = sLine.Base64Decode_n();
		const unsigned char *data = (const unsigned char*)sData.c_str();
		CString::size_type length = sLine.size();

		if (length < 2) {
			DEBUG("sasl: No prime number");
			return false;
		}

		/* Prime number */
		uint16_t size16;
		memcpy(&size16, data, sizeof(size16));
		unsigned int size = ntohs(size16);
		data += 2;
		length -= 2;

		if (size > length) {
			DEBUG("sasl: Extracting prime number. Invalid length");
			return false;
		}

		dh->p = BN_bin2bn(data, size, NULL);
		data += size;

		/* Generator */
		if (length < 2) {
			DEBUG("sasl: No generator");
			return false;
		}

		memcpy(&size16, data, sizeof(size16));
		size = ntohs(size16);
		data += 2;
		length -= 2;

		if (size > length) {
			DEBUG("sasl: Extracting generator. Invalid length");
			return false;
		}

		dh->g = BN_bin2bn(data, size, NULL);
		data += size;

		/* Server public key */
		if (length < 2) {
			DEBUG("sasl: No public key");
			return false;
		}

		memcpy(&size16, data, sizeof(size16));
		size = ntohs(size16);
		data += 2;
		length -= 2;

		if (size > length) {
			DEBUG("sasl: Extracting server public key. Invalid length");
			return false;
		}

		BIGNUM *server_pub_key = BN_bin2bn(data, size, NULL);

		/* Generate our own public/private keys */
		if (!DH_generate_key(dh)) {
			DEBUG("sasl: Failed to generate keys");
			return false;
		}

		/* Compute shared secret */
		secret = (unsigned char*)malloc(DH_size(dh));
		if ((key_size = DH_compute_key(secret, server_pub_key, dh)) == -1) {
			DEBUG("sasl: Failed to compute shared secret");
			return false;
		}
	
		return true;
	}
};
#endif

class CSASLMod : public CModule {
public:
	MODCONSTRUCTOR(CSASLMod) {
		AddCommand("Help",        static_cast<CModCommand::ModCmdFunc>(&CSASLMod::PrintHelp),
			"search", "Generate this output");
		AddCommand("Set",         static_cast<CModCommand::ModCmdFunc>(&CSASLMod::Set),
			"username password", "Set the password for DH-BLOWFISH/DH-AES/PLAIN");
		AddCommand("Mechanism",   static_cast<CModCommand::ModCmdFunc>(&CSASLMod::SetMechanismCommand),
			"[mechanism[ ...]]", "Set the mechanisms to be attempted (in order)");
		AddCommand("RequireAuth", static_cast<CModCommand::ModCmdFunc>(&CSASLMod::RequireAuthCommand),
			"[yes|no]", "Don't connect if SASL cannot be authenticated");

		m_bAuthenticated = false;
	}

	void PrintHelp(const CString& sLine) {
		HandleHelpCommand(sLine);

		CTable Mechanisms;
		Mechanisms.AddColumn("Mechanism");
		Mechanisms.AddColumn("Description");

		for (size_t i = 0; SupportedMechanisms[i].szName != NULL; i++) {
			Mechanisms.AddRow();
			Mechanisms.SetCell("Mechanism",   SupportedMechanisms[i].szName);
			Mechanisms.SetCell("Description", SupportedMechanisms[i].szDescription);
		}

		PutModule("The following mechanisms are available:");
		PutModule(Mechanisms);
	}

	void Set(const CString& sLine) {
		SetNV("username", sLine.Token(1));
		SetNV("password", sLine.Token(2));

		PutModule("Username has been set to [" + GetNV("username") + "]");
		PutModule("Password has been set to [" + GetNV("password") + "]");
	}

	void SetMechanismCommand(const CString& sLine) {
		CString sMechanisms = sLine.Token(1, true).AsUpper();

		if (!sMechanisms.empty()) {
			VCString vsMechanisms;
			sMechanisms.Split(" ", vsMechanisms);

			for (VCString::const_iterator it = vsMechanisms.begin(); it != vsMechanisms.end(); ++it) {
				if (!SupportsMechanism(*it)) {
					PutModule("Unsupported mechanism: " + *it);
					return;
				}
			}

			SetNV(NV_MECHANISMS, sMechanisms);
		}

		PutModule("Current mechanisms set: " + GetMechanismsString());
	}

	void RequireAuthCommand(const CString& sLine) {
		if (!sLine.Token(1).empty()) {
			SetNV(NV_REQUIRE_AUTH, sLine.Token(1));
		}

		if (GetNV(NV_REQUIRE_AUTH).ToBool()) {
			PutModule("We require SASL negotiation to connect");
		} else {
			PutModule("We will connect even if SASL fails");
		}
	}

	bool SupportsMechanism(const CString& sMechanism) const {
		for (size_t i = 0; SupportedMechanisms[i].szName != NULL; i++) {
			if (sMechanism.Equals(SupportedMechanisms[i].szName)) {
				return true;
			}
		}

		return false;
	}

	CString GetMechanismsString() const {
		if (GetNV(NV_MECHANISMS).empty()) {
			CString sDefaults = "";

			for (size_t i = 0; SupportedMechanisms[i].szName != NULL; i++) {
				if (SupportedMechanisms[i].bDefault) {
					if (!sDefaults.empty()) {
						sDefaults += " ";
					}

					sDefaults += SupportedMechanisms[i].szName;
				}
			}

			return sDefaults;
		}

		return GetNV(NV_MECHANISMS);
	}

	bool CheckRequireAuth() {
		if (!m_bAuthenticated && GetNV(NV_REQUIRE_AUTH).ToBool()) {
			GetNetwork()->SetIRCConnectEnabled(false);
			PutModule("Disabling network, we require authentication.");
			PutModule("Use 'RequireAuth no' to disable.");
			return true;
		}

		return false;
	}

#ifdef HAVE_SASL_MECHANISM
	bool AuthenticateAES(const CString& sLine) {
		CString::size_type length;

		DHCommon dh;
		if (!dh.ParseDH(sLine))
			return false;

		const int len = GetNV("username").size() + GetNV("password").size() + 2;
		const int padlen = 16 - (len % 16);
		CString::size_type userpass_length = len + padlen;
		unsigned char *encrypted_userpass = (unsigned char *)malloc(userpass_length);
		unsigned char *plaintext_userpass = (unsigned char *)malloc(userpass_length);

		memset(encrypted_userpass, 0, userpass_length);

		/* Create plaintext message */
		unsigned char *ptr = plaintext_userpass;
		memcpy(ptr, GetNV("username").c_str(), GetNV("username").size() + 1);
		ptr += GetNV("username").size() + 1;
		memcpy(ptr, GetNV("password").c_str(), GetNV("password").size() + 1);
		ptr += GetNV("password").size() + 1;
		if (padlen)
		{
			/* Padding */
			unsigned char randbytes[16];
			if (!RAND_bytes(randbytes, padlen)) {
				DEBUG("sasl: DH-AES: Unable to pad");
				free(encrypted_userpass);
				free(plaintext_userpass);
				return false;
			}
			memcpy(ptr, randbytes, padlen);
		}

		/* Create the IV 
		 * It is changed during encryption for some reason - so we need to keep a copy.
		 */
		unsigned char iv[16], iv_copy[16];
		if (!RAND_bytes(iv, sizeof (iv))) {
			DEBUG("sasl: DH-AES: Unable to create IV");
			free(encrypted_userpass);
			free(plaintext_userpass);
			return false;
		}
		memcpy(iv_copy, iv, sizeof(iv));

		/* Encrypt */
		AES_KEY key;
		AES_set_encrypt_key(dh.secret, dh.key_size * 8, &key);
		AES_cbc_encrypt(plaintext_userpass, encrypted_userpass, userpass_length,
				&key, iv_copy, AES_ENCRYPT);

		free(plaintext_userpass);

		/* Build our response */
		length = 2 + dh.key_size + sizeof(iv) + userpass_length;
		char *response = (char *)malloc(length);
		char *out_ptr = response;

		/* Size of the key + key */
		uint16_t size16 = htons((uint16_t)dh.key_size);
		memcpy(out_ptr, &size16, sizeof(size16));
		out_ptr += 2;
		BN_bn2bin(dh.dh->pub_key, (unsigned char *)out_ptr);
		out_ptr += dh.key_size;

		/* Add the IV */
		memcpy(out_ptr, iv, sizeof(iv));
		out_ptr += sizeof(iv);

		/* Add encrypted userpass to the response */
		memcpy(out_ptr, encrypted_userpass, userpass_length);
		free(encrypted_userpass);

		PutIRC("AUTHENTICATE " + CString((const char *)response, length).Base64Encode_n());

		DEBUG(CString((const char *)response, length).Base64Encode_n());

		free(response);
		return true;
	}

	bool AuthenticateBlowfish(const CString& sLine) {
		/* Encrypt our sasl password with blowfish
		 * 
		 * Our response should look something like:
		 *
		 *   base64(
		 *     our public key length (2 bytes)
		 *     our public key
		 *     sasl username + \0
		 *     blowfish(
		 *       sasl password
		 *     )
		 *   )
		 */
		CString::size_type length;

		/* Our DH params */
		DHCommon dh;
		if (!dh.ParseDH(sLine))
			return false;

		// TODO for passwords with length 8, 16, 24, 32, etc. this will have 8 additional zero bytes at the end...
		// But it works when treated as null-terminated string anyway, and if it works I don't want to touch it right now.
		CString::size_type password_length = GetNV("password").size() + (8 - (GetNV("password").size() % 8));
		unsigned char *encrypted_password = (unsigned char *)malloc(password_length);
		char *plaintext_password = (char *)malloc(password_length);

		memset(encrypted_password, 0, password_length);
		memset(plaintext_password, 0, password_length);
		memcpy(plaintext_password, GetNV("password").c_str(), GetNV("password").size());

		BF_KEY key;
		BF_set_key(&key, dh.key_size, dh.secret);

		char *out_ptr = (char *)encrypted_password;
		char *in_ptr = (char *)plaintext_password;
		for (length = password_length; length; length -= 8, in_ptr += 8, out_ptr += 8) {
			BF_ecb_encrypt((unsigned char *)in_ptr, (unsigned char *)out_ptr, &key, BF_ENCRYPT);
		}

		free(plaintext_password);

		/* Build our response */
		length = 2 + BN_num_bytes(dh.dh->pub_key) + password_length + GetNV("username").size() + 1;
		char *response = (char *)malloc(length);
		out_ptr = response;

		/* Add our key to the response */
		uint16_t size16 = htons((uint16_t)BN_num_bytes(dh.dh->pub_key));
		memcpy(out_ptr, &size16, sizeof(size16));
		out_ptr += 2;
		BN_bn2bin(dh.dh->pub_key, (unsigned char *)out_ptr);
		out_ptr += BN_num_bytes(dh.dh->pub_key);

		/* Add sasl username to response */
		memcpy(out_ptr, GetNV("username").c_str(), GetNV("username").length() + 1); // +1 for zero byte in the end
		out_ptr += GetNV("username").length() + 1;

		/* Finally add the encrypted password to the response */
		memcpy(out_ptr, encrypted_password, password_length);
		free(encrypted_password);

		/* Base 64 encode and send! */
		PutIRC("AUTHENTICATE " + CString((const char *)response, length).Base64Encode_n());

		free(response);
		return true;
	}
#endif

	void Authenticate(const CString& sLine) {
		if (m_Mechanisms.GetCurrent().Equals("PLAIN") && sLine.Equals("+")) {
			CString sAuthLine = GetNV("username") + '\0' + GetNV("username")  + '\0' + GetNV("password");
			sAuthLine.Base64Encode();
			PutIRC("AUTHENTICATE " + sAuthLine);
#ifdef HAVE_SASL_MECHANISM
		} else if (m_Mechanisms.GetCurrent().Equals("DH-BLOWFISH")) {
			AuthenticateBlowfish(sLine);
		} else if (m_Mechanisms.GetCurrent().Equals("DH-AES")) {
			AuthenticateAES(sLine);
#endif
		} else {
			/* Send blank authenticate for other mechanisms (like EXTERNAL). */
			PutIRC("AUTHENTICATE +");
		}
	}

	virtual bool OnServerCapAvailable(const CString& sCap) {
		return sCap.Equals("sasl");
	}

	virtual void OnServerCapResult(const CString& sCap, const bool bSuccess) {
		if (sCap.Equals("sasl")) {
			if (bSuccess) {
				GetMechanismsString().Split(" ", m_Mechanisms);

				if (m_Mechanisms.empty()) {
					CheckRequireAuth();
					return;
				}

				m_pNetwork->GetIRCSock()->PauseCap();

				m_Mechanisms.SetIndex(0);
				PutIRC("AUTHENTICATE " + m_Mechanisms.GetCurrent());
			} else {
				CheckRequireAuth();
			}
		}
	}

	virtual EModRet OnRaw(CString &sLine) {
		if (sLine.Token(0).Equals("AUTHENTICATE")) {
			Authenticate(sLine.Token(1, true));
		} else if (sLine.Token(1).Equals("903")) {
			/* SASL success! */
			m_pNetwork->GetIRCSock()->ResumeCap();
			m_bAuthenticated = true;
			DEBUG("sasl: Authenticated with mechanism [" << m_Mechanisms.GetCurrent() << "]");
		} else if (sLine.Token(1).Equals("904") || sLine.Token(1).Equals("905")) {
			DEBUG("sasl: Mechanism [" << m_Mechanisms.GetCurrent() << "] failed.");
			PutModule(m_Mechanisms.GetCurrent() + " mechanism failed.");

			if (m_Mechanisms.HasNext()) {
				m_Mechanisms.IncrementIndex();
				PutIRC("AUTHENTICATE " + m_Mechanisms.GetCurrent());
			} else {
				CheckRequireAuth();
				m_pNetwork->GetIRCSock()->ResumeCap();
			}
		} else if (sLine.Token(1).Equals("906")) {
			/* CAP wasn't paused? */
			DEBUG("sasl: Reached 906.");
			CheckRequireAuth();
		} else if (sLine.Token(1).Equals("907")) {
			m_bAuthenticated = true;
			m_pNetwork->GetIRCSock()->ResumeCap();
			DEBUG("sasl: Received 907 -- We are already registered");
		} else {
			return CONTINUE;
		}

		return HALT;
	}

	virtual void OnIRCConnected() {
		/* Just incase something slipped through, perhaps the server doesn't
		 * respond to our CAP negotiation. */

		CheckRequireAuth();
	}

	virtual void OnIRCDisconnected() {
		m_bAuthenticated = false;
	}
private:
	Mechanisms m_Mechanisms;
	bool m_bAuthenticated;
};

template<> void TModInfo<CSASLMod>(CModInfo& Info) {
	Info.SetWikiPage("sasl");
}

NETWORKMODULEDEFS(CSASLMod, "Adds support for sasl authentication capability to authenticate to an IRC server")
