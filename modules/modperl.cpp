#ifdef HAVE_PERL
#include "main.h"
#include "User.h"
#include "Nick.h"
#include "Modules.h"
#include "Chan.h"
#include "FileUtils.h"

// perl stuff
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#define NICK( a ) a.GetNickMask()
#define CHAN( a ) a.GetName()
#define ZNCEvalCB "ZNC::Eval"

class PString : public CString 
{
public:
	enum EType
	{
		STRING,
		INT,
		UINT,
		NUM,
		BOOL
	};
	
	PString() : CString() { m_eType = STRING; }
	PString( const char* c ) : CString(c) { m_eType = STRING; }
	PString( const CString& s ) : CString(s) { m_eType = STRING; }
	PString( int i ) : CString( PString::ToString( i ) ) { m_eType = INT; }
	PString( u_int i ) : CString( PString::ToString( i ) ) { m_eType = UINT; }
	PString( long i ) : CString( PString::ToString( i ) ) { m_eType = INT; }
	PString( u_long i ) : CString( PString::ToString( i ) ) { m_eType = UINT; }
	PString( long long i ) : CString( PString::ToString( (long long)i ) ) { m_eType = INT; }
	PString( unsigned long long i ) : CString( PString::ToString( i ) ) { m_eType = UINT; }
	PString( double i ) : CString( PString::ToString( i ) ) { m_eType = NUM; }
	PString( bool b ) : CString( ( b ? "1" : "0" ) ) { m_eType = BOOL; }

	virtual ~PString() {}

	
	EType GetType() const { return( m_eType ); }
	void SetType( EType e ) { m_eType = e; }
	
	SV * GetSV( bool bMakeMortal = true ) const
	{
		SV *pSV = NULL;
		switch( GetType() )
		{
			case NUM:
				pSV = newSVnv( ToDouble() );
				break;
			case INT:
				pSV = newSViv( ToLongLong() );
				break;
			case UINT:
			case BOOL:
				pSV = newSVuv( ToULongLong() );
				break;
			case STRING:
			default:
				pSV = newSVpv( c_str(), length() );
				break;
		}

		if ( bMakeMortal )
			pSV = sv_2mortal( pSV );

		return( pSV );
	}

private:
	
	EType	m_eType;
};


class CPerlHash : public map< CString, PString >
{
public:
	
	HV *GetHash()
	{
		HV *pHash = newHV();
		sv_2mortal( (SV *) pHash );
		for( CPerlHash::iterator it = this->begin(); it != this->end(); it++ )
		{
			SV *pSV = it->second.GetSV( false );
			hv_store( pHash, it->first.c_str(), it->first.length(), pSV, 0);
		}

		return( pHash );
	}
};

typedef vector< PString > VPString;

class CModPerl;
static CModPerl *g_ModPerl = NULL;

class CModPerl : public CModule 
{
public:
	MODCONSTRUCTOR( CModPerl ) 
	{
		g_ModPerl = this;
		m_pPerl = NULL;
	}

	virtual ~CModPerl() 
	{
		if ( m_pPerl )
		{
			CBNone( "Shutdown" );
			PerlInterpShutdown();
		}
		g_ModPerl = NULL;
	}

	void PerlInterpShutdown()
	{
		perl_destruct( m_pPerl );
		perl_free( m_pPerl );
		m_pPerl = NULL;
	}

	void SetupZNCScript()
	{
		CString sModule = m_pUser->FindModPath( "modperl.pm" );
		if ( !sModule.empty() )
		{
			CString sBuffer, sScript;
			CFile cFile( sModule );
			if ( ( cFile.Exists() ) && ( cFile.Open( O_RDONLY ) ) )
			{
				while( cFile.ReadLine( sBuffer ) )
					sScript += sBuffer;	
				cFile.Close();

				eval_pv( sScript.c_str(), FALSE );
			}
		}
	}

	void DumpError( const CString & sError )
	{
		CString sTmp = sError;
		for( CString::size_type a = 0; a < sTmp.size(); a++ )
		{
			if ( isspace( sTmp[a] ) )
				sTmp[a] = ' ';
		}
		PutModule( sTmp );
	}

	CUser * GetUser() { return( m_pUser ); }

	virtual bool OnLoad( const CString & sArgs );
	virtual bool OnBoot() { return( ( CBNone( "OnBoot" ) == CONTINUE ) ); }
	virtual void OnUserAttached() {  CBNone( "OnUserAttached" ); }
	virtual void OnUserDetached() {  CBNone( "OnUserDetached" ); }
	virtual void OnIRCDisconnected() {  CBNone( "OnIRCDisconnected" ); }
	virtual void OnIRCConnected() {  CBNone( "OnIRCConnected" ); }

