#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <iomanip>
#include <chrono>
#include <poll.h>
#include <fcntl.h>
#include <cerrno>
#include <utility>

#include "Server.h"
#include "DebugUtils.h"
#include "OpCodes.h"

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

bool ClientSocket::InitSSL(SSL_CTX* ctx, int timeoutSeconds)
{
    ssl = SSL_new(ctx);

    if (ssl == nullptr)
    {
        ERR_print_errors_fp(stderr);
        close();
        return false;
    }

    if (SSL_set_fd(ssl, socket) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        ssl = nullptr;
        close();
        return false;
    }

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(timeoutSeconds);

    while (true)
    {
        int ret = SSL_accept(ssl);

        if (ret == 1)
        {
            sslEnabled = true;
            return true;
        }

        int error = SSL_get_error(ssl, ret);

        if (error != SSL_ERROR_WANT_READ &&
            error != SSL_ERROR_WANT_WRITE)
        {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            ssl = nullptr;
            sslEnabled = false;
            return false;
        }

        auto now = std::chrono::steady_clock::now();

        if (now >= deadline)
        {
            sLog.log(LOG_FLAG_DEBUG,
                     "SSL handshake timed out.");

            SSL_free(ssl);
            ssl = nullptr;
            sslEnabled = false;
            return false;
        }

        auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now).count();

        struct pollfd pfd{};
        pfd.fd = socket;
        pfd.events =
            (error == SSL_ERROR_WANT_READ)
                ? POLLIN
                : POLLOUT;

        int result = poll(&pfd, 1, static_cast<int>(remaining));

        if (result == 0)
        {
            sLog.log(LOG_FLAG_DEBUG,
                     "SSL handshake timed out.");

            SSL_free(ssl);
            ssl = nullptr;
            sslEnabled = false;
            return false;
        }

        if (result < 0)
        {
            if (errno == EINTR)
                continue;

            perror("poll");

            SSL_free(ssl);
            ssl = nullptr;
            sslEnabled = false;
            return false;
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            sLog.log(LOG_FLAG_DEBUG,
                     "Socket error during SSL handshake.");

            SSL_free(ssl);
            ssl = nullptr;
            sslEnabled = false;
            return false;
        }
    }
}

ClientSocket::ClientSocket(ClientSocket&& other) noexcept
    : socket(other.socket),
      ssl(other.ssl),
      sslEnabled(other.sslEnabled)
{
    other.socket = INVALID_SOCKET;
    other.ssl = nullptr;
    other.sslEnabled = false;
}

ClientSocket& ClientSocket::operator=(ClientSocket&& other) noexcept
{
    if (this != &other)
    {
        if (ssl != nullptr)
        {
            SSL_free(ssl);
        }

        if (socket != INVALID_SOCKET)
        {
            ::close(socket);
        }

        socket = other.socket;
        ssl = other.ssl;
        sslEnabled = other.sslEnabled;

        other.socket = INVALID_SOCKET;
        other.ssl = nullptr;
        other.sslEnabled = false;
    }

    return *this;
}

Server::Server()
{
    // For connection pooling
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    m_SendHelloMsg = false;

    //type of socket created  
    m_Adress.sin_family = AF_INET;
    m_Adress.sin_addr.s_addr = INADDR_ANY;
    m_Adress.sin_port = htons(PORT);

    m_AddrLen = sizeof(m_Adress);

    m_ServerState = eServerState::NOT_STARTED;
}

Server::~Server()
{
    if (m_ctx != nullptr)
    {
        SSL_CTX_free(m_ctx);
    }
}

void Server::InitSSL()
{
    // Create a server-side TLS context.
    m_ctx = SSL_CTX_new(TLS_server_method());

    if (m_ctx == nullptr) {
        ERR_print_errors_fp(stderr);
        throw CommunoException(CommunoException::err_ssl_ctx, false, "TLS context creation failed");
        return;
    }

    // Require TLS 1.3.
    if (SSL_CTX_set_min_proto_version(m_ctx, TLS1_3_VERSION) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(m_ctx);
        throw CommunoException(CommunoException::err_tls, false, "TLS 1.3 context creation failed");
        return;
    }

    // Load the server certificate.
    //
    // The certificate file should contain the server certificate and,
    // if necessary, the intermediate certificate chain.
    if (SSL_CTX_use_certificate_chain_file(m_ctx, tls_cert_file) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(m_ctx);
        throw CommunoException(CommunoException::err_fileopen, false, "Error loading TLS cert file");
        return;
    }

    // Load the server's private key.
    if (SSL_CTX_use_PrivateKey_file(
            m_ctx,
            tls_key_file,
            SSL_FILETYPE_PEM) != 1) {

        ERR_print_errors_fp(stderr);
        SSL_CTX_free(m_ctx);
        throw CommunoException(CommunoException::err_fileopen, false, "Error loading TLS key file");
        return;
    }

    // Make sure the private key matches the certificate.
    if (SSL_CTX_check_private_key(m_ctx) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(m_ctx);
        throw CommunoException(CommunoException::err_tls, false, "Private key don't match certificate");
        return;
    }
    else
    {
        sLog.log(LOG_FLAG_DEBUG, "SSL cert and key loaded successfully.");
    }

    // Recommended TLS options.
    SSL_CTX_set_options(
        m_ctx,
        SSL_OP_NO_COMPRESSION
    );
}

