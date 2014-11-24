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

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>

using std::map;

class CFloodDetachMod : public CModule {
public:
	MODCONSTRUCTOR(CFloodDetachMod) {
		m_iThresholdSecs = 0;
		m_iThresholdMsgs = 0;

		AddHelpCommand();
		AddCommand("Show", static_cast<CModCommand::ModCmdFunc>(&CFloodDetachMod::ShowCommand), "");
		AddCommand("Secs", static_cast<CModCommand::ModCmdFunc>(&CFloodDetachMod::SecsCommand), "[<limit>]");
		AddCommand("Lines", static_cast<CModCommand::ModCmdFunc>(&CFloodDetachMod::LinesCommand), "[<limit>]");
		AddCommand("Silent", static_cast<CModCommand::ModCmdFunc>(&CFloodDetachMod::SilentCommand), "[yes|no]");
	}

	~CFloodDetachMod() {
	}

	void Save() {
		// We save the settings twice because the module arguments can
		// be more easily edited via webadmin, while the SetNV() stuff
		// survives e.g. /msg *status reloadmod ctcpflood.
		SetNV("secs", CString(m_iThresholdSecs));
		SetNV("msgs", CString(m_iThresholdMsgs));

		SetArgs(CString(m_iThresholdMsgs) + " " + CString(m_iThresholdSecs));
	}

	bool OnLoad(const CString& sArgs, CString& sMessage) override {
		m_iThresholdMsgs = sArgs.Token(0).ToUInt();
		m_iThresholdSecs = sArgs.Token(1).ToUInt();

		if (m_iThresholdMsgs == 0 || m_iThresholdSecs == 0) {
			m_iThresholdMsgs = GetNV("msgs").ToUInt();
			m_iThresholdSecs = GetNV("secs").ToUInt();
		}

		if (m_iThresholdSecs == 0)
			m_iThresholdSecs = 2;
		if (m_iThresholdMsgs == 0)
			m_iThresholdMsgs = 5;

		Save();

		return true;
	}

	void OnIRCDisconnected() override {
		m_chans.clear();
	}

	void Cleanup() {
		Limits::iterator it;
		time_t now = time(NULL);

		for (it = m_chans.begin(); it != m_chans.end(); ++it) {
			// The timeout for this channel did not expire yet?
			if (it->second.first + (time_t)m_iThresholdSecs >= now)
				continue;

			CChan *pChan = GetNetwork()->FindChan(it->first);
			if (it->second.second >= m_iThresholdMsgs
					&& pChan && pChan->IsDetached()) {
				// The channel is detached and it is over the
				// messages limit. Since we only track those
				// limits for non-detached channels or for
				// channels which we detached, this means that
				// we detached because of a flood.

				if (!GetNV("silent").ToBool()) {
					PutModule("Flood in [" + pChan->GetName() + "] is over, "
							"re-attaching...");
				}
				// No buffer playback, makes sense, doesn't it?
				pChan->ClearBuffer();
				pChan->JoinUser();
			}

			Limits::iterator it2 = it++;
			m_chans.erase(it2);

			// Without this Bad Things (tm) could happen
			if (it == m_chans.end())
				break;
		}
	}

	void Message(CChan& Channel) {
		Limits::iterator it;
		time_t now = time(NULL);

		// First: Clean up old entries and reattach where necessary
		Cleanup();

		it = m_chans.find(Channel.GetName());

		if (it == m_chans.end()) {
			// We don't track detached channels
			if (Channel.IsDetached())
				return;

			// This is the first message for this channel, start a
			// new timeout.
			std::pair<time_t, unsigned int> tmp(now, 1);
			m_chans[Channel.GetName()] = tmp;
			return;
		}

		// No need to check it->second.first (expiry time), since
		// Cleanup() would have removed it if it was expired.

		if (it->second.second >= m_iThresholdMsgs) {
			// The channel already hit the limit and we detached the
			// user, but it is still being flooded, reset the timeout
			it->second.first = now;
			it->second.second++;
			return;
		}

		it->second.second++;

		if (it->second.second < m_iThresholdMsgs)
			return;

		// The channel hit the limit, reset the timeout so that we keep
		// it detached for longer.
		it->second.first = now;

		Channel.DetachUser();
		if (!GetNV("silent").ToBool()) {
			PutModule("Channel [" + Channel.GetName() + "] was "
					"flooded, you've been detached");
		}
	}

	EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
		Message(Channel);
		return CONTINUE;
	}

	// This also catches OnChanAction()
	EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) override {
		Message(Channel);
		return CONTINUE;
	}

	EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) override {
		Message(Channel);
		return CONTINUE;
	}

	EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic) override {
		Message(Channel);
		return CONTINUE;
	}

	void ShowCommand(const CString& sLine) {
		PutModule("Current limit is " + CString(m_iThresholdMsgs) + " lines "
				"in " + CString(m_iThresholdSecs) + " secs.");
	}

	void SecsCommand(const CString& sLine) {
		const CString sArg = sLine.Token(1, true);

		if (sArg.empty()) {
			PutModule("Seconds limit is [" + CString(m_iThresholdSecs) + "]");
		} else {
			m_iThresholdSecs = sArg.ToUInt();
			if (m_iThresholdSecs == 0)
				m_iThresholdSecs = 1;

			PutModule("Set seconds limit to [" + CString(m_iThresholdSecs) + "]");
			Save();
		}
	}

	void LinesCommand(const CString& sLine) {
		const CString sArg = sLine.Token(1, true);

		if (sArg.empty()) {
			PutModule("Lines limit is [" + CString(m_iThresholdMsgs) + "]");
		} else {
			m_iThresholdMsgs = sArg.ToUInt();
			if (m_iThresholdMsgs == 0)
				m_iThresholdMsgs = 2;

			PutModule("Set lines limit to [" + CString(m_iThresholdMsgs) + "]");
			Save();
		}
	}

	void SilentCommand(const CString& sLine) {
		const CString sArg = sLine.Token(1, true);

		if (!sArg.empty()) {
			SetNV("silent", CString(sArg.ToBool()));
		}

		if (GetNV("silent").ToBool()) {
			PutModule("Module messages are disabled");
		} else {
			PutModule("Module messages are enabled");
		}
	}

private:
	typedef map<CString, std::pair<time_t, unsigned int> > Limits;
	Limits m_chans;
	unsigned int m_iThresholdSecs;
	unsigned int m_iThresholdMsgs;
};

template<> void TModInfo<CFloodDetachMod>(CModInfo& Info) {
	Info.SetWikiPage("flooddetach");
	Info.SetHasArgs(true);
	Info.SetArgsHelpText("This user module takes up to two arguments. Arguments are msgs and secs numbers.");
}

USERMODULEDEFS(CFloodDetachMod, "Detach channels when flooded")
