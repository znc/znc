#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "znc.h"
#include "md5.h"

void die(int sig) {
	signal( SIGSEGV, SIG_DFL );
	signal( SIGABRT, SIG_DFL );
	signal( SIGPIPE, SIG_DFL );

	delete CZNC::New();
	exit(sig);
}

int main(int argc, char** argv) {
	string sConfig = "znc.conf";

	if (argc > 1) {
		if ((argc > 2) || (strcasecmp(argv[1], "--help") == 0) || (strcasecmp(argv[1], "-h") == 0)) {
			cerr << "Usage: " << argv[0] << " [--makepass|--help|znc.conf]" << endl;
			return 1;
		}

		if (strcasecmp(argv[1], "--makepass") == 0) {
			char* pass = getpass( "Enter Password: " );
			int iLen = strlen(pass);
			cout << "Use this in the <User> section of your config:" << endl << endl << "Pass = " << CMD5(pass, iLen) << " -" << endl << endl;
			memset((char*) pass, 0, iLen);	// Null out our pass so it doesn't sit in memory
			return 0;
		} else {
			sConfig = argv[1];
		}
	}

	CZNC* pZNC = CZNC::New();

	pZNC->InitDirs(((argc) ? argv[0] : ""));

	if (!pZNC->ParseConfig(sConfig)) {
		cerr << endl << "*** Unrecoverable error while parsing [" << sConfig << "]" << endl;
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
