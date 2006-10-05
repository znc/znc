#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "Utils.h"
#include <pwd.h>
#include <map>
#include <vector>

class CPerform : public CModule
{
public:
	MODCONSTRUCTOR(CPerform)
	{
	}
	
	virtual ~CPerform()
	{
	}

	virtual bool OnLoad(const CString& sArgs)
	{
		GetNV("Perform").Split("\n", m_vPerform, false);
		
		return true;
	}

	virtual void OnModCommand( const CString& sCommand )
	{
		CString sCmdName = sCommand.Token(0).AsLower();
		if(sCmdName == "add")
		{
			CString sPerf = sCommand.Token(1, true);
			if(sPerf.Left(1) == "/")
				sPerf.LeftChomp();

			if(sPerf.Token(0).AsUpper() == "MSG") {
				sPerf = "PRIVMSG " + sPerf.Token(1, true);
			}

			if(sPerf.Token(0).AsUpper() == "PRIVMSG" ||
				sPerf.Token(0).AsUpper() == "NOTICE") {
				sPerf = sPerf.Token(0) + " " + sPerf.Token(1)
					+ " :" + sPerf.Token(2, true);
			}
			m_vPerform.push_back(sPerf);
			PutModule("Added!");
			Save();
		} else if(sCmdName == "del")
		{
			u_int iNum = atoi( sCommand.Token(1, true).c_str() );
			if ( iNum > m_vPerform.size() || iNum <= 0 )
			{
				PutModule( "Illegal # Requested");
				return;
			}
			else
			{
				m_vPerform.erase( m_vPerform.begin() + iNum - 1 );
				PutModule( "Command Erased.");
			}
			Save();
		} else if(sCmdName == "list")
		{
			int i = 1;
			for(VCString::iterator it = m_vPerform.begin(); it != m_vPerform.end(); it++, i++)
			{
				PutModule(CString( i ) + ": " + *it);
			}
			PutModule(" -- End of List");
		} else if(sCmdName == "save")
		{
			if(Save())
				PutModule("Saved!");
			else
				PutModule("Error");
		}else
		{
			PutModule( "Commands: add <command>, del <nr>, list, save");
		}
	}

	virtual void OnIRCConnected()
	{
		for( VCString::iterator it = m_vPerform.begin();
			it != m_vPerform.end();  it++)
		{
			PutIRC(*it);
		}
	}

private:
	bool Save()
	{
		CString sBuffer = "";
		
		for(VCString::iterator it = m_vPerform.begin(); it != m_vPerform.end(); it++)
		{
			sBuffer += *it + "\n";
		}
		SetNV("Perform", sBuffer);
		
		return true;
	}

	VCString	m_vPerform;
};

MODULEDEFS(CPerform, "Adds perform capabilities")
