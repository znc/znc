/*
 * Copyright (C) 2004-2018 ZNC, see the NOTICE file for details.
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

#include <znc/znc.h>
#include <signal.h>
#include <time.h>
#include <thread>

#if defined(HAVE_LIBSSL) && defined(HAVE_PTHREAD) && \
    (!defined(OPENSSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x10100004)
/* Starting with version 1.1.0-pre4, OpenSSL has a new threading implementation
   that doesn't need locking callbacks.

     "OpenSSL now uses a new threading API. It is no longer necessary to set
     locking callbacks to use OpenSSL in a multi-threaded environment. There are
     two supported threading models: pthreads and windows threads. It is also
     possible to configure OpenSSL at compile time for "no-threads". The old
     threading API should no longer be used. The functions have been replaced
     with "no-op" compatibility macros."

   See openssl/openssl@2e52e7df518d80188c865ea3f7bb3526d14b0c08. */
#include <znc/Threads.h>
#include <openssl/crypto.h>
#include <memory>

static std::vector<std::unique_ptr<CMutex>> lock_cs;

static void locking_callback(int mode, int type, const char* file, int line) {
    if (mode & CRYPTO_LOCK) {
        lock_cs[type]->lock();
    } else {
        lock_cs[type]->unlock();
    }
}

static unsigned long thread_id_callback() {
    return (unsigned long)pthread_self();
}

static CRYPTO_dynlock_value* dyn_create_callback(const char* file, int line) {
    return (CRYPTO_dynlock_value*)new CMutex;
}

static void dyn_lock_callback(int mode, CRYPTO_dynlock_value* dlock,
                              const char* file, int line) {
    CMutex* mtx = (CMutex*)dlock;

    if (mode & CRYPTO_LOCK) {
        mtx->lock();
    } else {
        mtx->unlock();
    }
}

static void dyn_destroy_callback(CRYPTO_dynlock_value* dlock, const char* file,
                                 int line) {
    CMutex* mtx = (CMutex*)dlock;

    delete mtx;
}

static void thread_setup() {
    lock_cs.resize(CRYPTO_num_locks());

    for (std::unique_ptr<CMutex>& mtx : lock_cs)
        mtx = std::unique_ptr<CMutex>(new CMutex());

    CRYPTO_set_id_callback(&thread_id_callback);
    CRYPTO_set_locking_callback(&locking_callback);

    CRYPTO_set_dynlock_create_callback(&dyn_create_callback);
    CRYPTO_set_dynlock_lock_callback(&dyn_lock_callback);
    CRYPTO_set_dynlock_destroy_callback(&dyn_destroy_callback);
}

#else
#define thread_setup()
#endif

using std::cout;
using std::endl;
using std::set;

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
#define no_argument 0
#define required_argument 1
#define optional_argument 2

struct option {
    const char* a;
    int opt;
    int* flag;
    int val;
};

static inline int getopt_long(int argc, char* const argv[],
                              const char* optstring, const struct option*,
                              int*) {
    return getopt(argc, argv, optstring);
}
#endif

static const struct option g_LongOpts[] = {
    {"help", no_argument, nullptr, 'h'},
    {"version", no_argument, nullptr, 'v'},
    {"debug", no_argument, nullptr, 'D'},
    {"foreground", no_argument, nullptr, 'f'},
    {"no-color", no_argument, nullptr, 'n'},
    {"allow-root", no_argument, nullptr, 'r'},
    {"makeconf", no_argument, nullptr, 'c'},
    {"makepass", no_argument, nullptr, 's'},
    {"makepem", no_argument, nullptr, 'p'},
    {"datadir", required_argument, nullptr, 'd'},
    {nullptr, 0, nullptr, 0}};

static void GenerateHelp(const char* appname) {
    CUtils::PrintMessage("USAGE: " + CString(appname) + " [options]");
    CUtils::PrintMessage("Options are:");
    CUtils::PrintMessage(
        "\t-h, --help         List available command line options (this page)");
    CUtils::PrintMessage(
        "\t-v, --version      Output version information and exit");
    CUtils::PrintMessage(
        "\t-c, --makeconf     Interactively create a new config");
    CUtils::PrintMessage(
        "\t-d, --datadir      Set a different ZNC repository (default is "
        "~/.znc)");
    CUtils::PrintMessage(
        "\t-D, --debug        Output debugging information (Implies -f)");
    CUtils::PrintMessage("\t-f, --foreground   Don't fork into the background");
    CUtils::PrintMessage(
        "\t-n, --no-color     Don't use escape sequences in the output");
#ifdef HAVE_LIBSSL
    CUtils::PrintMessage(
        "\t-p, --makepem      Generates a pemfile for use with SSL");
#endif /* HAVE_LIBSSL */
    CUtils::PrintMessage(
        "\t-r, --allow-root   Don't complain if ZNC is run as root");
    CUtils::PrintMessage(
        "\t-s, --makepass     Generates a password for use in config");
}

