#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <signal.h>
#include "znc.h"
#include "md5.h"

static struct option g_LongOpts[] = 
{
	{ "help",				0,	NULL,	0 },
	{ "makepass",			0,	NULL,	0 },
#ifdef HAVE_LIBSSL
	{ "makepem",			0,	NULL,	0 },
	{ "encrypt-pem",		0,	NULL, 	0 },
#endif /* HAVE_LIBSSL */
	{ NULL }
};

void GenerateHelp( const char *appname )
{
	cerr << "USAGE: " << appname << " [options] [znc.conf]" << endl;
	cerr << "Options are:" << endl;
	cerr << "	--help" << endl;
	cerr << "	--makepass      Generates a password for use in config" << endl;
#ifdef HAVE_LIBSSL
	cerr << "	--makepem       Generates a pemfile for use with SSL" << endl;
	cerr << "	--encrypt-pem   Encrypts the pemfile" << endl;
#endif /* HAVE_LIBSSL */	
}

void die(int sig) {
	signal( SIGSEGV, SIG_DFL );
	signal( SIGABRT, SIG_DFL );
	signal( SIGPIPE, SIG_DFL );

#ifdef _DEBUG
	cerr << "Exiting on SIG [" << sig << "]" << endl;
	if ( ( sig == SIGABRT ) || ( sig == SIGSEGV ) )
		abort();
#endif /* _DEBUG */

	delete CZNC::New();
	exit(sig);
}

int main(int argc, char** argv) {
	string sConfig;

#ifdef HAVE_LIBSSL
	// initialize ssl, allow client to have compression enabled if desired
	InitSSL( CT_ZLIB );
#endif /* HAVE_LIBSSL */

	int iArg, iOptIndex = -1;
#ifdef HAVE_LIBSSL
	bool bMakePem = false;
	bool bEncPem = false;
#endif /* HAVE_LIBSSL */	
	bool bMakePass = false;
	while( ( iArg = getopt_long( argc, argv, "h", g_LongOpts, &iOptIndex ) != -1 ) ) {
		switch( iArg ) {
			case 1:
			{ // long options
				if ( iOptIndex >= 0 ) {
					string sOption = Lower( g_LongOpts[iOptIndex].name );
					if ( sOption == "makepass" )
						bMakePass = true;
#ifdef HAVE_LIBSSL
					else if ( sOption == "makepem" )
						bMakePem = true;
					else if ( sOption == "encrypt-pem" )
						bEncPem = true;
#endif /* HAVE_LIBSSL */	
					else if ( sOption == "help" ) {
						GenerateHelp( argv[0] );
						return( 0 );
					}
				} else {
					GenerateHelp( argv[0] );
					return( 1 );
				}
				break;
			}
			case 'h':
				GenerateHelp( argv[0] );
				return( 0 );	
			default	:
			{
				GenerateHelp( argv[0] );
				return( 1 );
			}
		}
	}

	if ( optind < argc )
		sConfig = argv[optind];

#ifdef HAVE_LIBSSL
	if ( bMakePem ) {
		FILE *f = fopen( "znc.pem", "w" );
		if ( !f ) {
			cerr << "Unable to open znc.pem!" << endl;
			return( 1 );
		}
		CUtils::GenerateCert( f, bEncPem );
		fclose( f );
		return( 0 );
	}
#endif /* HAVE_LIBSSL */	
	if ( bMakePass ) {
		char* pass = getpass( "Enter Password: " );
		int iLen = strlen(pass);
		cout << "Use this in the <User> section of your config:" << endl << endl << "Pass = " << CMD5(pass, iLen) << " -" << endl << endl;
		memset((char*) pass, 0, iLen);	// Null out our pass so it doesn't sit in memory
		return 0;
	}

	CZNC* pZNC = CZNC::New();

	pZNC->InitDirs(((argc) ? argv[0] : ""));

	if (!pZNC->ParseConfig(sConfig)) {
		cerr << endl << "*** Unrecoverable error while parsing config." << endl;
		delete pZNC;
		return 1;
	}

	if (!pZNC->GetListenPort()) {
		cerr << "You must supply a ListenPort in your config." << endl;
		delete pZNC;
		return 1;
	}

	if (!pZNC->OnBoot()) {
		cerr << "Exiting due to module boot errors." << endl;
		delete pZNC;
		return 1;
	}

#ifndef _DEBUG
	int iPid = fork();
	if (iPid == -1) {
		cerr << "Failed to fork into background: [" << strerror(errno) << "]" << endl;
		delete pZNC;
		exit(1);
	}

	if (iPid > 0) {
		cout << "ZNC - by prozac [port: " << ((pZNC->IsSSL()) ? "+" : "") << pZNC->GetListenPort() << "] [pid: " << iPid << "]" << endl;
		pZNC->WritePidFile(iPid);
		exit(0);
	}
	
	/* keep the term from hanging on logout */
	close( 0 );
	close( 1 );
	close( 2 );

#endif

	struct sigaction sa;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, (struct sigaction *)NULL);

	sa.sa_handler = die;
	sigaction(SIGINT,  &sa, (struct sigaction *)NULL);
	sigaction(SIGILL,  &sa, (struct sigaction *)NULL);
	sigaction(SIGQUIT, &sa, (struct sigaction *)NULL);
	sigaction(SIGBUS,  &sa, (struct sigaction *)NULL);
	sigaction(SIGSEGV, &sa, (struct sigaction *)NULL);
	sigaction(SIGTERM, &sa, (struct sigaction *)NULL);

	int iRet = pZNC->Loop();
	delete pZNC;

	return iRet;
}
