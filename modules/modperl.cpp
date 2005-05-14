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
#define ZNCEvalCB "ZNCEval"

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
	PString( bool b ) : CString( ( b ? "0" : "1" ) ) { m_eType = BOOL; }

	virtual ~PString() {}

	
	EType GetType() const { return( m_eType ); }
	void SetType( EType e ) { m_eType = e; }
	
private:
	
	EType	m_eType;
};

typedef vector< PString > VPString;

class CModPerl;
static CModPerl *g_ModPerl = NULL;

class CModPerlTimer : public CTimer 
{
public:
	CModPerlTimer( CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription ) 
		: CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}
	virtual ~CModPerlTimer() {}

protected:
	virtual void RunJob();

};

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

	virtual bool OnLoad( const CString & sArgs );
	virtual bool OnBoot() { return( !CBNone( "OnBoot" ) ); }
	virtual void OnUserAttached() {  CBNone( "OnUserAttached" ); }
	virtual void OnUserDetached() {  CBNone( "OnUserDetached" ); }
	virtual void OnIRCDisconnected() {  CBNone( "OnIRCDisconnected" ); }
	virtual void OnIRCConnected() {  CBNone( "OnIRCConnected" ); }

	virtual bool OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, 
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
	virtual bool OnUserRaw(CString& sLine) { return( CBSingle( "OnUserRaw", sLine ) ); }
	virtual bool OnRaw(CString& sLine) { return( CBSingle( "OnRaw", sLine ) ); }
	virtual bool OnStatusCommand(const CString& sCommand) { return( CBSingle( "OnStatusCommand", sCommand ) ); }
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

	virtual bool OnUserCTCPReply(const CNick& Nick, CString& sMessage) 
	{ 
		return CBDouble( "OnUserCTCPReply", NICK( Nick ), sMessage ); 
	}
	virtual bool OnCTCPReply(const CNick& Nick, CString& sMessage)
	{
		return CBDouble( "OnCTCPReply", NICK( Nick ), sMessage ); 
	}
	virtual bool OnUserCTCP(const CString& sTarget, CString& sMessage)
	{
		return CBDouble( "OnUserCTCP", sTarget, sMessage ); 
	}
	virtual bool OnPrivCTCP(const CNick& Nick, CString& sMessage)
	{
		return CBDouble( "OnPrivCTCP", NICK( Nick ), sMessage ); 
	}
	virtual bool OnChanCTCP(const CNick& Nick, const CChan& Channel, CString& sMessage)
	{
		return CBTriple( "OnChanCTCP", NICK( Nick ), CHAN( Channel ), sMessage );
	}
	virtual bool OnUserMsg(const CString& sTarget, CString& sMessage)
	{
		return CBDouble( "OnUserMsg", sTarget, sMessage );
	}
	virtual bool OnPrivMsg(const CNick& Nick, CString& sMessage)
	{
		return CBDouble( "OnPrivMsg", NICK( Nick ), sMessage );
	}

	virtual bool OnChanMsg( const CNick& Nick, const CChan & Channel, CString & sMessage )
	{
		return( CBTriple( "OnChanMsg", NICK( Nick ), CHAN( Channel ), sMessage ) );
	}
	virtual bool OnUserNotice(const CString& sTarget, CString& sMessage)
	{
		return CBDouble( "OnUserNotice", sTarget, sMessage );
	}
	virtual bool OnPrivNotice(const CNick& Nick, CString& sMessage)
	{
		return CBDouble( "OnPrivNotice", NICK( Nick ), sMessage );
	}
	virtual bool OnChanNotice(const CNick& Nick, const CChan& Channel, CString& sMessage)
	{
		return( CBTriple( "OnChanNotice", NICK( Nick ), CHAN( Channel ), sMessage ) );
	}

	void AddHook( const PString & sHookName ) { m_mssHookNames.insert( sHookName ); }
	void DelHook( const PString & sHookName )
	{
		set< PString >::iterator it = m_mssHookNames.find( sHookName );
		if ( it != m_mssHookNames.end() )
			m_mssHookNames.erase( it );
	}

	int CallBack( const PString & sHookName, const VPString & vsArgs );
	int CBNone( const PString & sHookName )
	{
		VPString vsArgs;
		return( CallBack( sHookName, vsArgs ) );
	}

	template <class A>
	inline int CBSingle( const PString & sHookName, const A & a )
	{
		VPString vsArgs;
		vsArgs.push_back( a );
		return( CallBack( sHookName, vsArgs ) );
	}
	template <class A, class B>
	inline int CBDouble( const PString & sHookName, const A & a, const B & b )
	{
		VPString vsArgs;
		vsArgs.push_back( a );
		vsArgs.push_back( b );
		return( CallBack( sHookName, vsArgs ) );
	}
	template <class A, class B, class C>
	inline int CBTriple( const PString & sHookName, const A & a, const B & b, const C & c )
	{
		VPString vsArgs;
		vsArgs.push_back( a );
		vsArgs.push_back( b );
		vsArgs.push_back( c );
		return( CallBack( sHookName, vsArgs ) );
	}
	template <class A, class B, class C, class D>
	inline int CBFour( const PString & sHookName, const A & a, const B & b, const C & c, const D & d )
	{
		VPString vsArgs;
		vsArgs.push_back( a );
		vsArgs.push_back( b );
		vsArgs.push_back( c );
		vsArgs.push_back( d );
		return( CallBack( sHookName, vsArgs ) );
	}

	bool Eval( const CString & sScript );