class CSignalHandler {
  public:
    CSignalHandler(CZNC* pZNC) {
        if (pipe(m_iPipe)) {
            DEBUG("Ouch, can't open pipe for signal handler: "
                  << strerror(errno));
            exit(1);
        }
        pZNC->GetManager().MonitorFD(new CSignalHandlerMonitorFD(m_iPipe[0]));
        sigset_t signals;
        sigfillset(&signals);
        pthread_sigmask(SIG_SETMASK, &signals, nullptr);
        m_thread = std::thread([=]() { HandleSignals(pZNC); });
    }
    ~CSignalHandler() {
        pthread_cancel(m_thread.native_handle());
        m_thread.join();
    }

  private:
    class CSignalHandlerMonitorFD : public CSMonitorFD {
        // This class just prevents the pipe buffer from filling by clearing it
      public:
        CSignalHandlerMonitorFD(int fd) { Add(fd, CSockManager::ECT_Read); }

        bool FDsThatTriggered(
            const std::map<int, short>& miiReadyFds) override {
            for (const auto& it : miiReadyFds) {
                if (it.second) {
                    int sig;
                    read(it.first, &sig, sizeof(sig));
                }
            }
            return true;
        }
    };

    void HandleSignals(CZNC* pZNC) {
        sigset_t signals;
        sigemptyset(&signals);
        sigaddset(&signals, SIGHUP);
        sigaddset(&signals, SIGUSR1);
        sigaddset(&signals, SIGINT);
        sigaddset(&signals, SIGQUIT);
        sigaddset(&signals, SIGTERM);
        sigaddset(&signals, SIGPIPE);
        // Handle only these signals specially; the rest will have their default
        // action, but in this thread
        pthread_sigmask(SIG_SETMASK, &signals, nullptr);
        while (true) {
            int sig;
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
            // This thread can be cancelled, but only during this function.
            // Such cancel will be the only way to finish this thread.
            if (sigwait(&signals, &sig) == -1) continue;
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
            // TODO probably move switch() to CSignalHandlerMonitorFD?
            switch (sig) {
                case SIGHUP:
                    pZNC->SetConfigState(CZNC::ECONFIG_NEED_REHASH);
                    break;
                case SIGUSR1:
                    pZNC->SetConfigState(CZNC::ECONFIG_NEED_VERBOSE_WRITE);
                    break;
                case SIGINT:
                case SIGQUIT:
                case SIGTERM:
                    pZNC->SetConfigState(CZNC::ECONFIG_NEED_QUIT);
                    // Reset handler to default by:
                    // * not blocking it
                    // * not waiting for it
                    // So, if ^C is pressed, but for some reason it didn't work,
                    // second ^C will kill the process for sure.
                    sigdelset(&signals, sig);
                    pthread_sigmask(SIG_SETMASK, &signals, nullptr);
                    break;
                case SIGPIPE:
                default:
                    break;
            }
            // This write() must succeed because POSIX guarantees that writes of
            // less than PIPE_BUF are atomic (and PIPE_BUF is at least 512).
            size_t w = write(m_iPipe[1], &sig, sizeof(sig));
            if (w != sizeof(sig)) {
                DEBUG(
                    "Something bad happened during write() to a pipe for "
                    "signal handler, wrote "
                    << w << " bytes: " << strerror(errno));
                exit(1);
            }
        }
    }

    std::thread m_thread;

    // pipe for waking up the main thread
    int m_iPipe[2];
};

static bool isRoot() {
    // User root? If one of these were root, we could switch the others to root,
    // too
    return (geteuid() == 0 || getuid() == 0);
}

static void seedPRNG() {
    struct timeval tv;
    unsigned int seed;

    // Try to find a seed which can't be as easily guessed as only time()

    if (gettimeofday(&tv, nullptr) == 0) {
        seed = (unsigned int)tv.tv_sec;

        // This is in [0:1e6], which means that roughly 20 bits are
        // actually used, let's try to shuffle the high bits.
        seed ^= uint32_t((tv.tv_usec << 10) | tv.tv_usec);
    } else
        seed = (unsigned int)time(nullptr);

    seed ^= rand();
    seed ^= getpid();

    srand(seed);
}