	virtual EModRet OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, 
		const CString& sFile, unsigned long uFileSize);

	virtual void OnOp(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange)
	{
		CBFour( "OnOp", NICK( OpNick ), NICK( Nick ), CHAN( Channel ), bNoChange );
	}
	virtual void OnDeop(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange)
	{
		CBFour( "OnDeop", NICK( OpNick ), NICK( Nick ), CHAN( Channel ), bNoChange );
	}
	virtual void OnVoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange)
	{
		CBFour( "OnVoice", NICK( OpNick ), NICK( Nick ), CHAN( Channel ), bNoChange );
	}
	virtual void OnDevoice(const CNick& OpNick, const CNick& Nick, const CChan& Channel, bool bNoChange)
	{
		CBFour( "OnDevoice", NICK( OpNick ), NICK( Nick ), CHAN( Channel ), bNoChange );
	}
	virtual void OnRawMode(const CNick& Nick, const CChan& Channel, const CString& sModes, const CString& sArgs)
	{
		CBFour( "OnRawMode", NICK( Nick ), CHAN( Channel ), sModes, sArgs  );
	}
	virtual EModRet OnUserRaw(CString& sLine) { return( CBSingle( "OnUserRaw", sLine ) ); }
	virtual EModRet OnRaw(CString& sLine) { return( CBSingle( "OnRaw", sLine ) ); }

	virtual void OnModCommand(const CString& sCommand) 
	{ 
		if ( CBSingle( "OnModCommand", sCommand ) == 0 )
			Eval( sCommand );
	}
	virtual void OnModNotice(const CString& sMessage) { CBSingle( "OnModNotice", sMessage ); }
	virtual void OnModCTCP(const CString& sMessage) { CBSingle( "OnModCTCP", sMessage ); }

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans)
	{
		VPString vsArgs;
		vsArgs.push_back( Nick.GetNickMask() );
		vsArgs.push_back( sMessage );
		for( vector<CChan*>::size_type a = 0; a < vChans.size(); a++ )
			vsArgs.push_back( vChans[a]->GetName() );

		CallBack( "OnQuit", vsArgs );
	}

	virtual void OnNick(const CNick& Nick, const CString& sNewNick, const vector<CChan*>& vChans)
	{
		VPString vsArgs;
		vsArgs.push_back( Nick.GetNickMask() );
		vsArgs.push_back( sNewNick );
		for( vector<CChan*>::size_type a = 0; a < vChans.size(); a++ )
			vsArgs.push_back( vChans[a]->GetName() );

		CallBack( "OnNick", vsArgs );
	}

	virtual void OnKick(const CNick& Nick, const CString& sOpNick, const CChan& Channel, const CString& sMessage)
	{
		CBFour( "OnKick", NICK( Nick ), sOpNick, CHAN( Channel ), sMessage );
	}

	virtual void OnJoin(const CNick& Nick, const CChan& Channel) { CBDouble( "OnJoin", NICK( Nick ), CHAN( Channel ) ); }
	virtual void OnPart(const CNick& Nick, const CChan& Channel) { CBDouble( "OnPart", NICK( Nick ), CHAN( Channel ) ); }

	virtual EModRet OnUserCTCPReply(const CNick& Nick, CString& sMessage) 
	{ 
		return CBDouble( "OnUserCTCPReply", NICK( Nick ), sMessage ); 
	}
	virtual EModRet OnCTCPReply(const CNick& Nick, CString& sMessage)
	{
		return CBDouble( "OnCTCPReply", NICK( Nick ), sMessage ); 
	}
	virtual EModRet OnUserCTCP(const CString& sTarget, CString& sMessage)
	{
		return CBDouble( "OnUserCTCP", sTarget, sMessage ); 
	}
	virtual EModRet OnPrivCTCP(const CNick& Nick, CString& sMessage)
	{
		return CBDouble( "OnPrivCTCP", NICK( Nick ), sMessage ); 
	}
	virtual EModRet OnChanCTCP(const CNick& Nick, const CChan& Channel, CString& sMessage)
	{
		return CBTriple( "OnChanCTCP", NICK( Nick ), CHAN( Channel ), sMessage );
	}
	virtual EModRet OnUserMsg(const CString& sTarget, CString& sMessage)
	{
		return CBDouble( "OnUserMsg", sTarget, sMessage );
	}
	virtual EModRet OnPrivMsg(const CNick& Nick, CString& sMessage)
	{
		return CBDouble( "OnPrivMsg", NICK( Nick ), sMessage );
	}

	virtual EModRet OnChanMsg( const CNick& Nick, const CChan & Channel, CString & sMessage )
	{
		return( CBTriple( "OnChanMsg", NICK( Nick ), CHAN( Channel ), sMessage ) );
	}
	virtual EModRet OnUserNotice(const CString& sTarget, CString& sMessage)
	{
		return CBDouble( "OnUserNotice", sTarget, sMessage );
	}
	virtual EModRet OnPrivNotice(const CNick& Nick, CString& sMessage)
	{
		return CBDouble( "OnPrivNotice", NICK( Nick ), sMessage );
	}
	virtual EModRet OnChanNotice(const CNick& Nick, const CChan& Channel, CString& sMessage)
	{
		return( CBTriple( "OnChanNotice", NICK( Nick ), CHAN( Channel ), sMessage ) );
	}

	EModRet CallBack( const PString & sHookName, const VPString & vsArgs, bool bLocal = false );
	EModRet CBNone( const PString & sHookName )
	{
		VPString vsArgs;
		return( CallBack( sHookName, vsArgs ) );
	}

	template <class A>
	inline EModRet CBSingle( const PString & sHookName, const A & a )
	{
		VPString vsArgs;
		vsArgs.push_back( a );
		return( CallBack( sHookName, vsArgs ) );
	}
	template <class A, class B>
	inline EModRet CBDouble( const PString & sHookName, const A & a, const B & b )
	{
		VPString vsArgs;
		vsArgs.push_back( a );
		vsArgs.push_back( b );
		return( CallBack( sHookName, vsArgs ) );
	}
	template <class A, class B, class C>
	inline EModRet CBTriple( const PString & sHookName, const A & a, const B & b, const C & c )
	{
		VPString vsArgs;
		vsArgs.push_back( a );
		vsArgs.push_back( b );
		vsArgs.push_back( c );
		return( CallBack( sHookName, vsArgs ) );
	}
	template <class A, class B, class C, class D>
	inline EModRet CBFour( const PString & sHookName, const A & a, const B & b, const C & c, const D & d )
	{
		VPString vsArgs;
		vsArgs.push_back( a );
		vsArgs.push_back( b );
		vsArgs.push_back( c );
		vsArgs.push_back( d );
		return( CallBack( sHookName, vsArgs ) );
	}

	bool Eval( const CString & sScript, const CString & sFuncName = ZNCEvalCB );

	virtual EModRet OnStatusCommand( const CString& sLine )
	{
		CString sCommand = sLine.Token( 0 );

		if ( ( sCommand == "loadmod" ) || ( sCommand == "unloadmod" ) || ( sCommand == "reloadmod" ) )
		{
			CString sModule = sLine.Token( 1 );
			if ( sModule.find( ".pm" ) != CString::npos )
			{
				if ( sCommand == "loadmod" )
					LoadPerlMod( sModule );
				else if ( sCommand == "unloadmod" )
					UnLoadPerlMod( sModule );
				else
					PutModule( "Perl modules can not be reloaded one at a time, you have to reload the interpreter to pick up code changes (reloadmod modperl)" );
				return( HALT );
			}
		}
		return( CONTINUE );
	}

