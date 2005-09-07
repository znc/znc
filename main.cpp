#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <signal.h>
#include "znc.h"
#include "Modules.h"
#include "MD5.h"

static struct option g_LongOpts[] = {
	{ "help",				0,	NULL,	0 },
	{ "version",			0,	NULL,	0 },
	{ "makeconf",			0,	NULL,	0 },
	{ "makepass",			0,	NULL,	0 },
#ifdef HAVE_LIBSSL
	{ "makepem",			0,	NULL,	0 },
	{ "encrypt-pem",		0,	NULL, 	0 },
#endif /* HAVE_LIBSSL */
	{ NULL }
};

void GenerateHelp(const char *appname) {
	CUtils::PrintMessage("USAGE: " + CString(appname) + " [options] [config]");
	CUtils::PrintMessage("Options are:");
	CUtils::PrintMessage("\t--help)");
	CUtils::PrintMessage("\t--version      Output version information and exit");
	CUtils::PrintMessage("\t--makeconf     Interactively create a new config");
	CUtils::PrintMessage("\t--makepass     Generates a password for use in config");
#ifdef HAVE_LIBSSL
	CUtils::PrintMessage("\t--makepem      Generates a pemfile for use with SSL");
	CUtils::PrintMessage("\t--encrypt-pem  Encrypts the pemfile");
#endif /* HAVE_LIBSSL */
}

void die(int sig) {
	signal(SIGSEGV, SIG_DFL);
	signal(SIGABRT, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

#ifdef _DEBUG
	CUtils::PrintMessage("Exiting on SIG [" + CString::ToString(sig) + "]");
	if ((sig == SIGABRT) || (sig == SIGSEGV)) {
		abort();
	}
#endif /* _DEBUG */

	delete CZNC::New();
	exit(sig);
}

int main(int argc, char** argv) {
	CString sConfig;

#ifdef HAVE_LIBSSL
	// initialize ssl, allow client to have compression enabled if desired
	InitSSL(CT_ZLIB);
#endif /* HAVE_LIBSSL */

	int iArg, iOptIndex = -1;
	bool bMakeConf = false;
	bool bMakePass = false;
#ifdef HAVE_LIBSSL
	bool bMakePem = false;
	bool bEncPem = false;
#endif /* HAVE_LIBSSL */
	while ((iArg = getopt_long(argc, argv, "c|h", g_LongOpts, &iOptIndex) != -1)) {
		switch (iArg) {
			case 1: { // long options
				if (iOptIndex >= 0) {
					CString sOption = Lower(g_LongOpts[iOptIndex].name);
					if (sOption == "version") {
						cout << CZNC::GetTag() << endl;
						return 0;
					} else if (sOption == "makeconf") {
						bMakeConf = true;
					} else if (sOption == "makepass") {
						bMakePass = true;
#ifdef HAVE_LIBSSL
					} else if (sOption == "makepem") {
						bMakePem = true;
					} else if (sOption == "encrypt-pem") {
						bEncPem = true;
#endif /* HAVE_LIBSSL */
					} else if (sOption == "help") {
						GenerateHelp(argv[0]);
						return 0;
					}
				} else {
					GenerateHelp(argv[0]);
					return 1;
				}
				break;
			}
			case 'h':
				GenerateHelp(argv[0]);
				return 0;
			default: {
				GenerateHelp(argv[0]);
				return 1;
			}
		}
	}

	if (optind < argc) {
		sConfig = argv[optind];
	}

	if (bMakeConf) {
		CZNC* pZNC = CZNC::New();
		pZNC->InitDirs("");
		pZNC->WriteNewConfig(sConfig);
		return 0;
	}

#ifdef HAVE_LIBSSL
	if (bMakePem) {
		CZNC* pZNC = CZNC::New();
		pZNC->InitDirs("");
		CString sPemFile = pZNC->GetPemLocation();

		CUtils::PrintAction("Writing Pem file [" + sPemFile + "]");

		if (CFile::Exists(sPemFile)) {
			CUtils::PrintStatus(false, "File already exists");
			delete pZNC;
			return 1;
		}

		FILE *f = fopen(sPemFile.c_str(), "w");

		if (!f) {
			CUtils::PrintStatus(false, "Unable to open");
			delete pZNC;
			return 1 ;
		}

		CUtils::GenerateCert(f, bEncPem);
		fclose(f);

		CUtils::PrintStatus(true);

		delete pZNC;
		return 0;
	}
#endif /* HAVE_LIBSSL */
	if (bMakePass) {
		CString sHash = CUtils::GetHashPass();
		CUtils::PrintMessage("Use this in the <User> section of your config:");
		CUtils::PrintMessage("Pass = " + sHash + " -");

		return 0;
	}

	CZNC* pZNC = CZNC::New();
	pZNC->InitDirs(((argc) ? argv[0] : ""));

	if (!pZNC->ParseConfig(sConfig)) {
		CUtils::PrintError("Unrecoverable config error.");
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

#ifdef _DEBUG
	CUtils::PrintMessage("Staying open for debugging");
#else	
	CUtils::PrintAction("Forking into the background");

	int iPid = fork();

	if (iPid == -1) {
		CUtils::PrintStatus(false, strerror(errno));
		delete pZNC;
		exit(1);
	}

	if (iPid > 0) {
		CUtils::PrintStatus(true, "[pid: " + CString::ToString(iPid) + "]");

		pZNC->WritePidFile(iPid);
		CUtils::PrintMessage(CZNC::GetTag(false));
		exit(0);
	}

	// Redirect std in/out/err to /dev/null
	close(0); open("/dev/null", O_RDONLY);
	close(1); open("/dev/null", O_WRONLY);
	close(2); open("/dev/null", O_WRONLY);
#endif

	struct sigaction sa;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, (struct sigaction*) NULL);

	sa.sa_handler = die;
	sigaction(SIGINT,  &sa, (struct sigaction*) NULL);
	sigaction(SIGILL,  &sa, (struct sigaction*) NULL);
	sigaction(SIGQUIT, &sa, (struct sigaction*) NULL);
	sigaction(SIGBUS,  &sa, (struct sigaction*) NULL);
	sigaction(SIGSEGV, &sa, (struct sigaction*) NULL);
	sigaction(SIGTERM, &sa, (struct sigaction*) NULL);

	int iRet = 0;

	try {
		iRet = pZNC->Loop();
	} catch (CException e) {
		// EX_Shutdown is thrown to exit
		switch (e.GetType()) {
			case CException::EX_Shutdown:
				iRet = 0;
			default:
				iRet = 1;
		}
	}

	delete pZNC;

	return iRet;
}
