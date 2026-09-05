// Client side C/C++ program to demonstrate Socket programming 

#include <stdio.h> 
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>
#include <vector>
#include <functional>
#include <unordered_map>
#include <csignal>
#include <thread>
#include <atomic>
#include <iomanip>
#include <stdexcept>
#include <utility>

#include "DebugUtils.h"
#include "Client.h"
#include "OpCodes.h"

std::atomic<bool> g_Running{true};

class CLI {
public:
    using Handler = std::function<void(const std::vector<std::string>&)>;

    void addCommand(const std::string& name, Handler handler) {
        commands[name] = std::move(handler);
    }

    void run(Client& client) {
        std::string line;

        while (g_Running) {
            std::cout << "> ";

            if (!std::getline(std::cin, line))
                break;

            if (line.empty())
                continue;

            auto args = tokenize(line);
            if (args.empty())
                continue;

            if (args[0] == "exit" || args[0] == "quit")
                break;

            // Find the longest matching command.
            std::string command;
            std::vector<std::string> commandArgs;

            for (size_t i = 0; i < args.size(); ++i) {
                std::string candidate;

                for (size_t j = 0; j <= i; ++j) {
                    if (j > 0)
                        candidate += ' ';

                    candidate += args[j];
                }

                auto it = commands.find(candidate);

                if (it != commands.end()) {
                    command = candidate;

                    commandArgs.assign(
                        args.begin() + i + 1,
                        args.end()
                    );
                }
            }

            if (command.empty()) {
                std::cout << "Unknown command: " << args[0] << '\n';
                continue;
            }

            try {
                commands.at(command)(commandArgs);
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << '\n';
            }
        }
    }

private:
    std::unordered_map<std::string, Handler> commands;

    static std::vector<std::string> tokenize(const std::string& line) {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;

        while (iss >> token)
            tokens.push_back(token);

        return tokens;
    }
};

void handleSignal(int signal)
{
    if (signal == SIGINT)
        g_Running = 0;
}

int main(int argc, char const* argv[])
{
    std::signal(SIGINT, handleSignal);

    Client client;
    CLI cli;

    if (!client.initClientConnection())
        return -1;

    if (!client.initTLS())
        return -1;

    /* *********************************************************** */
    /* *********************** CLI options *********************** */
    /* *********************************************************** */

    // echo ...
    cli.addCommand("echo", [&client](const std::vector<std::string>& args) {
        std::string replyStr;

        for (const std::string& arg : args)
            replyStr += arg + ' ';

        client.sendEchoRequest(replyStr);
    });

    // add <a> <b> ...
    cli.addCommand("add", [&client](const std::vector<std::string>& args)  {
        if (args.size() < 2)
            throw std::runtime_error("usage: add <a> <b> ...");

        std::vector<Number> nums;
        bool validNumbers = true;
        try
        {
            for (auto arg : args)
            {
                Number num = parse_number(arg);
                nums.push_back(num);
            }
        }
        catch (const std::exception& e)
        {
            validNumbers = false;
            std::cout << "invalid numbers: " << e.what() << '\n';
        }

        if (validNumbers)
            client.sendAdditionRequest(nums);

    });

    // broadcast ...
    cli.addCommand("broadcast", [&client](const std::vector<std::string>& args) {
        std::string replyStr;

        for (const std::string& arg : args)
            replyStr += arg + ' ';

        client.sendBroadcast(replyStr);
    });

    // ping
    cli.addCommand("ping", [&client](const std::vector<std::string>&) {
        client.sendPing();
    });

    // uptime
    cli.addCommand("uptime", [&client](const std::vector<std::string>&) {
        client.sendUptime();
    });

    // increment-counter
    cli.addCommand("increment-counter", [&client](const std::vector<std::string>&) {
        client.sendIncrementCounter();
    });

    // get-counter
    cli.addCommand("get-counter", [&client](const std::vector<std::string>&) {
        client.sendGetCounter();
    });

    /* todo:
    CMSG_GET_CLIENT_LIST    = 0x0004, // todo
    CMSG_SEND_MSG_TO_CLIENT = 0x0005, // todo
    */

    cli.addCommand("chat", [&client](const std::vector<std::string>&) {
        std::cout
            << "Available commands for chat:\n"
            << "  get-rooms\n"
            << "  info\n"
            << "  join\n"
            << "  say\n";
    });

    cli.addCommand("chat get-rooms", [&client](const std::vector<std::string>&) {
        client.sendGetChatRooms();
    });

    cli.addCommand("chat info", [&client](const std::vector<std::string>&) {
        std::cout << " chat info\n";
    });

    cli.addCommand("chat join", [&client](const std::vector<std::string>&) {
        std::cout << " chat join\n";
    });

    cli.addCommand("chat say", [&client](const std::vector<std::string>&) {
        std::cout << " chat say\n";
    });

    // help
    cli.addCommand("help", [](const std::vector<std::string>&) {
        std::cout
            << "Available commands:\n"
            << "  echo <text...>\n"
            << "  broadcast <text...>\n"
            << "  add <a> <b> ...\n"
            << "  ping\n"
            << "  uptime\n"
            << "  increment-counter\n"
            << "  get-counter\n"
            << "  chat ..\n"
            << "  help\n"
            << "  exit\n";
    });

    std::thread t([&client]() {
        while (g_Running)
            client.processReplyFromServerIfAny();
    });
    t.detach();

    cli.run(client);
    g_Running = false;

    return 0;
}
