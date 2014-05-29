/*
 * Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
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

#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <znc/Chan.h>

using std::set;

#ifndef Q_DEBUG_COMMUNICATION
	#define Q_DEBUG_COMMUNICATION 0
#endif

class CQModule : public CModule {
public:
	MODCONSTRUCTOR(CQModule) {}
	virtual ~CQModule() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (!sArgs.empty()) {
			SetUsername(sArgs.Token(0));
			SetPassword(sArgs.Token(1));
		} else {
			m_sUsername = GetNV("Username");
			m_sPassword = GetNV("Password");
		}

		CString sTmp;
		m_bUseCloakedHost   = (sTmp = GetNV("UseCloakedHost")).empty() ? true : sTmp.ToBool();
		m_bUseChallenge     = (sTmp = GetNV("UseChallenge")).empty()  ? true : sTmp.ToBool();
		m_bRequestPerms     = GetNV("RequestPerms").ToBool();
		m_bJoinOnInvite     = (sTmp = GetNV("JoinOnInvite")).empty() ? true : sTmp.ToBool();
		m_bJoinAfterCloaked = (sTmp = GetNV("JoinAfterCloaked")).empty() ? true : sTmp.ToBool();

		// Make sure NVs are stored in config. Note: SetUseCloakedHost() is called further down.
		SetUseChallenge(m_bUseChallenge);
		SetRequestPerms(m_bRequestPerms);
		SetJoinOnInvite(m_bJoinOnInvite);
		SetJoinAfterCloaked(m_bJoinAfterCloaked);

		OnIRCDisconnected(); // reset module's state

		if (IsIRCConnected()) {
			// check for usermode +x if we are already connected
			set<unsigned char> scUserModes = m_pNetwork->GetIRCSock()->GetUserModes();
			if (scUserModes.find('x') != scUserModes.end())
				m_bCloaked = true;

			// This will only happen once, and only if the user loads the module after connecting to IRC.
			// Also don't notify the user in case he already had mode +x set.
			if (GetNV("UseCloakedHost").empty()) {
				if (!m_bCloaked)
					PutModule("Notice: Your host will be cloaked the next time you reconnect to IRC. "
						"If you want to cloak your host now, /msg *q Cloak. You can set your preference "
						"with /msg *q Set UseCloakedHost true/false.");
				m_bUseCloakedHost = true;
				SetUseCloakedHost(m_bUseCloakedHost);
				m_bJoinAfterCloaked = true;
				SetJoinAfterCloaked(m_bJoinAfterCloaked);
			} else if (m_bUseChallenge) {
				Cloak();
			}
			WhoAmI();
		} else {
			SetUseCloakedHost(m_bUseCloakedHost);
		}

		return true;
	}

	virtual void OnIRCDisconnected() {
		m_bCloaked = false;
		m_bAuthed  = false;
		m_bRequestedWhoami    = false;
		m_bRequestedChallenge = false;
		m_bCatchResponse = false;
	}

	virtual void OnIRCConnected() {
		if (m_bUseCloakedHost)
			Cloak();
		WhoAmI();
	}

	virtual void OnModCommand(const CString& sLine) {
		CString sCommand = sLine.Token(0).AsLower();

		if (sCommand == "help") {
			PutModule("The following commands are available:");
			CTable Table;
			Table.AddColumn("Command");
			Table.AddColumn("Description");
			Table.AddRow();
			Table.SetCell("Command", "Auth [<username> <password>]");
			Table.SetCell("Description", "Tries to authenticate you with Q. Both parameters are optional.");
			Table.AddRow();
			Table.SetCell("Command", "Cloak");
			Table.SetCell("Description", "Tries to set usermode +x to hide your real hostname.");
			Table.AddRow();
			Table.SetCell("Command", "Status");
			Table.SetCell("Description", "Prints the current status of the module.");
			Table.AddRow();
			Table.SetCell("Command", "Update");
			Table.SetCell("Description", "Re-requests the current user information from Q.");
			Table.AddRow();
			Table.SetCell("Command", "Set <setting> <value>");
			Table.SetCell("Description", "Changes the value of the given setting. See the list of settings below.");
			Table.AddRow();
			Table.SetCell("Command", "Get");
			Table.SetCell("Description", "Prints out the current configuration. See the list of settings below.");
			PutModule(Table);

			PutModule("The following settings are available:");
			CTable Table2;
			Table2.AddColumn("Setting");
			Table2.AddColumn("Type");
			Table2.AddColumn("Description");
			Table2.AddRow();
			Table2.SetCell("Setting", "Username");
			Table2.SetCell("Type", "String");
			Table2.SetCell("Description", "Your Q username.");
			Table2.AddRow();
			Table2.SetCell("Setting", "Password");
			Table2.SetCell("Type", "String");
			Table2.SetCell("Description", "Your Q password.");
			Table2.AddRow();
			Table2.SetCell("Setting", "UseCloakedHost");
			Table2.SetCell("Type", "Boolean");
			Table2.SetCell("Description", "Whether to cloak your hostname (+x) automatically on connect.");
			Table2.AddRow();
			Table2.SetCell("Setting", "UseChallenge");
			Table2.SetCell("Type", "Boolean");
			Table2.SetCell("Description", "Whether to use the CHALLENGEAUTH mechanism to avoid sending passwords in cleartext.");
			Table2.AddRow();
			Table2.SetCell("Setting", "RequestPerms");
			Table2.SetCell("Type", "Boolean");
			Table2.SetCell("Description", "Whether to request voice/op from Q on join/devoice/deop.");
			Table2.AddRow();
			Table2.SetCell("Setting", "JoinOnInvite");
			Table2.SetCell("Type", "Boolean");
			Table2.SetCell("Description", "Whether to join channels when Q invites you.");
			Table2.AddRow();
			Table2.SetCell("Setting", "JoinAfterCloaked");
			Table2.SetCell("Type", "Boolean");
			Table2.SetCell("Description", "Whether to delay joining channels until after you are cloaked.");
			PutModule(Table2);

			PutModule("This module takes 2 optional parameters: <username> <password>");
			PutModule("Module settings are stored between restarts.");

		} else if (sCommand == "set") {
			CString sSetting = sLine.Token(1).AsLower();
			CString sValue   = sLine.Token(2);
			if (sSetting.empty() || sValue.empty()) {
				PutModule("Syntax: Set <setting> <value>");
			} else if (sSetting == "username") {
				SetUsername(sValue);
				PutModule("Username set");
			} else if (sSetting == "password") {
				SetPassword(sValue);
				PutModule("Password set");
			} else if (sSetting == "usecloakedhost") {
				SetUseCloakedHost(sValue.ToBool());
				PutModule("UseCloakedHost set");
			} else if (sSetting == "usechallenge") {
				SetUseChallenge(sValue.ToBool());
				PutModule("UseChallenge set");
			} else if (sSetting == "requestperms") {
				SetRequestPerms(sValue.ToBool());
				PutModule("RequestPerms set");
			} else if (sSetting == "joinoninvite") {
				SetJoinOnInvite(sValue.ToBool());
				PutModule("JoinOnInvite set");
			} else if (sSetting == "joinaftercloaked") {
				SetJoinAfterCloaked(sValue.ToBool());
				PutModule("JoinAfterCloaked set");
			} else
				PutModule("Unknown setting: " + sSetting);

		} else if (sCommand == "get" || sCommand == "list") {
			CTable Table;
			Table.AddColumn("Setting");
			Table.AddColumn("Value");
			Table.AddRow();
			Table.SetCell("Setting", "Username");
			Table.SetCell("Value", m_sUsername);
			Table.AddRow();
			Table.SetCell("Setting", "Password");
			Table.SetCell("Value", "*****"); // m_sPassword
			Table.AddRow();
			Table.SetCell("Setting", "UseCloakedHost");
			Table.SetCell("Value", CString(m_bUseCloakedHost));
			Table.AddRow();
			Table.SetCell("Setting", "UseChallenge");
			Table.SetCell("Value", CString(m_bUseChallenge));
			Table.AddRow();
			Table.SetCell("Setting", "RequestPerms");
			Table.SetCell("Value", CString(m_bRequestPerms));
			Table.AddRow();
			Table.SetCell("Setting", "JoinOnInvite");
			Table.SetCell("Value", CString(m_bJoinOnInvite));
			Table.AddRow();
			Table.SetCell("Setting", "JoinAfterCloaked");
			Table.SetCell("Value", CString(m_bJoinAfterCloaked));
			PutModule(Table);

		} else if (sCommand == "status") {
			PutModule("Connected: " + CString(IsIRCConnected() ? "yes" : "no"));
			PutModule("Cloaked: "   + CString(m_bCloaked       ? "yes" : "no"));
			PutModule("Authed: "    + CString(m_bAuthed        ? "yes" : "no"));

		} else {
			// The following commands require an IRC connection.
			if (!IsIRCConnected()) {
				PutModule("Error: You are not connected to IRC.");
				return;
			}

			if (sCommand == "cloak") {
				if (!m_bCloaked)
					Cloak();
				else
					PutModule("Error: You are already cloaked!");

			} else if (sCommand == "auth") {
				if (!m_bAuthed)
					Auth(sLine.Token(1), sLine.Token(2));
				else
					PutModule("Error: You are already authed!");

			} else if (sCommand == "update") {
				WhoAmI();
				PutModule("Update requested.");

			} else {
				PutModule("Unknown command. Try 'help'.");
			}
		}
	}

	virtual EModRet OnRaw(CString& sLine) {
		// use OnRaw because OnUserMode is not defined (yet?)
		if (sLine.Token(1) == "396" && sLine.Token(3).find("users.quakenet.org") != CString::npos) {
			m_bCloaked = true;
			PutModule("Cloak successful: Your hostname is now cloaked.");
		}
		return CONTINUE;
	}

	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage) {
		return HandleMessage(Nick, sMessage);
	}

	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage) {
		return HandleMessage(Nick, sMessage);
	}

	virtual EModRet OnJoining(CChan& Channel) {
		if (m_bJoinAfterCloaked && !m_bCloaked)
			return HALT;

		return CONTINUE;
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		if (m_bRequestPerms && IsSelf(Nick))
			HandleNeed(Channel, "ov");
	}

	virtual void OnDeop2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
		if (m_bRequestPerms && IsSelf(Nick) && (!pOpNick || !IsSelf(*pOpNick)))
			HandleNeed(Channel, "o");
	}

	virtual void OnDevoice2(const CNick* pOpNick, const CNick& Nick, CChan& Channel, bool bNoChange) {
		if (m_bRequestPerms && IsSelf(Nick) && (!pOpNick || !IsSelf(*pOpNick)))
			HandleNeed(Channel, "v");
	}

	virtual EModRet OnInvite(const CNick& Nick, const CString& sChan) {
		if (!Nick.NickEquals("Q") || !Nick.GetHost().Equals("CServe.quakenet.org"))
			return CONTINUE;
		if (m_bJoinOnInvite)
			m_pNetwork->AddChan(sChan, false);
		return CONTINUE;
	}

	virtual CString GetWebMenuTitle() { return "Q"; }

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		if (sPageName == "index") {
			bool bSubmitted = (WebSock.GetParam("submitted").ToInt() != 0);

			if (bSubmitted) {
				CString FormUsername = WebSock.GetParam("user");
				if (!FormUsername.empty())
					SetUsername(FormUsername);

				CString FormPassword = WebSock.GetParam("password");
				if (!FormPassword.empty())
					SetPassword(FormPassword);

				SetUseCloakedHost(WebSock.GetParam("usecloakedhost").ToBool());
				SetUseChallenge(WebSock.GetParam("usechallenge").ToBool());
				SetRequestPerms(WebSock.GetParam("requestperms").ToBool());
				SetJoinOnInvite(WebSock.GetParam("joinoninvite").ToBool());
				SetJoinAfterCloaked(WebSock.GetParam("joinaftercloaked").ToBool());
			}

			Tmpl["Username"] = m_sUsername;

			CTemplate& o1 = Tmpl.AddRow("OptionLoop");
			o1["Name"] = "usecloakedhost";
			o1["DisplayName"] = "UseCloakedHost";
			o1["Tooltip"] = "Whether to cloak your hostname (+x) automatically on connect.";
			o1["Checked"] = CString(m_bUseCloakedHost);

			CTemplate& o2 = Tmpl.AddRow("OptionLoop");
			o2["Name"] = "usechallenge";
			o2["DisplayName"] = "UseChallenge";
			o2["Tooltip"] = "Whether to use the CHALLENGEAUTH mechanism to avoid sending passwords in cleartext.";
			o2["Checked"] = CString(m_bUseChallenge);

			CTemplate& o3 = Tmpl.AddRow("OptionLoop");
			o3["Name"] = "requestperms";
			o3["DisplayName"] = "RequestPerms";
			o3["Tooltip"] = "Whether to request voice/op from Q on join/devoice/deop.";
			o3["Checked"] = CString(m_bRequestPerms);

			CTemplate& o4 = Tmpl.AddRow("OptionLoop");
			o4["Name"] = "joinoninvite";
			o4["DisplayName"] = "JoinOnInvite";
			o4["Tooltip"] = "Whether to join channels when Q invites you.";
			o4["Checked"] = CString(m_bJoinOnInvite);

			CTemplate& o5 = Tmpl.AddRow("OptionLoop");
			o5["Name"] = "joinaftercloaked";
			o5["DisplayName"] = "JoinAfterCloaked";
			o5["Tooltip"] = "Whether to delay joining channels until after you are cloaked.";
			o5["Checked"] = CString(m_bJoinAfterCloaked);

			if (bSubmitted) {
				WebSock.GetSession()->AddSuccess("Changes have been saved!");
			}

			return true;
		}

		return false;
	}

private:
	bool m_bCloaked;
	bool m_bAuthed;
	bool m_bRequestedWhoami;
	bool m_bRequestedChallenge;
	bool m_bCatchResponse;
	MCString m_msChanModes;

	void PutQ(const CString& sMessage) {
		PutIRC("PRIVMSG Q@CServe.quakenet.org :" + sMessage);
#if Q_DEBUG_COMMUNICATION
		PutModule("[ZNC --> Q] " + sMessage);
#endif
	}

	void Cloak() {
		if (m_bCloaked)
			return;

		PutModule("Cloak: Trying to cloak your hostname, setting +x...");
		PutIRC("MODE " + m_pNetwork->GetIRCSock()->GetNick() + " +x");
	}

	void WhoAmI() {
		m_bRequestedWhoami = true;
		PutQ("WHOAMI");
	}

	void Auth(const CString& sUsername = "", const CString& sPassword = "") {
		if (m_bAuthed)
			return;

		if (!sUsername.empty())
			SetUsername(sUsername);
		if (!sPassword.empty())
			SetPassword(sPassword);

		if (m_sUsername.empty() || m_sPassword.empty()) {
			PutModule("You have to set a username and password to use this module! See 'help' for details.");
			return;
		}

		if (m_bUseChallenge) {
			PutModule("Auth: Requesting CHALLENGE...");
			m_bRequestedChallenge = true;
			PutQ("CHALLENGE");
		} else {
			PutModule("Auth: Sending AUTH request...");
			PutQ("AUTH " + m_sUsername + " " + m_sPassword);
		}
	}

	void ChallengeAuth(CString sChallenge) {
		if (m_bAuthed)
			return;

		CString sUsername     = m_sUsername.AsLower()
		                                   .Replace_n("[",  "{")
		                                   .Replace_n("]",  "}")
		                                   .Replace_n("\\", "|");
		CString sPasswordHash = m_sPassword.Left(10).SHA256();
		CString sKey          = CString(sUsername + ":" + sPasswordHash).SHA256();
		CString sResponse     = HMAC_SHA256(sKey, sChallenge);

		PutModule("Auth: Received challenge, sending CHALLENGEAUTH request...");
		PutQ("CHALLENGEAUTH " + m_sUsername + " " + sResponse + " HMAC-SHA-256");
	}

	EModRet HandleMessage(const CNick& Nick, CString sMessage) {
		if (!Nick.NickEquals("Q") || !Nick.GetHost().Equals("CServe.quakenet.org"))
			return CONTINUE;

		sMessage.Trim();

#if Q_DEBUG_COMMUNICATION
		PutModule("[ZNC <-- Q] " + sMessage);
#endif

		// WHOAMI
		if (sMessage.find("WHOAMI is only available to authed users") != CString::npos) {
			m_bAuthed = false;
			Auth();
			m_bCatchResponse = m_bRequestedWhoami;
		}
		else if (sMessage.find("Information for user") != CString::npos) {
			m_bAuthed = true;
			m_msChanModes.clear();
			m_bCatchResponse = m_bRequestedWhoami;
			m_bRequestedWhoami = true;
		}
		else if (m_bRequestedWhoami && sMessage.WildCmp("#*")) {
			CString sChannel = sMessage.Token(0);
			CString sFlags   = sMessage.Token(1, true).Trim_n().TrimLeft_n("+");
			m_msChanModes[sChannel] = sFlags;
		}
		else if (m_bRequestedWhoami && m_bCatchResponse
				&& (sMessage.Equals("End of list.")
				||  sMessage.Equals("account, or HELLO to create an account."))) {
			m_bRequestedWhoami = m_bCatchResponse = false;
			return HALT;
		}

		// AUTH
		else if (sMessage.Equals("Username or password incorrect.")) {
			m_bAuthed = false;
			PutModule("Auth failed: " + sMessage);
			return HALT;
		}
		else if (sMessage.WildCmp("You are now logged in as *.")) {
			m_bAuthed = true;
			PutModule("Auth successful: " + sMessage);
			WhoAmI();
			return HALT;
		}
		else if (m_bRequestedChallenge && sMessage.Token(0).Equals("CHALLENGE")) {
			m_bRequestedChallenge = false;
			if (sMessage.find("not available once you have authed") != CString::npos) {
				m_bAuthed = true;
			} else {
				if (sMessage.find("HMAC-SHA-256") != CString::npos) {
					ChallengeAuth(sMessage.Token(1));
				} else {
					PutModule("Auth failed: Q does not support HMAC-SHA-256 for CHALLENGEAUTH, falling back to standard AUTH.");
					SetUseChallenge(false);
					Auth();
				}
			}
			return HALT;
		}

		// prevent buffering of Q's responses
		return !m_bCatchResponse && GetUser()->IsUserAttached() ? CONTINUE : HALT;
	}

	void HandleNeed(const CChan& Channel, const CString& sPerms) {
		MCString::iterator it = m_msChanModes.find(Channel.GetName());
		if (it == m_msChanModes.end())
			return;
		CString sModes = it->second;

		bool bMaster = (sModes.find("m") != CString::npos) || (sModes.find("n") != CString::npos);

		if (sPerms.find("o") != CString::npos) {
			bool bOp     = (sModes.find("o") != CString::npos);
			bool bAutoOp = (sModes.find("a") != CString::npos);
			if (bMaster || bOp) {
				if (!bAutoOp) {
					PutModule("RequestPerms: Requesting op on " + Channel.GetName());
					PutQ("OP " + Channel.GetName());
				}
				return;
			}
		}

		if (sPerms.find("v") != CString::npos) {
			bool bVoice     = (sModes.find("v") != CString::npos);
			bool bAutoVoice = (sModes.find("g") != CString::npos);
			if (bMaster || bVoice) {
				if (!bAutoVoice) {
					PutModule("RequestPerms: Requesting voice on " + Channel.GetName());
					PutQ("VOICE " + Channel.GetName());
				}
				return;
			}
		}
	}


/* Utility Functions */
	bool IsIRCConnected() {
		CIRCSock* pIRCSock = m_pNetwork->GetIRCSock();
		return pIRCSock && pIRCSock->IsAuthed();
	}

	bool IsSelf(const CNick& Nick) {
		return Nick.NickEquals(m_pNetwork->GetCurNick());
	}

	bool PackHex(const CString& sHex, CString& sPackedHex) {
		if (sHex.length() % 2)
			return false;

		sPackedHex.clear();

		CString::size_type len = sHex.length() / 2;
		for (CString::size_type i = 0; i < len; i++) {
			unsigned int value;
			int n = sscanf(&sHex[i*2], "%02x", &value);
			if (n != 1 || value > 0xff)
				return false;
			sPackedHex += (unsigned char) value;
		}

		return true;
	}

	CString HMAC_SHA256(const CString& sKey, const CString& sData) {
		CString sRealKey;
		if (sKey.length() > 64)
			PackHex(sKey.SHA256(), sRealKey);
		else
			sRealKey = sKey;

		CString sOuterKey, sInnerKey;
		CString::size_type iKeyLength = sRealKey.length();
		for (unsigned int i = 0; i < 64; i++) {
			char r = (i < iKeyLength ? sRealKey[i] : '\0');
			sOuterKey += r ^ 0x5c;
			sInnerKey += r ^ 0x36;
		}

		CString sInnerHash;
		PackHex(CString(sInnerKey + sData).SHA256(), sInnerHash);
		return CString(sOuterKey + sInnerHash).SHA256();
	}

