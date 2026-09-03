
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unordered_set>

#include "Server.h"
#include "DebugUtils.h"
#include "Logging.h"

bool closingRequested = false;

void InitDaemon(bool daemonize)
{
#ifdef __linux__

    if (daemonize)
    {
        pid_t pid = fork();

        if (pid < 0)
            exit(EXIT_FAILURE);

        if (pid > 0)
            exit(EXIT_SUCCESS);

        umask(0);

        if (setsid() < 0)
            exit(EXIT_FAILURE);

        if (chdir("/") != 0)
            exit(EXIT_FAILURE);

        // Close file descriptors only when daemonizing.
        long maxFd = sysconf(_SC_OPEN_MAX);
        if (maxFd < 0)
            maxFd = 1024;

        for (long fd = maxFd; fd >= 0; --fd)
            close(static_cast<int>(fd));
    }

    // Install signals regardless of daemon mode.
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP,  SIG_IGN);

    signal(SIGINT, [](int) {
        closingRequested = true;
    });

    signal(SIGTERM, [](int) {
        closingRequested = true;
    });

#else
    (void)daemonize;
#endif
}

class Options {
public:
    Options(int argc, const char* const argv[])
        : valid_(false)
    {
        // Options that this application recognizes.
        known_ = {
            "no-daemon",
            "debug",
            "help"
        };

        for (int i = 1; i < argc; ++i) {
            std::string arg(argv[i]);

            if (arg.size() < 3 || arg.compare(0, 2, "--") != 0) {
                error_ = "Invalid argument: " + arg;
                return;
            }

            std::string name = arg.substr(2);

            if (known_.find(name) == known_.end()) {
                error_ = "Unknown option: --" + name;
                return;
            }

            options_.insert(name);
        }

        valid_ = true;
    }

    bool valid() const {
        return valid_;
    }

    const std::string& error() const {
        return error_;
    }

    bool has(const std::string& name) const {
        return options_.find(name) != options_.end();
    }

    void displayHelp() {
        sLog.ToConsole("Options:");
        sLog.ToConsole("  --no-daemon");
        sLog.ToConsole("  --debug");
        sLog.ToConsole("  --help");
    }

private:
    std::unordered_set<std::string> known_;
    std::unordered_set<std::string> options_;
    std::string error_;
    bool valid_;
};

int main(int argc, char const *argv[])
{
    // Parse the options
    bool daemonize = true;
    Options options(argc, argv);
    if (!options.valid()) {
        std::cerr << options.error() << std::endl;
        options.displayHelp();
        return 1;
    }

    if (options.has("help")) {
        options.displayHelp();
        return 0;
    }

    if (options.has("no-daemon")) {
        daemonize = false;
        sLog.ToConsole("Running without daemonization, logs will be written in console");
        sLog.setLogOutput(LOG_CONSOLE);
    }
    else
    {
        sLog.setLogOutput(LOG_SYSTEM);
    }

    if (options.has("debug")) {
        sLog.setLogLevel(LOG_LEVEL_DEBUG);
    }

    Server server;

    InitDaemon(daemonize);
    if (daemonize)
        sLog.log(LOG_FLAG_INFO, SERVER_DAEMON_NAME + " daemon started.");

    // enable MOTD
    server.SetSendHelloMessagesToNewClients(true);

    // Init SSL
    try {
        sLog.log(LOG_FLAG_DEBUG, SERVER_DAEMON_NAME + " InitSSL launched.");
        server.InitSSL();
        sLog.log(LOG_FLAG_DEBUG, SERVER_DAEMON_NAME + " InitSSL ended.");
    }
    catch (CommunoException& e)
    {
        sLog.log(LOG_FLAG_ERROR, SERVER_DAEMON_NAME + " failed to init SSL: " + std::string(e.what()));
        e.SendSystemErrDialogBox();
    }

    // Init socket
    try {
        sLog.log(LOG_FLAG_DEBUG, SERVER_DAEMON_NAME + " InitSocket launched.");
        server.InitSocket();
        sLog.log(LOG_FLAG_DEBUG, SERVER_DAEMON_NAME + " InitSocket ended.");
    }
    catch (CommunoException& e)
    {
        sLog.log(LOG_FLAG_ERROR, SERVER_DAEMON_NAME + " failed to init socket: " + std::string(e.what()));
        e.SendSystemErrDialogBox();
    }

    sLog.log(LOG_FLAG_INFO, SERVER_DAEMON_NAME + " Server started.");
    
    while (server.getServerState() != eServerState::CLOSED)
    {
        try {
                //std::cout << "[MAIN] before PoolActivity()" << std::endl;
                server.PoolActivity();
                //std::cout << "[MAIN] after PoolActivity()" << std::endl;
                server.HandleNewConnections();
                //std::cout << "[MAIN] after HandleNewConnections()" << std::endl;
                server.ProcessRequests();
                //std::cout << "[MAIN] after ProcessRequests()" << std::endl;
        }
        catch (CommunoException& e)
        {
            sLog.log(LOG_FLAG_ERROR, SERVER_DAEMON_NAME + " runtime error: " + std::string(e.what()));
            e.SendSystemErrDialogBox();
        }

        if (closingRequested)
        {
            sLog.log(LOG_FLAG_INFO, SERVER_DAEMON_NAME + " closing requested...");
            server.ClosingRequested();
        }
       
        if (server.getServerState() == eServerState::CLOSING)
        {
            server.CloseServer();
            sLog.log(LOG_FLAG_INFO, SERVER_DAEMON_NAME + "server is now closed.");
        }
        else
        {
            usleep(50);
        }
    }

    server.Cleanup();

    if (daemonize)
        sLog.log(LOG_FLAG_INFO, SERVER_DAEMON_NAME + " daemon terminated.");

    return EXIT_SUCCESS;
}
