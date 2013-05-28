#include <znc/Modules.h>

using namespace std;

class CAliasModule : public CModule
{
	#define NICKSERV_USED false
	#define NICKSERV_ACCOUNTNAME ""
	#define NICKSERV_PASSWORD ""
	#define NICKSERV_REGAIN_ALIAS_USED false
	
public:
	MODCONSTRUCTOR(CAliasModule) {}
	string BoolToString(bool BoolToConvert)
	{
		if (BoolToConvert)
		{
			return "true";
		}
		return "false";
	}
	void UseNickServRegain()
	{
	}
	virtual bool OnLoad(const CString& Args, CString& Message)
	{
		if (GetNV("NickServRegainAliasUsed").empty())
		{
			SetNV("NickServRegainAliasUsed", BoolToString(NICKSERV_REGAIN_ALIAS_USED));
		}
		if (GetNV("NickServAccountName").empty())
		{
			SetNV("NickServAccountName", NICKSERV_ACCOUNTNAME);
		}
		if (GetNV("NickServPassword").empty())
		{
			SetNV("NickServPassword", NICKSERV_PASSWORD);
		}
		if (GetNV("NickServUsed").empty())
		{
			SetNV("NickServUsed", BoolToString(NICKSERV_USED));
		}
		return true;
	}
	virtual void OnModCommand(const CString& Line)
	{
		CString Command = Line.Token(0);
		if (Command == "help")
		{
			CTable Table;
			Table.AddColumn("Command");
			Table.AddColumn("Description");
			Table.AddColumn("Arguments");
			Table.AddRow();
			Table.SetCell("Command", "setnspassword");
			Table.SetCell("Description", "Sets the NickServ password for the Alias module");
			Table.SetCell("Arguments", "Password");
			Table.AddRow();
			Table.SetCell("Command", "setnsaccountname");
			Table.SetCell("Description", "Sets the NickServ accountname for the Alias module");
			Table.SetCell("Arguments", "AccountName");
			Table.AddRow();
			Table.SetCell("Command", "setnsregainalias");
			Table.SetCell("Description", "Sets the NickServ REGAIN alias on/off");
			Table.SetCell("Arguments", "on|off");
			Table.AddRow();
			Table.SetCell("Command", "nsregain");
			Table.SetCell("Description", "Runs the NickServ REGAIN alias");
			Table.SetCell("Arguments", "None");
			PutModule(Table);
		}
		else if (Command == "setnspassword")
		{
			SetNV("NickServUsed", "true");
			SetNV("NickServPassword", Line.Token(1));
			PutModule("Set NickServ password");
		}
		else if (Command == "setnsaccountname")
		{
			SetNV("NickServUsed", "true");
			SetNV("NickServAccountName", Line.Token(1));
			PutModule("Set NickServ account name to " + Line.Token(1));
		}
		else if (Command == "setnsregainalias")
		{
			if (GetNV("NickServAccountName") == "" || GetNV("NickServPassword") == "" || GetNV("NickServUsed") != "true")
			{
				PutModule("You need to set your NickServ accountname and NickServ password first");
				return;
			}
			if (Line.Token(1) == "on")
			{
				SetNV("NickServRegainAliasUsed", "true");
				PutModule("Set the NickServ REGAIN alias to on");
			}
			else if (Line.Token(1) == "off")
			{
				SetNV("NickServRegainAliasUsed", "false");
				PutModule("Set the NickServ REGAIN alias to off");
			}
			else
			{
				PutModule("Argument 1 not valid");
			}
		}
		else if (Command == "nsregain")
		{
			if (GetNV("NickServRegainAliasUsed") != "true")
			{
				PutModule("The alias NickServ REGAIN is not on");
				return;
			}
			PutIRC("PRIVMSG NickServ :REGAIN " + GetNV("NickServAccountName") + " " + GetNV("NickServPassword"));
		}
	}
};

template<> void TModInfo<CAliasModule>(CModInfo& Info)
{
	Info.SetWikiPage("Alias");
}

NETWORKMODULEDEFS(CAliasModule, "Allows you to set aliases to server commands")