private:
	void LoadPerlMod( const CString & sModule );
	void UnLoadPerlMod( const CString & sModule );

	PerlInterpreter	*m_pPerl;

};

MODULEDEFS( CModPerl )



//////////////////////////////// PERL GUTS //////////////////////////////

XS(XS_ZNC_PutIRC)
{
	dXSARGS;
	if ( items != 1 )
		Perl_croak( aTHX_ "Usage: PutIRC( sLine )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CString sLine = (char *)SvPV(ST(0),PL_na);
			g_ModPerl->PutIRC( sLine );
		}
		PUTBACK;
	}
}

	
XS(XS_ZNC_PutUser)
{
	dXSARGS;
	if ( items != 1 )
		Perl_croak( aTHX_ "Usage: PutUser( sLine )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CString sLine = (char *)SvPV(ST(0),PL_na);
			g_ModPerl->PutUser( sLine );
		}
		PUTBACK;
	}
}

	
XS(XS_ZNC_PutStatus)
{
	dXSARGS;
	if ( items != 1 )
		Perl_croak( aTHX_ "Usage: PutStatus( sLine )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CString sLine = (char *)SvPV(ST(0),PL_na);
			g_ModPerl->PutStatus( sLine );
		}
		PUTBACK;
	}
}

XS(XS_ZNC_PutModule)
{
	dXSARGS;
	if ( items < 1 )
		Perl_croak( aTHX_ "Usage: PutModule( sLine, sIdent, sHost )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CString sLine, sIdent, sHost;
			sLine = (char *)SvPV(ST(0),PL_na);
			if ( items >= 2 )
				sIdent = (char *)SvPV(ST(1),PL_na);
			if ( items >= 3 )
				sHost = (char *)SvPV(ST(2),PL_na);
			g_ModPerl->PutModule( sLine, sIdent, sHost );
		}
		PUTBACK;
	}
}

