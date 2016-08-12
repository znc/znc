/*
 * Copyright (C) 2015 Michael Morris
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

/* Notes: */
/* This module sends the certificate chain of a connection to the
 connected client so that the client can display this information
 in a user friendly dialog. */
/* As-is customary, the chain starts with the peer certificate and
 and ends with the root certificate authority certificate. */
/* The certificate chain is sent using the BATCH command. */
/* Each certificate is surrounded in its own nested BATCH within
 a single global BATCH for the whole session. */

/* Example response:

	>> :znc.in BATCH +1f5fdd znc.in/tlsinfo
	>> @batch=1f5fdd :znc.in BATCH +6f7a1a znc.in/tlsinfo-certificate
	>> @batch=6f7a1a :znc.in CERTINFO ExampleUser :-----BEGIN CERTIFICATE-----
	>> @batch=6f7a1a :znc.in CERTINFO ExampleUser :...
	>> @batch=6f7a1a :znc.in CERTINFO ExampleUser :-----END CERTIFICATE-----
	>> @batch=1f5fdd :znc.in BATCH -6f7a1a
	>> @batch=1f5fdd :znc.in BATCH +31fbfc znc.in/tlsinfo-certificate
	>> @batch=31fbfc :znc.in CERTINFO ExampleUser :-----BEGIN CERTIFICATE-----
	>> @batch=31fbfc :znc.in CERTINFO ExampleUser :...
	>> @batch=31fbfc :znc.in CERTINFO ExampleUser :-----END CERTIFICATE-----
	>> @batch=1f5fdd :znc.in BATCH -31fbfc
	>> :znc.in BATCH -1f5fdd
 */

#include <znc/Modules.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>

static const char *TlsInfoCap = "znc.in/tlsinfo";

static const char *TlsInfoBatchGlobalType = "znc.in/tlsinfo";
static const char *TlsInfoBatchChildType = "znc.in/tlsinfo-certificate";

class CTlsInfoMod : public CModule
{
public:

#ifndef HAVE_LIBSSL
	MODCONSTRUCTOR(CTlsInfoMod) {}

	bool OnLoad(const CString&, CString& sMessage) {
		sMessage = "Module built against install of ZNC that lacks SSL support.";

		return false;
	}

#else // HAVE_LIBSSL

	MODCONSTRUCTOR(CTlsInfoMod) {
		AddHelpCommand();
		AddCommand("Cert", static_cast<CModCommand::ModCmdFunc>(&CTlsInfoMod::PrintCertificateCommand), "[details]", "View certificate information for the active connection. Append 'details' to the 'cert' command ('cert details') to include the entire certificate chain in output.");
		AddCommand("Cipher", static_cast<CModCommand::ModCmdFunc>(&CTlsInfoMod::PrintCipherCommand), "", "View the protocol and cipher suite used for the active connection.");
		AddCommand("Send-Data", static_cast<CModCommand::ModCmdFunc>(&CTlsInfoMod::SendCertificateCommand), "", "Send certificate information for the active connection in an easily parsable format for application developers.");
	}

	void OnClientCapLs(CClient *mClient, SCString &mCaps) override
	{
		mCaps.insert(TlsInfoCap);
	}

	bool IsClientCapSupported(CClient *mClient, const CString &mCap, bool mState) override
	{
		return mCap.Equals(TlsInfoCap);
	}

	void PrintCipherCommand(const CString &mLine)
	{
		CClient *mClient;

		SSL *mSSLObject;

		if (GetRelevantObjects(nullptr, &mClient, nullptr, &mSSLObject) == false) {
			return;
		}

		/* Determine the protocol used for the connection */
		int mSSLVersion = SSL_version(mSSLObject);

		CString mSSLVersionString;

		if (mSSLVersion == TLS1_2_VERSION) {
			mSSLVersionString = CString("Transport Layer Security (TLS), version 1.2");
		} else if (mSSLVersion == TLS1_1_VERSION) {
			mSSLVersionString = CString("Transport Layer Security (TLS), version 1.1");
		} else if (mSSLVersion == TLS1_VERSION) {
			mSSLVersionString = CString("Transport Layer Security (TLS), version 1.0");
		} else if (mSSLVersion == SSL3_VERSION) {
			mSSLVersionString = CString("Secure Sockets Layer (SSL), version 3.0");
		} else {
			mSSLVersionString = CString("Unknown");
		}

		/* Output cipher information */
		CString mCipherNameString = CString(SSL_get_cipher_name(mSSLObject));

		PutModule("Connection secured using " + mSSLVersionString + " with the cipher suite: " + mCipherNameString);
	}

	void PrintCertificateCommand(const CString &mLine)
	{
		bool mPrintCertificateParents = false;

		CString sCmd = mLine.Token(1);

		if (sCmd.Equals("details")) {
			mPrintCertificateParents = true;
		}

		PresentCertificateInformation(mLine, true, mPrintCertificateParents);
	}

	void SendCertificateCommand(const CString &mLine)
	{
		PresentCertificateInformation(mLine, false, true);
	}