/* Settings */
	CString m_sUsername;
	CString m_sPassword;
	bool    m_bUseCloakedHost;
	bool    m_bUseChallenge;
	bool    m_bRequestPerms;
	bool    m_bJoinOnInvite;
	bool    m_bJoinAfterCloaked;

	void SetUsername(const CString& sUsername) {
		m_sUsername = sUsername;
		SetNV("Username", sUsername);
	}

	void SetPassword(const CString& sPassword) {
		m_sPassword = sPassword;
		SetNV("Password", sPassword);
	}

	void SetUseCloakedHost(const bool bUseCloakedHost) {
		m_bUseCloakedHost = bUseCloakedHost;
		SetNV("UseCloakedHost", CString(bUseCloakedHost));

		if (!m_bCloaked && m_bUseCloakedHost && IsIRCConnected())
			Cloak();
	}

	void SetUseChallenge(const bool bUseChallenge) {
		m_bUseChallenge = bUseChallenge;
		SetNV("UseChallenge", CString(bUseChallenge));
	}

	void SetRequestPerms(const bool bRequestPerms) {
		m_bRequestPerms = bRequestPerms;
		SetNV("RequestPerms", CString(bRequestPerms));
	}

	void SetJoinOnInvite(const bool bJoinOnInvite) {
		m_bJoinOnInvite = bJoinOnInvite;
		SetNV("JoinOnInvite", CString(bJoinOnInvite));
	}

	void SetJoinAfterCloaked(const bool bJoinAfterCloaked) {
		m_bJoinAfterCloaked = bJoinAfterCloaked;
		SetNV("JoinAfterCloaked", CString(bJoinAfterCloaked));
	}
};

template<> void TModInfo<CQModule>(CModInfo& Info) {
	Info.SetWikiPage("Q");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("Please provide your username and password for Q.");
}

NETWORKMODULEDEFS(CQModule, "Auths you with QuakeNet's Q bot.")
