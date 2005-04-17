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
	CUtils::PrintMessage("USAGE: " + string(appname) + " [options] [znc.conf]");
	CUtils::PrintMessage("Options are:");
	CUtils::PrintMessage("\t--help");
	CUtils::PrintMessage("\t--makepass      Generates a password for use in config");
#ifdef HAVE_LIBSSL
	CUtils::PrintMessage("\t--makepem       Generates a pemfile for use with SSL");
	CUtils::PrintMessage("\t--encrypt-pem   Encrypts the pemfile");
#endif /* HAVE_LIBSSL */	
}

void die(int sig) {
	signal( SIGSEGV, SIG_DFL );
	signal( SIGABRT, SIG_DFL );
	signal( SIGPIPE, SIG_DFL );

#ifdef _DEBUG
	CUtils::PrintMessage("Exiting on SIG [" + CUtils::ToString(sig) + "]");
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
	if (bMakePem) {
		CZNC* pZNC = CZNC::New();
		pZNC->InitDirs("");
		string sPemFile = pZNC->GetPemLocation();

		CUtils::PrintAction("Writing Pem file [" + sPemFile + "]");

		if (CFile::Exists(sPemFile)) {
			CUtils::PrintStatus(false, "File already exists");
			delete pZNC;
			return 1;
		}

		FILE *f = fopen( sPemFile.c_str(), "w" );

		if (!f) {
			CUtils::PrintStatus(false, "Unable to open");
			delete pZNC;
			return 1 ;
		}

		CUtils::GenerateCert( f, bEncPem );
		fclose(f);

		CUtils::PrintStatus(true);

		delete pZNC;
		return 0;
	}
#endif /* HAVE_LIBSSL */	
	if ( bMakePass ) {
		char* pass = getpass( "Enter Password: " );
		int iLen = strlen(pass);
		CUtils::PrintMessage("Use this in the <User> section of your config:");
		CUtils::PrintMessage("Pass = " + string((const char*) CMD5(pass, iLen)) + " -");
		memset((char*) pass, 0, iLen);	// null out our pass so it doesn't sit in memory
		return 0;
	}

	CZNC* pZNC = CZNC::New();
	pZNC->InitDirs(((argc) ? argv[0] : ""));

	if (!pZNC->ParseConfig(sConfig)) {
		CUtils::PrintError("Unrecoverable error while parsing config.");
		delete pZNC;
		return 1;
	}

	if (!pZNC->GetListenPort()) {
		CUtils::PrintError("You must supply a ListenPort in your config.");
		delete pZNC;
		return 1;
	}

	if (!pZNC->OnBoot()) {
		CUtils::PrintError("Exiting due to module boot errors.");
		delete pZNC;
		return 1;
	}

#ifndef _DEBUG
	CUtils::PrintAction("Forking into the background");

	int iPid = fork();

	if (iPid == -1) {
		CUtils::PrintStatus(false, strerror(errno));
		delete pZNC;
		exit(1);
	}

	if (iPid > 0) {
		CUtils::PrintStatus(true, "[pid: " + CUtils::ToString(iPid) + "]");

		CUtils::PrintMessage("ZNC - by prozac@gmail.com");
	//	[port: " << ((pZNC->IsSSL()) ? "+" : "") << pZNC->GetListenPort() << "] [pid: " << iPid << "]" << endl;
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
