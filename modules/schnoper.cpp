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

#include <znc/User.h>
#include <znc/IRCNetwork.h>

// Schnoper module.  stores your oper password in memory when you first oper, and
// uses it to re-oper on reconnection if you drop for some reason

class CSchnoper : public CModule 
{
private:
	CString      m_sOperPassword;
	CString      m_sOperUser;
	bool	     m_bAutoOper;

public:
	MODCONSTRUCTOR(CSchnoper) 
	{
		m_sOperPassword  = "";
		m_sOperUser  = "";
		m_bAutoOper      = false;

		AddHelpCommand();
		AddCommand("Forget", static_cast<CModCommand::ModCmdFunc>(&CSchnoper::ForgetCommand), "", "Forgets a cached oper username and password, if one exists.");
	}

	void ForgetCommand(const CString& sLine)
	{
		for (size_t i = 0; i < m_sOperUser.length(); ++i) m_sOperUser[i] = '\0';
		for (size_t i = 0; i < m_sOperPassword.length(); ++i) m_sOperPassword[i] = '\0';
		m_sOperUser.clear();
		m_sOperPassword.clear();
		m_bAutoOper = false;
		PutModule("Deleted stored username and password.");
	}

	virtual ~CSchnoper() 
	{
	}

	bool OnLoad(const CString& sArgs, CString& sMessage)
	{
		if (GetType() != CModInfo::NetworkModule) {
			sMessage = "You can only load this module in network mode.";
			return false;
		}
		return true;
	}

	virtual void OnIRCConnected() 
	{
		if (m_bAutoOper && !m_sOperPassword.empty() && !m_sOperUser.empty())
		{
			PutModule("Resending cached OPER line...");
			PutIRC("OPER " + m_sOperUser + " :" + m_sOperPassword);
		}
	}

	virtual EModRet OnUserRaw(CString &sLine)
	{
		if (sLine.Token(0).AsUpper() == "OPER")
		{
			PutModule("Storing oper password in case of reconnect.  To erase it, use 'forget'.");
			m_bAutoOper = true;
			m_sOperUser = sLine.Token(1);
			m_sOperPassword = sLine.Token(2, true);
			if (m_sOperPassword.length() > 0 && m_sOperPassword[0] == ':') m_sOperPassword = m_sOperPassword.substr(1);
		}
		return CONTINUE;
	}

};

template<> void TModInfo<CSchnoper>(CModInfo& Info) {
	Info.SetWikiPage("schnoper");
	Info.SetHasArgs(false);
	Info.SetArgsHelpText("");
}

NETWORKMODULEDEFS(CSchnoper, "Stores your oper credentials in RAM after you oper, and reopers if your connection is lost..")
