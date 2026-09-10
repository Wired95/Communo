// Client side C/C++ program to demonstrate Socket programming

#include <atomic>
#include <csignal>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Client.h"
#include "DebugUtils.h"
#include "OpCodes.h"

std::atomic<bool> g_Running{true};

class CLI
{
  public:
    using Handler = std::function<void(const std::vector<std::string> &)>;

    void addCommand(const std::string &name, Handler handler)
    {
        commands[name] = std::move(handler);
    }

    void run(Client &client)
    {
        std::string line;

        while (g_Running)
        {
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

            for (size_t i = 0; i < args.size(); ++i)
            {
                std::string candidate;

                for (size_t j = 0; j <= i; ++j)
                {
                    if (j > 0)
                        candidate += ' ';

                    candidate += args[j];
                }

                auto it = commands.find(candidate);

                if (it != commands.end())
                {
                    command = candidate;

                    commandArgs.assign(args.begin() + i + 1, args.end());
                }
            }

            if (command.empty())
            {
                std::cout << "Unknown command: " << args[0] << '\n';
                continue;
            }

            try
            {
                commands.at(command)(commandArgs);
            }
            catch (const std::exception &e)
            {
                std::cout << "Error: " << e.what() << '\n';
            }
        }
    }

  private:
    std::unordered_map<std::string, Handler> commands;

    static std::vector<std::string> tokenize(const std::string &line)
    {
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

int main(int argc, char const *argv[])
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
    cli.addCommand("echo",
                   [&client](const std::vector<std::string> &args)
                   {
                       std::string replyStr;

                       for (const std::string &arg : args)
                           replyStr += arg + ' ';

                       client.sendEchoRequest(replyStr);
                   });

    // add <a> <b> ...
    cli.addCommand("add",
                   [&client](const std::vector<std::string> &args)
                   {
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
                       catch (const std::exception &e)
                       {
                           validNumbers = false;
                           std::cout << "invalid numbers: " << e.what() << '\n';
                       }

                       if (validNumbers)
                           client.sendAdditionRequest(nums);
                   });

    // broadcast ...
    cli.addCommand("broadcast",
                   [&client](const std::vector<std::string> &args)
                   {
                       std::string replyStr;

                       for (const std::string &arg : args)
                           replyStr += arg + ' ';

                       client.sendBroadcast(replyStr);
                   });

    // ping
    cli.addCommand("ping", [&client](const std::vector<std::string> &)
                   { client.sendPing(); });

    // uptime
    cli.addCommand("uptime", [&client](const std::vector<std::string> &)
                   { client.sendUptime(); });

    // counter
    cli.addCommand("counter",
                   [&client](const std::vector<std::string> &)
                   {
                       std::cout << "Available commands for counter:\n"
                                 << "  increment\n"
                                 << "  get\n";
                   });

    // counter increment
    cli.addCommand("counter increment",
                   [&client](const std::vector<std::string> &)
                   { client.sendIncrementCounter(); });

    // counter get
    cli.addCommand("counter get", [&client](const std::vector<std::string> &)
                   { client.sendGetCounter(); });

    cli.addCommand("client",
                   [](const std::vector<std::string> &)
                   {
                       std::cout << "Available commands for client:\n"
                                 << "  get-clients\n"
                                 << "  set-username <username>\n"
                                 << "  send-msg <clientID> <msg>\n";
                   });

    cli.addCommand("client get-clients",
                   [&client](const std::vector<std::string> &)
                   { client.sendGetClients(); });

    cli.addCommand("client set-username",
                   [&client](const std::vector<std::string> &args)
                   {
                       if (args.size() != 1)
                           throw std::runtime_error(
                               "usage: client set-username <username>");

                       client.sendChangeUsername(args[0]);
                   });

    cli.addCommand(
        "client send-msg",
        [&client](const std::vector<std::string> &args)
        {
            if (args.size() < 2)
                throw std::runtime_error(
                    "usage: client send-msg <clientID> <msg>");

            Number num;
            bool validNumbers = true;
            uint64_t clientID;
            try
            {
                num = parse_number(args[0]);
            }
            catch (const std::exception &e)
            {
                validNumbers = false;
                std::cout << "invalid room number: " << e.what() << '\n';
            }

            if (!is_unsigned_integer(num))
                validNumbers = false;
            else
                clientID =
                    std::visit([](auto value) -> uint64_t
                               { return static_cast<uint64_t>(value); }, num);

            if (validNumbers)
            {
                std::string msg;
                for (size_t i = 1; i < args.size(); ++i)
                    msg += args[i] + ' ';

                client.sendClientMessage(clientID, msg);
            }
            else
                std::cout << "invalid client ID: " << args[0] << std::endl;
        });

    cli.addCommand("chat",
                   [](const std::vector<std::string> &)
                   {
                       std::cout << "Available commands for chat:\n"
                                 << "  get-rooms\n"
                                 << "  info\n"
                                 << "  join <room ID> <opt: password>\n"
                                 << "  say\n";
                   });

    cli.addCommand("chat get-rooms", [&client](const std::vector<std::string> &)
                   { client.sendGetChatRooms(); });

    cli.addCommand("chat info", [&client](const std::vector<std::string> &)
                   { client.sendGetRoomInfo(); });

    cli.addCommand(
        "chat join",
        [&client](const std::vector<std::string> &args)
        {
            if (args.size() != 1 && args.size() != 2)
                throw std::runtime_error(
                    "usage: chat join <room ID> <opt: password>");

            Number num;
            bool validNumbers = true;
            try
            {
                num = parse_number(args[0]);
            }
            catch (const std::exception &e)
            {
                validNumbers = false;
                std::cout << "invalid room number: " << e.what() << '\n';
            }

            std::string pwd = "";
            if (args.size() == 2)
                pwd = args[1];

            if (validNumbers && is_unsigned_integer(num) && is_uint8_t(num))
                client.sendJoinRoomRequest(std::get<uint8_t>(num), pwd);
            else
                std::cout << "invalid room number: " << args[0] << std::endl;
        });

    cli.addCommand("chat say",
                   [&client](const std::vector<std::string> &args)
                   {
                       std::string msg;

                       for (const std::string &arg : args)
                           msg += arg + ' ';

                       client.sendChatSay(msg);
                   });

    // help
    cli.addCommand("help",
                   [](const std::vector<std::string> &)
                   {
                       std::cout << "Available commands:\n"
                                 << "  echo <text...>\n"
                                 << "  broadcast <text...>\n"
                                 << "  add <a> <b> ...\n"
                                 << "  ping\n"
                                 << "  uptime\n"
                                 << "  counter ..\n"
                                 << "  client ..\n"
                                 << "  chat ..\n"
                                 << "  help\n"
                                 << "  exit\n";
                   });

    std::thread t(
        [&client]()
        {
            while (g_Running)
                client.processReplyFromServerIfAny();
        });
    t.detach();

    cli.run(client);
    g_Running = false;

    return 0;
}