private:

	PerlInterpreter	*m_pPerl;
	set< PString >	m_mssHookNames;

};

MODULEDEFS( CModPerl )



//////////////////////////////// PERL GUTS //////////////////////////////

XS(XS_AddHook)
{
	dXSARGS;
	if ( items != 1 )
		Perl_croak( aTHX_ "Usage: AddHook( sFuncName )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
			g_ModPerl->AddHook( (char *)SvPV(ST(0),PL_na) );

		PUTBACK;
	}
}

XS(XS_DelHook)
{
	dXSARGS;
	if ( items != 1 )
		Perl_croak( aTHX_ "Usage: DelHook( sFuncName )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
			g_ModPerl->DelHook( (char *)SvPV(ST(0),PL_na) );

		PUTBACK;
	}
}

XS(XS_AddTimer)
{
	dXSARGS;
	if ( items != 4 )
		Perl_croak( aTHX_ "Usage: AddZNCTimer( sFuncName, iInterval, iCycles, sDesc )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CString sLabel = (char *)SvPV(ST(0),PL_na);
			u_int iInterval = (u_int)SvIV(ST(1));
			u_int iCycles = (u_int)SvIV(ST(2));
			CString sDesc = (char *)SvPV(ST(3),PL_na);

			CModPerlTimer *pTimer = new CModPerlTimer( g_ModPerl, iInterval, iCycles, sLabel, sDesc ); 
			g_ModPerl->AddHook( sLabel );
			g_ModPerl->AddTimer( pTimer );
		}
		PUTBACK;
	}
}

XS(XS_UnlinkTimer)
{
	dXSARGS;
	if ( items != 1 )
		Perl_croak( aTHX_ "Usage: KillZNCTimer( sLabel )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CString sLabel = (char *)SvPV(ST(0),PL_na);

			CTimer *pTimer = g_ModPerl->FindTimer( sLabel );
			if ( pTimer )
			{
				g_ModPerl->UnlinkTimer( pTimer );
				g_ModPerl->DelHook( sLabel );
			}
		}
		PUTBACK;
	}
}

XS(XS_PutIRC)
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

	
XS(XS_PutUser)
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

	
XS(XS_PutStatus)
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

XS(XS_PutModule)
{
	dXSARGS;
	if ( items != 3 )
		Perl_croak( aTHX_ "Usage: PutModule( sLine, sIdent, sHost )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CString sLine = (char *)SvPV(ST(0),PL_na);
			CString sIdent = (char *)SvPV(ST(1),PL_na);
			CString sHost = (char *)SvPV(ST(2),PL_na);
			g_ModPerl->PutModule( sLine, sIdent, sHost );
		}
		PUTBACK;
	}
}

XS(XS_PutModNotice)
{
	dXSARGS;
	if ( items != 3 )
		Perl_croak( aTHX_ "Usage: PutModNotice( sLine, sIdent, sHost )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CString sLine = (char *)SvPV(ST(0),PL_na);
			CString sIdent = (char *)SvPV(ST(1),PL_na);
			CString sHost = (char *)SvPV(ST(2),PL_na);
			g_ModPerl->PutModNotice( sLine, sIdent, sHost );
		}
		PUTBACK;
	}
}

XS(XS_exit)
{
	dXSARGS;
	if ( items != 1 )
		Perl_croak( aTHX_ "Usage: exit( status )" );

	SP -= items;
	ax = (SP - PL_stack_base) + 1 ;
	{
		if ( g_ModPerl )
		{
			CString sStatus = (char *)SvPV(ST(0),PL_na);
			g_ModPerl->PutModule( "Shutting down module, status " + sStatus );
			g_ModPerl->PerlInterpShutdown();
		}
		PUTBACK;
	}
}

/////////// supporting functions from within module

