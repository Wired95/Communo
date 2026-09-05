#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "SharedDefinitions.h"
#include "NetworkHeaders.h"
#include "NumParser.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <vector>
#include <chrono>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#define CLOSE_SOCKET closesocket

#else

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#define SD_SEND SHUT_WR

#define CLOSE_SOCKET close

#endif

class Client
{
public:
    Client() : m_Sock(0) {}
    ~Client();

    bool initClientConnection();

    bool initTLS();

    void processReplyFromServerIfAny();

    void sendSSLPacketToServer(const std::string& packet);
    void sendEchoRequest(std::string msg);
    void sendAdditionRequest(const std::vector<Number> numbers);
    void sendBroadcast(std::string const msg);
    void sendPing();
    void sendUptime();
    void sendIncrementCounter();
    void sendGetCounter();

private:
    unsigned long long m_Sock;
    SSL_CTX* m_ctx;
    SSL* m_ssl;

    std::chrono::steady_clock::time_point m_pingStart;
};

#endif // _CLIENT_H_