int main(int argc, char** argv) {
    CString sConfig;
    CString sDataDir = "";

    thread_setup();

    seedPRNG();
    CDebug::SetStdoutIsTTY(isatty(1));

    int iArg, iOptIndex = -1;
    bool bMakeConf = false;
    bool bMakePass = false;
    bool bAllowRoot = false;
    bool bForeground = false;
#ifdef ALWAYS_RUN_IN_FOREGROUND
    bForeground = true;
#endif
#ifdef HAVE_LIBSSL
    bool bMakePem = false;
#endif
    CZNC::CreateInstance();

    while ((iArg = getopt_long(argc, argv, "hvnrcspd:Df", g_LongOpts,
                               &iOptIndex)) != -1) {
        switch (iArg) {
            case 'h':
                GenerateHelp(argv[0]);
                return 0;
            case 'v':
                cout << CZNC::GetTag() << endl;
                cout << CZNC::GetCompileOptionsString() << endl;
                return 0;
            case 'n':
                CDebug::SetStdoutIsTTY(false);
                break;
            case 'r':
                bAllowRoot = true;
                break;
            case 'c':
                bMakeConf = true;
                break;
            case 's':
                bMakePass = true;
                break;
            case 'p':
#ifdef HAVE_LIBSSL
                bMakePem = true;
                break;
#else
                CUtils::PrintError("ZNC is compiled without SSL support.");
                return 1;
#endif /* HAVE_LIBSSL */
            case 'd':
                sDataDir = CString(optarg);
                break;
            case 'f':
                bForeground = true;
                break;
            case 'D':
                bForeground = true;
                CDebug::SetDebug(true);
                break;
            case '?':
            default:
                GenerateHelp(argv[0]);
                return 1;
        }
    }

    if (optind < argc) {
        CUtils::PrintError("Unrecognized command line arguments.");
        CUtils::PrintError("Did you mean to run `/znc " +
                           CString(argv[optind]) + "' in IRC client instead?");
        CUtils::PrintError("Hint: `/znc " + CString(argv[optind]) +
                           "' is an alias for `/msg *status " +
                           CString(argv[optind]) + "'");
        return 1;
    }

    CZNC* pZNC = &CZNC::Get();
    pZNC->InitDirs(((argc) ? argv[0] : ""), sDataDir);

#ifdef HAVE_LIBSSL
    if (bMakePem) {
        pZNC->WritePemFile();

        CZNC::DestroyInstance();
        return 0;
    }
#endif /* HAVE_LIBSSL */

    if (bMakePass) {
        CString sSalt;
        CUtils::PrintMessage("Type your new password.");
        CString sHash = CUtils::GetSaltedHashPass(sSalt);
        CUtils::PrintMessage("Kill ZNC process, if it's running.");
        CUtils::PrintMessage(
            "Then replace password in the <User> section of your config with "
            "this:");
        // Not PrintMessage(), to remove [**] from the beginning, to ease
        // copypasting
        std::cout << "<Pass password>" << std::endl;
        std::cout << "\tMethod = " << CUtils::sDefaultHash << std::endl;
        std::cout << "\tHash = " << sHash << std::endl;
        std::cout << "\tSalt = " << sSalt << std::endl;
        std::cout << "</Pass>" << std::endl;
        CUtils::PrintMessage(
            "After that start ZNC again, and you should be able to login with "
            "the new password.");

        CZNC::DestroyInstance();
        return 0;
    }

    {
        set<CModInfo> ssGlobalMods;
        set<CModInfo> ssUserMods;
        set<CModInfo> ssNetworkMods;
        CUtils::PrintAction("Checking for list of available modules");
        pZNC->GetModules().GetAvailableMods(ssGlobalMods,
                                            CModInfo::GlobalModule);
        pZNC->GetModules().GetAvailableMods(ssUserMods, CModInfo::UserModule);
        pZNC->GetModules().GetAvailableMods(ssNetworkMods,
                                            CModInfo::NetworkModule);
        if (ssGlobalMods.empty() && ssUserMods.empty() &&
            ssNetworkMods.empty()) {
            CUtils::PrintStatus(false, "");
            CUtils::PrintError(
                "No modules found. Perhaps you didn't install ZNC properly?");
            CUtils::PrintError(
                "Read https://wiki.znc.in/Installation for instructions.");
            if (!CUtils::GetBoolInput(
                    "Do you really want to run ZNC without any modules?",
                    false)) {
                CZNC::DestroyInstance();
                return 1;
            }
        }
        CUtils::PrintStatus(true, "");
    }

    if (isRoot()) {
        CUtils::PrintError(
            "You are running ZNC as root! Don't do that! There are not many "
            "valid");
        CUtils::PrintError(
            "reasons for this and it can, in theory, cause great damage!");
        if (!bAllowRoot) {
            CZNC::DestroyInstance();
            return 1;
        }
        CUtils::PrintError("You have been warned.");
        CUtils::PrintError(
            "Hit CTRL+C now if you don't want to run ZNC as root.");
        CUtils::PrintError("ZNC will start in 30 seconds.");
        sleep(30);
    }

    if (bMakeConf) {
        if (!pZNC->WriteNewConfig(sConfig)) {
            CZNC::DestroyInstance();
            return 0;
        }
        /* Fall through to normal bootup */
    }

    CString sConfigError;
    if (!pZNC->ParseConfig(sConfig, sConfigError)) {
        CUtils::PrintError("Unrecoverable config error.");
        CZNC::DestroyInstance();
        return 1;
    }

    if (!pZNC->OnBoot()) {
        CUtils::PrintError("Exiting due to module boot errors.");
        CZNC::DestroyInstance();
        return 1;
    }

    if (bForeground) {
        int iPid = getpid();
        CUtils::PrintMessage("Staying open for debugging [pid: " +
                             CString(iPid) + "]");

        pZNC->WritePidFile(iPid);
        CUtils::PrintMessage(CZNC::GetTag());
    } else {
        CUtils::PrintAction("Forking into the background");

        int iPid = fork();

        if (iPid == -1) {
            CUtils::PrintStatus(false, strerror(errno));
            CZNC::DestroyInstance();
            return 1;
        }

        if (iPid > 0) {
            // We are the parent. We are done and will go to bed.
            CUtils::PrintStatus(true, "[pid: " + CString(iPid) + "]");

            pZNC->WritePidFile(iPid);
            CUtils::PrintMessage(CZNC::GetTag());
            /* Don't destroy pZNC here or it will delete the pid file. */
            return 0;
        }

        /* fcntl() locks don't necessarily propagate to forked()
         *   children.  Reacquire the lock here.  Use the blocking
         *   call to avoid race condition with parent exiting.
         */
        if (!pZNC->WaitForChildLock()) {
            CUtils::PrintError(
                "Child was unable to obtain lock on config file.");
            CZNC::DestroyInstance();
            return 1;
        }

        // Redirect std in/out/err to /dev/null
        close(0);
        open("/dev/null", O_RDONLY);
        close(1);
        open("/dev/null", O_WRONLY);
        close(2);
        open("/dev/null", O_WRONLY);

        CDebug::SetStdoutIsTTY(false);

        // We are the child. There is no way we can be a process group
        // leader, thus setsid() must succeed.
        setsid();
        // Now we are in our own process group and session (no
        // controlling terminal). We are independent!
    }

    // Handle all signals in separate thread
    std::unique_ptr<CSignalHandler> SignalHandler(new CSignalHandler(pZNC));

    int iRet = 0;

    try {
        pZNC->Loop();
    } catch (const CException& e) {
        switch (e.GetType()) {
            case CException::EX_Shutdown:
                iRet = 0;
                break;
            case CException::EX_Restart: {
                // strdup() because GCC is stupid
                char* args[] = {
                    strdup(argv[0]),                    strdup("--datadir"),
                    strdup(pZNC->GetZNCPath().c_str()), nullptr,
                    nullptr,                            nullptr,
                    nullptr};
                int pos = 3;
                if (CDebug::Debug())
                    args[pos++] = strdup("--debug");
                else if (bForeground)
                    args[pos++] = strdup("--foreground");
                if (!CDebug::StdoutIsTTY()) args[pos++] = strdup("--no-color");
                if (bAllowRoot) args[pos++] = strdup("--allow-root");
                // The above code adds 3 entries to args tops
                // which means the array should be big enough

                SignalHandler.reset();
                CZNC::DestroyInstance();
                execvp(args[0], args);
                CUtils::PrintError("Unable to restart ZNC [" +
                                   CString(strerror(errno)) + "]");
            } /* Fall through */
            default:
                iRet = 1;
        }
    }

    SignalHandler.reset();
    CZNC::DestroyInstance();

    CUtils::PrintMessage("Exiting");

    return iRet;
}
