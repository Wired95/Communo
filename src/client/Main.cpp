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

            const std::string command = args[0];
            args.erase(args.begin());

            if (command == "exit" || command == "quit")
                break;

            auto it = commands.find(command);

            if (it == commands.end()) {
                std::cout << "Unknown command: " << command << '\n';
                continue;
            }

            try {
                it->second(args);
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

    cli.addCommand("echo", [&client](const std::vector<std::string>& args) {
        std::string replyStr;

        for (const std::string& arg : args)
            replyStr += arg + ' ';

        client.sendEchoRequest(replyStr);
    });

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

    cli.addCommand("help", [](const std::vector<std::string>&) {
        std::cout
            << "Available commands:\n"
            << "  echo <text...>\n"
            << "  add <a> <b> ...\n"
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