void Server::InitSocket()
{
    INIT_SOCK;

    //throw CommunoException(CommunoException::err_socket_creation, true);
    //std::cout << "TEST" << std::endl;

    //create a master socket  
    if ((m_MasterSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        throw CommunoException(CommunoException::err_socket_creation, false, "Creation failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections
    int allowMultipleConnections = true;
    if (setsockopt(m_MasterSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&allowMultipleConnections,
        sizeof(allowMultipleConnections)) < 0)
    {
        sLog.log(LOG_FLAG_ERROR, "setsockopt failed");
        exit(EXIT_FAILURE);
    }

    //bind the socket to localhost port
    if (bind(m_MasterSocket, (struct sockaddr*) & m_Adress, sizeof(m_Adress)) < 0)
    {
        sLog.log(LOG_FLAG_ERROR, "bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    //try to specify maximum of 3 pending connections for the master socket  
    if (listen(m_MasterSocket, 3) < 0)
    {
            sLog.log(LOG_FLAG_ERROR, "listen failed");
            exit(EXIT_FAILURE);
    }

    sLog.log(LOG_FLAG_INFO, "Waiting for connections ...");

    m_ServerState = eServerState::STARTED;
}

void Server::Cleanup()
{
    CLEANUP_SOCK;
}

void Server::SetSendHelloMessagesToNewClients(bool send)
{
    m_SendHelloMsg = send;
}

void Server::SendMsgToSocket(ClientSocket* client, const char* msg)
{
    std::string reply;

    unsigned short int opcode = htons(SMSG_MESSAGE);
    reply.append(reinterpret_cast<const char*>(&opcode), sizeof(opcode));
    reply += msg;

    int sent = SSL_write(
        client->ssl,
        reply.data(),
        static_cast<int>(reply.size())
    );

    if (sent <= 0)
    {
        int sslError = SSL_get_error(client->ssl, sent);

        sLog.log(
            LOG_FLAG_DEBUG,
            "SMSG_MESSAGE SSL_write failed, error: " +
            std::to_string(sslError)
        );

        return;
    }

    sLog.log(LOG_FLAG_DEBUG, "SMSG_MESSAGE sent");
}

void Server::SendMOTD(ClientSocket& socket)
{
    std::string reply;

    unsigned short int opcode = htons(SMSG_MOTD);

    reply.append(
        reinterpret_cast<const char*>(&opcode),
        sizeof(opcode)
    );

    reply += m_HelloMsg;

    const char* data = reply.data();
    size_t remaining = reply.size();

    while (remaining > 0)
    {
        int written = SSL_write(
            socket.ssl,
            data,
            static_cast<int>(remaining)
        );

        if (written <= 0)
        {
            int sslError = SSL_get_error(socket.ssl, written);

            sLog.log(
                LOG_FLAG_ERROR,
                std::string("Failed to send SMSG_MOTD over TLS, SSL error: ") + std::to_string(sslError)
            );

            break;
        }

        data += written;
        remaining -= static_cast<size_t>(written);
    }

    if (remaining == 0)
    {
        sLog.log(LOG_FLAG_DEBUG, "SMSG_MOTD sent");
    }
}

void Server::PoolActivity()
{
    //clear the socket set  
    FD_ZERO(&m_Readfds);

    //add master socket to set  
    FD_SET(m_MasterSocket, &m_Readfds);
    int max_sd = m_MasterSocket;

    //add child sockets to set
    for (const auto& itr : m_ClientSocket)
    {
        //if valid socket descriptor then add to read list  
        if (itr.socket > 0)
            FD_SET(itr.socket, &m_Readfds);

        //highest file descriptor number, need it for the select function  
        if (itr.socket > max_sd)
            max_sd = itr.socket;
    }

    //pool activity on one of the sockets  
    int activity = select(max_sd + 1, &m_Readfds, nullptr, nullptr, &tv);

    if ((activity < 0) && (errno != EINTR))
    {
        sLog.log(LOG_FLAG_ERROR, "select error");
    }
}

void Server::HandleNewConnections()
{
    //If something happened on the master socket ,  
    //then its an incoming connection  
    if (FD_ISSET(m_MasterSocket, &m_Readfds))
    {
        int new_socket = accept(m_MasterSocket, (struct sockaddr*) & m_Adress, (socklen_t*)&m_AddrLen);
        if (new_socket < 0)
        {
            perror("accept");
            return;
        }

        //inform user of socket number - used in send and receive commands  
        std::stringstream connLog;
        connLog << "New connection, socket fd is: " << new_socket \
        << ", ip is: " << inet_ntoa(m_Adress.sin_addr) \
        << ", port: " << ntohs(m_Adress.sin_port);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        // Make the client socket non-blocking.
        int flags = fcntl(new_socket, F_GETFL, 0);
        fcntl(new_socket, F_SETFL, flags | O_NONBLOCK);

        //add new socket to array of sockets
        ClientSocket client(new_socket);
        if (client.InitSSL(m_ctx, 10))
        {
            m_ClientSocket.push_back(std::move(client));
            connLog.str("");
            connLog.clear();
            connLog << "Adding to list of sockets as: " << m_ClientSocket.size() - 1;
            sLog.log(LOG_FLAG_DEBUG, connLog.str());

            //send new connection greeting message  
            if (m_SendHelloMsg)
            {
                sendHelloMsg(m_ClientSocket[m_ClientSocket.size() - 1]);
                sLog.log(LOG_FLAG_DEBUG, "Welcome message sent successfully");
            }
        }
        else
        {
            close(new_socket);
            sLog.log(LOG_FLAG_DEBUG, "SSL handshake failed, socket freed.");
        }

    }
}

void Server::ProcessRequests()
{
    //else its some IO operation on some other socket 
    for (std::vector<ClientSocket>::iterator it = m_ClientSocket.begin();
        it != m_ClientSocket.end(); )
    {
        int socket = it->socket;

        if (m_ServerState == eServerState::CLOSING)
        {
            it->close();
            it = m_ClientSocket.erase(it);
            continue;
        }

        if (FD_ISSET(socket, &m_Readfds))
        {
            int valread = SSL_read(it->ssl, buffer, sizeof(buffer));

            if (valread <= 0)
            {
                getpeername(socket,
                            (struct sockaddr*)&m_Adress,
                            (socklen_t*)&m_AddrLen);

                std::stringstream connLog;
                connLog << "Host disconnected, ip is: "
                        << inet_ntoa(m_Adress.sin_addr)
                        << ", port: "
                        << ntohs(m_Adress.sin_port);

                sLog.log(LOG_FLAG_DEBUG, connLog.str());

                it->close();

                // erase() returns the next valid iterator
                it = m_ClientSocket.erase(it);
                continue;
            }

            if (valread >= static_cast<int>(sizeof(unsigned short)))
            {
                CallHandler(&(*it), valread);
            }
            else
            {
                std::string reply = "Invalid packet.";
                
                int sent = SSL_write(it->ssl,
                                    reply.data(),
                                    static_cast<int>(reply.size()));

                if (sent <= 0)
                {
                    int ssl_error = SSL_get_error(it->ssl, sent);

                    // @todo: Handle SSL error here
                }
            }
        }

        ++it;
    }
}

void Server::CallHandler(ClientSocket* client, int payloadSize)
{
    unsigned short int opcode;
    memcpy(&opcode, buffer, sizeof(opcode));
    opcode = ntohs(opcode);

    std::stringstream connLog;
    connLog << "Received opcode: ";

    std::string _payload; // CMSG_ECHO_REQUEST

    switch (opcode) {
        case CMSG_ECHO_REQUEST:
            _payload = std::string(buffer + sizeof(opcode), payloadSize - sizeof(opcode));

            connLog << OPCODE_STR(CMSG_ECHO_REQUEST) << std::endl;
            connLog << "payload: " << _payload.c_str();
            sLog.log(LOG_FLAG_DEBUG, connLog.str());

            CallHandlerEcho(client, _payload);
            break;
        default:
            // Log the unknown opcode as CMSG_UNKNOWN_OPCODE
            uint16_t CMSG_UNKNOWN_OPCODE = opcode;
            connLog << OPCODE_STR(CMSG_UNKNOWN_OPCODE);
            sLog.log(LOG_FLAG_DEBUG, connLog.str());

            // Send reply to the client
            std::string errMsg = "Unknown opcode: ";
            errMsg += opcode;
            SendMsgToSocket(client, errMsg);
            break;
    }
}

void Server::CallHandlerEcho(ClientSocket* client, std::string reply)
{
    std::string packet;

    unsigned short int ropcode = htons(SMSG_ECHO_REQUEST);
    packet.append(reinterpret_cast<const char*>(&ropcode), sizeof(ropcode));
    packet += reply;

    std::stringstream connLog;
    connLog << "Sending opcode: " << OPCODE_STR(SMSG_ECHO_REQUEST);
    connLog << " (size:" << packet.size() << ")";
    sLog.log(LOG_FLAG_DEBUG, connLog.str());

    int sent = SSL_write(
        client->ssl,
        packet.data(),
        static_cast<int>(packet.size())
    );

    if (sent <= 0)
    {
        int sslError = SSL_get_error(client->ssl, sent);

        std::stringstream errorLog;
        errorLog << "SSL_write failed for "
                 << OPCODE_STR(SMSG_ECHO_REQUEST)
                 << ", SSL error: " << sslError;

        sLog.log(LOG_FLAG_DEBUG, errorLog.str());
        return;
    }
}