	void PresentCertificateInformation(const CString &mLine, bool mPrintCertificates = false, bool mPrintCertificateParents = false)
	{
		CClient *mClient;

		SSL *mSSLObject;

		if (GetRelevantObjects(nullptr, &mClient, nullptr, &mSSLObject) == false) {
			return;
		}

		if (mClient->IsCapEnabled(TlsInfoCap) == false ||
			mClient->HasBatch() == false)
		{
			mPrintCertificates = true;
		}

		/* Obtain list of certificates from SSL context object */
		STACK_OF(X509) *mCertificateChain = SSL_get_peer_cert_chain(mSSLObject);

		if (mCertificateChain == NULL) {
			PutModule("Error: SSL_get_peer_cert_chain() returned nullptr");

			return;
		}

		if (mPrintCertificates) {
			PrintCertificateChainToQuery(mClient, mCertificateChain, mPrintCertificateParents);
		} else {
			SendCertificateChain(mClient, mCertificateChain);
		}
	}

private:
	bool GetRelevantObjects(CIRCNetwork **eNetwork, CClient **eClient, CIRCSock **eSocket, SSL **eSSLObject)
	{
		CClient *mClient = GetClient();

		if (mClient == nullptr) {
			PutModule("Error: GetClient() returned nullptr");

			return false;
		}

		/* Check whether we are connected and whether SSL is in use. */
		CIRCNetwork *mNetwork = mClient->GetNetwork();

		if (mNetwork == nullptr) {
			PutModule("Error: mClient->GetNetwork() returned nullptr");

			return false;
		}

		CIRCSock *mSocket = mNetwork->GetIRCSock();

		if (mSocket == nullptr) {
			PutModule("Error: mNetwork->GetIRCSock() returned nullptr");

			return false;
		}

		if (mSocket->GetSSL() == false) {
			PutModule("Error: Client is not connected using SSL/TLS");

			return false;
		}

		/* Ask the socket for the SSL context object */
		SSL *mSSLObject = mSocket->GetSSLObject();

		if (mSSLObject == nullptr) {
			PutModule("Error: mSocket->GetSSLObject() returned nullptr");

			return false;
		}

		/* Assign objects and return success */
		if ( eNetwork) {
			*eNetwork = mNetwork;
		}

		if ( eSocket) {
			*eSocket = mSocket;
		}

		if ( eClient) {
			*eClient = mClient;
		}

		if ( eSSLObject) {
			*eSSLObject = mSSLObject;
		}

		return true;
	}

	void SendCertificateChain(CClient *mClient, STACK_OF(X509) *mCertificateChain)
	{
		/* Send batch command opening to client */
		CString mBatchName = CString::RandomString(10).MD5();

		CString mNickname = mClient->GetNick();

		mClient->PutClient(":znc.in BATCH +" + mBatchName + " " + TlsInfoBatchGlobalType);

		/* The certificates are converted into PEM format, then each line
		 is sent as a separate value to client. */
		for (size_t i = 0; i < sk_X509_num(mCertificateChain); i++)
		{
			/* Convert certificate into PEM format in a BIO buffer */
			X509 *certificate = sk_X509_value(mCertificateChain, i);

			BIO *bio_out = BIO_new(BIO_s_mem());

			PEM_write_bio_X509(bio_out, certificate);

			BUF_MEM *bio_buf;
			BIO_get_mem_ptr(bio_out, &bio_buf);

			/* Create string from the BIO buffer,  the data up into
			 lines by newline, and send the result to the client. */
			CString pemDataString = CString(bio_buf->data, bio_buf->length);

			VCString pemDataStringSplit;
			pemDataString.Split("\n", pemDataStringSplit, false);

			CString pemBatchName = CString::RandomString(10).MD5();

			mClient->PutClient("@batch=" + mBatchName + " :znc.in BATCH +" + pemBatchName + " " + TlsInfoBatchChildType);

			for (const CString& s : pemDataStringSplit) {
				mClient->PutClient("@batch=" + pemBatchName + " :znc.in CERTINFO " + mNickname + " :" + s);
			}

			mClient->PutClient("@batch=" + mBatchName + " :znc.in BATCH -" + pemBatchName + " " + TlsInfoBatchChildType);

			/* Cleanup memory allocation */
			BIO_free(bio_out);
		}

		/* Send batch command closing to client */
		mClient->PutClient(":znc.in BATCH -" + mBatchName);
	}

	void PrintCertificateChainToQuery(CClient *mClient, STACK_OF(X509) *mCertificateChain, bool mPrintCertificateParents = false)
	{
		/* Print certificate chain */
		for (size_t i = 0; i < sk_X509_num(mCertificateChain); i++)
		{
			/* Convert printed result into a single string. */
			X509 *certificate = sk_X509_value(mCertificateChain, i);

			BIO *bio_out = BIO_new(BIO_s_mem());

			X509_print(bio_out, certificate);

			BUF_MEM *bio_buf;
			BIO_get_mem_ptr(bio_out, &bio_buf);

			CString certificateString = CString(bio_buf->data, bio_buf->length);

			/* Split string into newlines and escape each line similar to how ZNC
			 does it when presenting a certificate for fingerprinting. */
			CString certificateNumber = CString(i + 1);

			if (i == 0) {
				PutModule("| ---- Certificate #" + certificateNumber + " (Peer Certificate) Start ---- |");
			} else {
				PutModule("| ---- Certificate #" + certificateNumber + " Start ---- |");
			}

			VCString certificateStringSplit;

			certificateString.Split("\n", certificateStringSplit);

			for (const CString& s : certificateStringSplit) {
				PutModule("| " + s.Escape_n(CString::EDEBUG));
			}

			PutModule("| ---- Certificate #" + certificateNumber + " End ---- |");

			/* Cleanup memory allocation */
			BIO_free(bio_out);

			/* First certificate is the peer certificate so if we do not
			 need the rest of the chain, then break the loop here. */
			if (mPrintCertificateParents == false) {
				break;
			}
		}
	}
#endif
	
};

GLOBALMODULEDEFS(CTlsInfoMod, "A module for presenting information about SSL/TLS connection")
