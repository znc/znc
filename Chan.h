#ifndef _CHAN_H
#define _CHAN_H

#include "Nick.h"
#include <vector>
#include <map>
using std::vector;
using std::map;

// Forward Declarations
class CUser;
// !Forward Declarations

class CChan {
public:
	typedef enum {
		Private		= 1 << 0,
		Secret		= 1 << 1,
		NoMessages	= 1 << 2,
		Moderated	= 1 << 3,
		OpTopic		= 1 << 4,
		InviteOnly	= 1 << 5,
		Key			= 1 << 6
	} EMode;

	CChan(const string& sName, CUser* pUser, unsigned int uBufferCount = 0) {
		m_sName = sName;
		m_pUser = pUser;
		m_bKeepBuffer = false;
		m_uBufferCount = uBufferCount;
		Reset();
	}
	virtual ~CChan() {
		ClearNicks();
	}

	void Reset();
	void Joined();
	void Cycle() const;

	void IncClientRequests();
	bool DecClientRequests(); 

	bool Who();
	void OnWho(const string& sNick, const string& sIdent, const string& sHost);

	// Modes
	void SetModes(const string& s);
	void ModeChange(const string& sModes, const string& sNick = "");
	void OnOp(const string& sOpNick, const string& sNick, bool bOpped);
	void OnVoice(const string& sOpNick, const string& sNick, bool bVoiced);
	string GetModeString() const;
	string GetModeArg(string& sArgs) const;
	// !Modes

	// Nicks
	void ClearNicks();
	CNick* FindNick(const string& sNick) const;
	int AddNicks(const string& sNicks);
	bool AddNick(const string& sNick);
	bool RemNick(const string& sNick);
	bool ChangeNick(const string& sOldNick, const string& sNewNick);
	// !Nicks

	// Buffer
	int AddBuffer(const string& sLine);
	void ClearBuffer();
	// !Buffer

	// Setters
	void SetIsOn(bool b) { m_bIsOn = b; if (!b) { Reset(); } else { Joined(); } }
	void SetKey(const string& s) { m_sKey = s; }
	void SetTopic(const string& s) { m_sTopic = s; }
	void SetDefaultModes(const string& s) { m_sDefaultModes = s; }
	void IncOpCount() { m_uOpCount++; }
	void DecOpCount() { m_uOpCount -= (m_uOpCount > 0); }
	void IncVoiceCount() { m_uVoiceCount++; }
	void DecVoiceCount() { m_uVoiceCount -= (m_uVoiceCount > 0); }
	void SetBufferCount(unsigned int u) { m_uBufferCount = u; }
	void SetKeepBuffer(bool b) { m_bKeepBuffer = b; }
	void SetWhoDone(bool b = true) { m_bWhoDone = b; }
	// !Setters

	// Getters
	const bool IsOn() const { return m_bIsOn; }
	const string& GetName() const { return m_sName; }
	unsigned int GetModes() const { return m_uModes; }
	const string& GetKey() const { return m_sKey; }
	unsigned int GetLimit() const { return m_uLimit; }
	const string& GetTopic() const { return m_sTopic; }
	const string& GetDefaultModes() const { return m_sDefaultModes; }
	const vector<string>& GetBans() const { return m_vsBans; }
	const vector<string>& GetBuffer() const { return m_vsBuffer; }
	const map<string,CNick*>& GetNicks() const { return m_msNicks; }
	unsigned int GetNickCount() const { return m_msNicks.size(); }
	unsigned int GetOpCount() const { return m_uOpCount; }
	unsigned int GetVoiceCount() const { return m_uVoiceCount; }
	unsigned int GetBufferCount() const { return m_uBufferCount; }
	bool HasMode(EMode eMode) const { return (m_uModes & eMode); }
	bool KeepBuffer() const { return m_bKeepBuffer; }
	// !Getters
private:
protected:
	bool				m_bIsOn;
	bool				m_bWhoDone;
	bool				m_bKeepBuffer;
	string				m_sName;
	string				m_sKey;
	string				m_sTopic;
	CUser*				m_pUser;
	unsigned int		m_uLimit;
	unsigned int		m_uModes;
	string				m_sDefaultModes;
	vector<string>		m_vsBans;
	map<string,CNick*>	m_msNicks;	// Todo: make this caseless (irc style)
	unsigned int		m_uBufferCount;
	unsigned int		m_uOpCount;
	unsigned int		m_uVoiceCount;
	unsigned int		m_uClientRequests;	// Used to tell how many times a client tried to join this chan
	vector<string>		m_vsBuffer;
};

#endif // !_CHAN_H