bool CModPerl::Eval( const CString & sScript )
{
	dSP;
	ENTER;
	SAVETMPS;

	PUSHMARK( SP );
	XPUSHs( sv_2mortal( newSVpv( sScript.c_str(), sScript.length() ) ) );
	PUTBACK;

	SV *val = sv_2mortal( newSVpv( ZNCEvalCB, strlen( ZNCEvalCB ) ) );

	SPAGAIN;

	call_sv( val, G_EVAL|G_VOID );

	if ( SvTRUE( ERRSV ) ) 
	{ 
		CString sError = SvPV( ERRSV, PL_na);
		PutModule( "Perl Error [" + sError + "]" );
		cerr << "Perl Error [" << sError << "]" << endl;
		POPs; 
	}
	PUTBACK;
	FREETMPS;
	LEAVE;

	return( true );
}

int CModPerl::CallBack( const PString & sHookName, const VPString & vsArgs )
{
	if ( !m_pPerl )
		return( 0 );

	set< PString >::iterator it = m_mssHookNames.find( sHookName );

	if ( it == m_mssHookNames.end() )
		return( 0 );
	
	dSP;
	ENTER;
	SAVETMPS;

	PUSHMARK( SP );
	for( VPString::size_type a = 0; a < vsArgs.size(); a++ )
	{
		switch( vsArgs[a].GetType() )
		{
			case PString::NUM:
				XPUSHs( sv_2mortal( newSVnv( vsArgs[a].ToDouble() ) ) );
				break;
			case PString::INT:
				XPUSHs( sv_2mortal( newSViv( vsArgs[a].ToLongLong() ) ) );
				break;
			case PString::UINT:
			case PString::BOOL:
				XPUSHs( sv_2mortal( newSVuv( vsArgs[a].ToULongLong() ) ) );
				break;
			case PString::STRING:
			default:
				XPUSHs( sv_2mortal( newSVpv( vsArgs[a].c_str(), vsArgs[a].length() ) ) );
				break;
		}
	}
	PUTBACK;

	int iCount = call_pv( sHookName.c_str(), G_EVAL|G_SCALAR );

	SPAGAIN;
	int iRet = 0;

	if ( SvTRUE( ERRSV ) ) 
	{ 
		CString sError = SvPV( ERRSV, PL_na);
		PutModule( "Perl Error [" + *it + "] [" + sError + "]" );
		cerr << "Perl Error [" << *it << "] [" << sError << "]" << endl;
		POPs; 

	} else
	{
		if ( iCount == 1 )
			iRet = POPi;	
	}

	PUTBACK;
	FREETMPS;
	LEAVE;

	return( iRet );
}

////////////////////// Events ///////////////////

// special case this, required for perl modules that are dynamic
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

bool CModPerl::OnLoad( const CString & sArgs ) 
{
	m_pPerl = perl_alloc();
	perl_construct( m_pPerl );
	const char *pArgv[] = { "", "-e" "0" };

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
	newXS( "AddHook", XS_AddHook, (char *)file );
	newXS( "DelHook", XS_DelHook, (char *)file );
	newXS( "AddTimer", XS_AddTimer, (char *)file );
	newXS( "UnlinkTimer", XS_UnlinkTimer, (char *)file );
	newXS( "PutIRC", XS_PutIRC, (char *)file );
	newXS( "PutUser", XS_PutUser, (char *)file );
	newXS( "PutStatus", XS_PutStatus, (char *)file );
	newXS( "PutModule", XS_PutModule, (char *)file );
	newXS( "PutModNotice", XS_PutModNotice, (char *)file );

	// this sets up the eval CB that we call from here on out. this way we can grab the error produced
	CString sTMP = "sub ";
   	sTMP += ZNCEvalCB;
	sTMP += " { my $arg = shift; eval $arg; }";
	eval_pv( sTMP.c_str(), FALSE );

	if ( !sArgs.empty() )
	{
		if ( !Eval( "require \"" + sArgs + "\"" ) )
		{
			PerlInterpShutdown();
			return( false );
		}
		AddHook( "OnLoad" );
		AddHook( "Shutdown" );
		return( CBNone( "OnLoad" ) );
	}

	return( true );
}

bool CModPerl::OnDCCUserSend(const CNick& RemoteNick, unsigned long uLongIP, unsigned short uPort, 
		const CString& sFile, unsigned long uFileSize)
{
	VPString vsArgs;
	vsArgs.push_back( NICK( RemoteNick ) );
	vsArgs.push_back( uLongIP );
	vsArgs.push_back( uPort );
	vsArgs.push_back( sFile );
	
	return( CallBack( "OnDCCUserSend", vsArgs ) );
}
/////////////// CModPerlTimer //////////////
void CModPerlTimer::RunJob() 
{
	((CModPerl *)m_pModule)->CBNone( GetName() );
}

#endif /* HAVE_PERL */
