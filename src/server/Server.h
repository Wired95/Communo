#ifndef _SERVER_H_
#define _SERVER_H_

#include "SharedDefinitions.h"
#include "NetworkHeaders.h"
#include "Logging.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <iostream>
#include <cstdlib>
#include <vector>

enum class eServerState
{
    NOT_STARTED,
    STARTED,
    CLOSING,
    CLOSED
};

struct ClientSocket
{
    ClientSocket() : socket(0), sslEnabled(false) {}
    ClientSocket(int _socket) : socket(_socket), sslEnabled(false) {}

    ClientSocket(const ClientSocket&) = delete;
    ClientSocket& operator=(const ClientSocket&) = delete;

    ClientSocket(ClientSocket&& other) noexcept;
    ClientSocket& operator=(ClientSocket&& other) noexcept;

    ~ClientSocket() {
        if (ssl)
        {
            SSL_free(ssl);
            SSL_shutdown(ssl);
        }
    }

    bool InitSSL(SSL_CTX* ctx, int timeoutSeconds);

    void close() {
        ::shutdown(socket, SHUT_RDWR);
        ::close(socket);
    }

    int socket;
    SSL* ssl;
    bool sslEnabled;
};

class Server
{
public:
    Server();
    ~Server();

    void InitSSL();

    void InitSocket();

    void Cleanup();

    void SetSendHelloMessagesToNewClients(bool send);

    void SendMsgToSocket(ClientSocket* client, const char* msg);
    void SendMsgToSocket(ClientSocket* client, const std::string msg) {
        SendMsgToSocket(client, msg.c_str());
    }
    void SendMOTD(ClientSocket& socket);

    void PoolActivity();

    void HandleNewConnections();

    void ProcessRequests();

    eServerState getServerState() { return m_ServerState; }

    void ClosingRequested() {
        m_ServerState = eServerState::CLOSING;

        // Close all sockets
        for (auto& itr : m_ClientSocket) {
            itr.close();
        }
    }

    void CloseServer() { m_ServerState = eServerState::CLOSED; }

private:
    struct timeval tv;

    char buffer[4096];  //data buffer of 4K  

    int m_MasterSocket;
    struct sockaddr_in m_Adress;
    int m_AddrLen;
    std::vector<ClientSocket> m_ClientSocket;

    eServerState m_ServerState;

    // SSL
    SSL_CTX* m_ctx;

    // Hello msg
    bool m_SendHelloMsg;
    const char* m_HelloMsg = "Server v1.0 on duty! waiting for commands.\n";
    void sendHelloMsg(ClientSocket& socket) {
        SendMOTD(socket);
    }

    //set of socket descriptors  
    fd_set m_Readfds;

    void CallHandler(ClientSocket* client, int payloadSize);
    void CallHandlerEcho(ClientSocket* client, std::string reply);
    void CallHandlerAdd(ClientSocket* client, size_t offset);
};

#endif // _SERVER_H_