XS(XS_ZNC_PutModNotice)
{
	dXSARGS;
	if ( items < 1 )
		Perl_croak( aTHX_ "Usage: PutModNotice( sLine, sIdent, sHost )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CString sLine, sIdent, sHost;
			sLine = (char *)SvPV(ST(0),PL_na);
			if ( items >= 2 )
				sIdent = (char *)SvPV(ST(1),PL_na);
			if ( items >= 3 )
				sHost = (char *)SvPV(ST(2),PL_na);
			g_ModPerl->PutModNotice( sLine, sIdent, sHost );
		}
		PUTBACK;
	}
}


XS(XS_ZNC_GetNicks)
{
	dXSARGS;
	if ( items != 1 )
		Perl_croak( aTHX_ "Usage: GetNicks( sChan )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CString sChan = (char *)SvPV(ST(0),PL_na);
			CUser * pUser = g_ModPerl->GetUser();
			CChan * pChan = pUser->FindChan( sChan );
			if ( !pChan )
				XSRETURN( 0 );
			
			const map< CString,CNick* > & mscNicks = pChan->GetNicks();

			for( map< CString,CNick* >::const_iterator it = mscNicks.begin(); it != mscNicks.end(); it++ )
			{
				CNick & cNick = *(it->second);
				CPerlHash cHash;
				cHash["Nick"] = cNick.GetNick();
				cHash["Ident"] = cNick.GetIdent();
				cHash["Host"] = cNick.GetHost();
				cHash["Perms"] = cNick.GetPermStr();
				XPUSHs( newRV_noinc( (SV*)cHash.GetHash() ) );
			}
		}
		PUTBACK;
	}
}

XS(XS_ZNC_GetString)
{
	dXSARGS;
	
	if ( items != 1 )
		Perl_croak( aTHX_ "Usage: GetString( sName )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CUser * pUser = g_ModPerl->GetUser();
			PString sReturn;
			CString sName = (char *)SvPV(ST(0),PL_na);

			if( sName == "UserName" ) sReturn = pUser->GetUserName();
			else if ( sName == "Nick" ) sReturn = pUser->GetNick();
			else if ( sName == "AltNick" ) sReturn = pUser->GetAltNick();
			else if ( sName == "Ident" ) sReturn = pUser->GetIdent();
			else if ( sName == "RealName" ) sReturn = pUser->GetRealName();
			else if ( sName == "VHost" ) sReturn = pUser->GetVHost();
			else if ( sName == "Pass" ) sReturn = pUser->GetPass();
			else if ( sName == "CurPath" ) sReturn = pUser->GetCurPath();
			else if ( sName == "DLPath" ) sReturn = pUser->GetDLPath();
			else if ( sName == "ModPath" ) sReturn = pUser->GetModPath();
			else if ( sName == "HomePath" ) sReturn = pUser->GetHomePath();
			else if ( sName == "DataPath" ) sReturn = pUser->GetDataPath();
			else if ( sName == "StatusPrefix" ) sReturn = pUser->GetStatusPrefix();
			else if ( sName == "DefaultChanModes" ) sReturn = pUser->GetDefaultChanModes();
			else if ( sName == "IRCServer" ) sReturn = pUser->GetIRCServer();
			else
				XSRETURN( 0 );

			XPUSHs( sReturn.GetSV() );
		}
		PUTBACK;
	}
}

/////////// supporting functions from within module

bool CModPerl::Eval( const CString & sScript, const CString & sFuncName )
{
	dSP;
	ENTER;
	SAVETMPS;

	PUSHMARK( SP );
	XPUSHs( sv_2mortal( newSVpv( sScript.c_str(), sScript.length() ) ) );
	PUTBACK;

	SPAGAIN;

	call_pv( sFuncName.c_str(), G_EVAL|G_KEEPERR|G_VOID|G_DISCARD );

	bool bReturn = true;

	if ( SvTRUE( ERRSV ) ) 
	{ 
		DumpError( SvPV( ERRSV, PL_na) );
		bReturn = false;
	}
	PUTBACK;
	FREETMPS;
	LEAVE;

	return( bReturn );
}

CModPerl::EModRet CModPerl::CallBack( const PString & sHookName, const VPString & vsArgs, bool bLocal )
{
	if ( !m_pPerl )
		return( CONTINUE );
	
	dSP;
	ENTER;
	SAVETMPS;

	PUSHMARK( SP );

	if ( !bLocal )
		XPUSHs( sHookName.GetSV() );

	for( VPString::size_type a = 0; a < vsArgs.size(); a++ )
		XPUSHs( vsArgs[a].GetSV() );

	PUTBACK;

	int iCount = call_pv( ( !bLocal ? "ZNC::CallFunc" : sHookName.c_str() ), G_EVAL|G_SCALAR );

	SPAGAIN;
	int iRet = CONTINUE;

	if ( SvTRUE( ERRSV ) ) 
	{ 
		CString sError = SvPV( ERRSV, PL_na);
		DumpError( sHookName + ": " + sError );

	} else
	{
		if ( iCount == 1 )
			iRet = POPi;
	}

	PUTBACK;
	FREETMPS;
	LEAVE;

	return( (CModPerl::EModRet)iRet );
}

////////////////////// Events ///////////////////

// special case this, required for perl modules that are dynamic
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

bool CModPerl::OnLoad( const CString & sArgs ) 
{
	m_pPerl = perl_alloc();
	perl_construct( m_pPerl );
	const char *pArgv[] = { "", "-e", "0", "-T", "-w" };

	if ( perl_parse( m_pPerl, NULL, 2, (char **)pArgv, (char **)NULL ) != 0 )
	{
		perl_free( m_pPerl );
		m_pPerl = NULL;
		return( false );
	}

#ifdef PERL_EXIT_DESTRUCT_END
	PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
#endif /* PERL_EXIT_DESTRUCT_END */

	char *file = __FILE__;

	newXS( "DynaLoader::boot_DynaLoader", boot_DynaLoader, (char *)file );
	newXS( "ZNC::PutIRC", XS_ZNC_PutIRC, (char *)file );
	newXS( "ZNC::PutUser", XS_ZNC_PutUser, (char *)file );
	newXS( "ZNC::PutStatus", XS_ZNC_PutStatus, (char *)file );
	newXS( "ZNC::PutModule", XS_ZNC_PutModule, (char *)file );
	newXS( "ZNC::PutModNotice", XS_ZNC_PutModNotice, (char *)file );
	newXS( "ZNC::GetNicks", XS_ZNC_GetNicks, (char *)file );
	newXS( "ZNC::GetString", XS_ZNC_GetString, (char *)file );
	
	// this sets up the eval CB that we call from here on out. this way we can grab the error produced
	SetupZNCScript();

	HV *pZNCSpace = get_hv ( "ZNC::", TRUE);

	if ( !pZNCSpace )
		return( false );

	newCONSTSUB( pZNCSpace, "CONTINUE", newSViv( CONTINUE ) );
	newCONSTSUB( pZNCSpace, "HALT", newSViv( HALT ) );
	newCONSTSUB( pZNCSpace, "HALTMODS", newSViv( HALTMODS ) );
	newCONSTSUB( pZNCSpace, "HALTCORE", newSViv( HALTCORE ) );

	for( u_int a = 0; a < 255; a++ )
	{
		CString sModule = sArgs.Token( a );
		if ( sModule.empty() )
			break;

		LoadPerlMod( sModule );
	}

	return( true );
}

void CModPerl::LoadPerlMod( const CString & sModule )
{
	CString sModPath = m_pUser->FindModPath( sModule );
	if ( sModPath.empty() )
		PutModule( "No such module " + sModule );
	else
	{
		PutModule( "Using " + sModPath );
		Eval( "ZNC::LoadMod( '" + m_pUser->GetUserName() + "', '" + sModPath + "');" );
	}
}

void CModPerl::UnLoadPerlMod( const CString & sModule )
{
	Eval( "ZNC::UnLoadMod( '" + m_pUser->GetUserName() + "', '" + sModule + "');" );
}


CModPerl::EModRet CModPerl::OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, 
		const CString& sFile, unsigned long uFileSize)
{
	VPString vsArgs;
	vsArgs.push_back( NICK( RemoteNick ) );
	vsArgs.push_back( uLongIP );
	vsArgs.push_back( uPort );
	vsArgs.push_back( sFile );
	
	return( CallBack( "OnDCCUserSend", vsArgs ) );
}

#endif /* HAVE_PERL */
