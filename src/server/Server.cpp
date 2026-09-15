#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <string>
#include <utility>

#include <openssl/sha.h>

#include "Chat.h"
#include "DebugUtils.h"
#include "FileUtils.h"
#include "NumParser.h"
#include "OpCodes.h"
#include "Server.h"
#include "Universe.h"

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#define CLOSE_SOCKET closesocket

#else

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#define SD_SEND SHUT_WR

#define CLOSE_SOCKET close

#endif

bool ClientSocket::InitSSL(SSL_CTX *ctx, int timeoutSeconds)
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
        std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);

    while (true)
    {
        int ret = SSL_accept(ssl);

        if (ret == 1)
        {
            sslEnabled = true;
            return true;
        }

        int error = SSL_get_error(ssl, ret);

        if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE)
        {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            ssl        = nullptr;
            sslEnabled = false;
            return false;
        }

        auto now = std::chrono::steady_clock::now();

        if (now >= deadline)
        {
            sLog.log(LOG_FLAG_DEBUG, "SSL handshake timed out.");

            SSL_free(ssl);
            ssl        = nullptr;
            sslEnabled = false;
            return false;
        }

        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                             deadline - now)
                             .count();

        struct pollfd pfd{};
        pfd.fd     = socket;
        pfd.events = (error == SSL_ERROR_WANT_READ) ? POLLIN : POLLOUT;

        int result = poll(&pfd, 1, static_cast<int>(remaining));

        if (result == 0)
        {
            sLog.log(LOG_FLAG_DEBUG, "SSL handshake timed out.");

            SSL_free(ssl);
            ssl        = nullptr;
            sslEnabled = false;
            return false;
        }

        if (result < 0)
        {
            if (errno == EINTR)
                continue;

            perror("poll");

            SSL_free(ssl);
            ssl        = nullptr;
            sslEnabled = false;
            return false;
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            sLog.log(LOG_FLAG_DEBUG, "Socket error during SSL handshake.");

            SSL_free(ssl);
            ssl        = nullptr;
            sslEnabled = false;
            return false;
        }
    }
}

ClientSocket::ClientSocket(ClientSocket &&other) noexcept
    : socket(other.socket), ssl(other.ssl), sslEnabled(other.sslEnabled),
      chatRoomJoined(other.chatRoomJoined),
      joinedChatRoomID(other.joinedChatRoomID), clientID(other.clientID),
      clientUsername(other.clientUsername)
{
    // belts and buckles
    other.socket           = INVALID_SOCKET;
    other.ssl              = nullptr;
    other.sslEnabled       = false;
    other.chatRoomJoined   = false;
    other.joinedChatRoomID = 0;
    other.clientID         = 0;
    other.clientUsername   = "<unk>";
}

ClientSocket &ClientSocket::operator=(ClientSocket &&other) noexcept
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

        socket                 = other.socket;
        ssl                    = other.ssl;
        sslEnabled             = other.sslEnabled;
        chatRoomJoined         = other.chatRoomJoined;
        joinedChatRoomID       = other.joinedChatRoomID;
        clientID               = other.clientID;
        clientUsername         = other.clientUsername;

        other.socket           = INVALID_SOCKET;
        other.ssl              = nullptr;
        other.sslEnabled       = false;
        other.chatRoomJoined   = false;
        other.joinedChatRoomID = 0;
        other.clientID         = 0;
        other.clientUsername   = "<unk>";
    }

    return *this;
}

Server::Server()
{
    // For connection pooling
    tv.tv_sec                = 0;
    tv.tv_usec               = 0;

    m_SendHelloMsg           = false;

    // type of socket created
    m_Adress.sin_family      = AF_INET;
    m_Adress.sin_addr.s_addr = INADDR_ANY;
    m_Adress.sin_port        = htons(PORT);

    m_AddrLen                = sizeof(m_Adress);

    m_ServerState            = eServerState::NOT_STARTED;

    m_UniqueCLientCounter    = 0;
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

    if (m_ctx == nullptr)
    {
        ERR_print_errors_fp(stderr);
        throw CommunoException(CommunoException::err_ssl_ctx, false,
                               "TLS context creation failed");
        return;
    }

    // Require TLS 1.3.
    if (SSL_CTX_set_min_proto_version(m_ctx, TLS1_3_VERSION) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(m_ctx);
        throw CommunoException(CommunoException::err_tls, false,
                               "TLS 1.3 context creation failed");
        return;
    }

    // Load the server certificate.
    //
    // The certificate file should contain the server certificate and,
    // if necessary, the intermediate certificate chain.
    if (SSL_CTX_use_certificate_chain_file(m_ctx, tls_cert_file) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(m_ctx);
        throw CommunoException(CommunoException::err_fileopen, false,
                               "Error loading TLS cert file");
        return;
    }

    // Load the server's private key.
    if (SSL_CTX_use_PrivateKey_file(m_ctx, tls_key_file, SSL_FILETYPE_PEM) != 1)
    {

        ERR_print_errors_fp(stderr);
        SSL_CTX_free(m_ctx);
        throw CommunoException(CommunoException::err_fileopen, false,
                               "Error loading TLS key file");
        return;
    }

    // Make sure the private key matches the certificate.
    if (SSL_CTX_check_private_key(m_ctx) != 1)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(m_ctx);
        throw CommunoException(CommunoException::err_tls, false,
                               "Private key don't match certificate");
        return;
    }
    else
    {
        sLog.log(LOG_FLAG_DEBUG, "SSL cert and key loaded successfully.");
    }

    // Recommended TLS options.
    SSL_CTX_set_options(m_ctx, SSL_OP_NO_COMPRESSION);
}

void Server::InitSocket()
{
    INIT_SOCK;

    // throw CommunoException(CommunoException::err_socket_creation, true);
    // std::cout << "TEST" << std::endl;

    // create a master socket
    if ((m_MasterSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        throw CommunoException(CommunoException::err_socket_creation, false,
                               "Creation failed");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections
    int allowMultipleConnections = true;
    if (setsockopt(m_MasterSocket, SOL_SOCKET, SO_REUSEADDR,
                   (char *)&allowMultipleConnections,
                   sizeof(allowMultipleConnections)) < 0)
    {
        sLog.log(LOG_FLAG_ERROR, "setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // bind the socket to localhost port
    if (bind(m_MasterSocket, (struct sockaddr *)&m_Adress, sizeof(m_Adress)) <
        0)
    {
        sLog.log(LOG_FLAG_ERROR, "bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    // try to specify maximum of 3 pending connections for the master socket
    if (listen(m_MasterSocket, 3) < 0)
    {
        sLog.log(LOG_FLAG_ERROR, "listen failed");
        exit(EXIT_FAILURE);
    }

    sLog.log(LOG_FLAG_INFO, "Waiting for connections ...");

    m_ServerState = eServerState::STARTED;
}

void Server::InitEntities()
{
    // Store initial start time
    m_StartTime = std::chrono::steady_clock::now();

    // Load chat rooms
    sLog.log(LOG_FLAG_DEBUG, "Load Chat rooms ...");
    sChat.loadChatRooms();
    sLog.log(LOG_FLAG_DEBUG, "Loaded Chat rooms.");
}

void Server::Cleanup() { CLEANUP_SOCK; }

void Server::SetSendHelloMessagesToNewClients(bool send)
{
    m_SendHelloMsg = send;
}

void Server::SendMsgToSocket(ClientSocket *client, const char *msg)
{
    Packet pkt = INIT_PACKET(SMSG_MESSAGE);
    pkt << msg;

    pkt.sendToSSLClient(client->ssl);
}

void Server::SendMOTD(ClientSocket &socket)
{
    Packet pkt = INIT_PACKET(SMSG_MOTD);
    pkt << m_HelloMsg;

    pkt.sendToSSLClient(socket.ssl);
}

void Server::PoolActivity()
{
    // clear the socket set
    FD_ZERO(&m_Readfds);

    // add master socket to set
    FD_SET(m_MasterSocket, &m_Readfds);
    int max_sd = m_MasterSocket;

    // add child sockets to set
    for (const auto &itr : m_ClientSocket)
    {
        // if valid socket descriptor then add to read list
        if (itr.socket > 0)
            FD_SET(itr.socket, &m_Readfds);

        // highest file descriptor number, need it for the select function
        if (itr.socket > max_sd)
            max_sd = itr.socket;
    }

    // pool activity on one of the sockets
    int activity = select(max_sd + 1, &m_Readfds, nullptr, nullptr, &tv);

    if ((activity < 0) && (errno != EINTR))
    {
        sLog.log(LOG_FLAG_ERROR, "select error");
    }
}

void Server::HandleNewConnections()
{
    // If something happened on the master socket ,
    // then its an incoming connection
    if (FD_ISSET(m_MasterSocket, &m_Readfds))
    {
        int new_socket = accept(m_MasterSocket, (struct sockaddr *)&m_Adress,
                                (socklen_t *)&m_AddrLen);
        if (new_socket < 0)
        {
            perror("accept");
            return;
        }

        // inform user of socket number - used in send and receive commands
        std::stringstream connLog;
        connLog << "New connection, socket fd is: " << new_socket
                << ", ip is: " << inet_ntoa(m_Adress.sin_addr)
                << ", port: " << ntohs(m_Adress.sin_port);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        // Make the client socket non-blocking.
        int flags = fcntl(new_socket, F_GETFL, 0);
        fcntl(new_socket, F_SETFL, flags | O_NONBLOCK);

        // add new socket to array of sockets
        ClientSocket client(new_socket);
        client.clientID = m_UniqueCLientCounter++;

        if (client.InitSSL(m_ctx, 10))
        {

            m_ClientSocket.push_back(std::move(client));
            connLog.str("");
            connLog.clear();
            connLog << "Adding to list of sockets as: "
                    << m_ClientSocket.size() - 1;
            sLog.log(LOG_FLAG_DEBUG, connLog.str());

            // send new connection greeting message
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
    // else its some IO operation on some other socket
    for (std::vector<ClientSocket>::iterator it = m_ClientSocket.begin();
         it != m_ClientSocket.end();)
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
            int valread = SSL_read(it->ssl, buffer, MAX_PACKET_LENGTH);

            if (valread <= 0)
            {
                getpeername(socket, (struct sockaddr *)&m_Adress,
                            (socklen_t *)&m_AddrLen);

                std::stringstream connLog;
                connLog << "Host disconnected, ip is: "
                        << inet_ntoa(m_Adress.sin_addr)
                        << ", port: " << ntohs(m_Adress.sin_port);

                sLog.log(LOG_FLAG_DEBUG, connLog.str());

                it->close();

                // erase() returns the next valid iterator
                it = m_ClientSocket.erase(it);
                continue;
            }

            if (valread >= static_cast<int>(sizeof(unsigned short)))
            {
                Packet packet(buffer, static_cast<std::size_t>(valread));
                CallHandler(&(*it), packet);
            }
            else
            {
                std::string reply = "Invalid packet.";

                int sent          = SSL_write(it->ssl, reply.data(),
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

void Server::SendSSLPacketToClientSocket(
    ClientSocket *client, std::string const &packet,
    const std::string
        &opcodeFancyName /*= "[SMSG_DEFAULT_OPCODE_NAME (undefined)]"*/)
{
    // Log the packet sending
    std::stringstream connLog;
    connLog << "Sending opcode: " << opcodeFancyName;
    connLog << " (size:" << packet.size() << ")";
    sLog.log(LOG_FLAG_DEBUG, connLog.str());

    // Send the packet to the client socket
    int sent =
        SSL_write(client->ssl, packet.data(), static_cast<int>(packet.size()));

    // check return codes
    if (sent <= 0)
    {
        int sslError = SSL_get_error(client->ssl, sent);

        std::stringstream errorLog;
        errorLog << "SSL_write failed for " << opcodeFancyName
                 << ", SSL error: " << sslError;

        sLog.log(LOG_FLAG_DEBUG, errorLog.str());
        return;
    }
}

void Server::SendSSLPacketToClientSocket(ClientSocket *client,
                                         Packet const &packet)
{
    // Log the packet sending
    std::stringstream connLog;
    connLog << "Sending opcode: " << packet.fancy_name();
    connLog << " (size:" << packet.size() << ")";
    sLog.log(LOG_FLAG_DEBUG, connLog.str());

    // Send the packet to the client socket
    int sent =
        SSL_write(client->ssl, packet.data(), static_cast<int>(packet.size()));

    // check return codes
    if (sent <= 0)
    {
        int sslError = SSL_get_error(client->ssl, sent);

        std::stringstream errorLog;
        errorLog << "SSL_write failed for " << packet.fancy_name()
                 << ", SSL error: " << sslError;

        sLog.log(LOG_FLAG_DEBUG, errorLog.str());
        return;
    }
}

bool Server::ReadClientSSLData(ClientSocket *client, void *data,
                               std::size_t size)
{
    char *ptr = static_cast<char *>(data);
    SSL *ssl  = client->ssl;

    while (size > 0)
    {
        int n = SSL_read(
            ssl, ptr, static_cast<int>(std::min(size, std::size_t(INT_MAX))));

        if (n > 0)
        {
            ptr += n;
            size -= n;
            continue;
        }

        const int error = SSL_get_error(ssl, n);

        if (error == SSL_ERROR_WANT_READ)
        {
            const int fd = SSL_get_fd(ssl);

            struct pollfd pfd{};
            pfd.fd     = fd;
            pfd.events = POLLIN;

            if (poll(&pfd, 1, -1) <= 0)
            {
                sLog.log(LOG_FLAG_ERROR, "poll() failed");
                return false;
            }

            continue;
        }

        if (error == SSL_ERROR_WANT_WRITE)
        {
            const int fd = SSL_get_fd(ssl);

            struct pollfd pfd{};
            pfd.fd     = fd;
            pfd.events = POLLOUT;

            if (poll(&pfd, 1, -1) <= 0)
            {
                sLog.log(LOG_FLAG_ERROR, "poll() failed");
                return false;
            }

            continue;
        }

        if (error == SSL_ERROR_ZERO_RETURN)
        {
            sLog.log(LOG_FLAG_INFO, "TLS connection closed cleanly");
            return true;
        }

        sLog.log(LOG_FLAG_ERROR, "SSL_read failed: " + std::to_string(error));
        return false;
    }

    return true;
}

void Server::CallHandler(ClientSocket *client, Packet &packet)
{
    uint16_t opcode;
    packet >> opcode;
    opcode = ntohs(opcode);

    std::stringstream connLog;
    connLog << "Received opcode: ";

    std::string _payload;   // CMSG_ECHO_REQUEST
    size_t offset, minSize; // CMSG_ADDITION_REQUEST

    size_t payloadSize = packet.size();

    switch (opcode)
    {
    case CMSG_ECHO_REQUEST:
        connLog << OPCODE_STR(CMSG_ECHO_REQUEST) << std::endl;
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        CallHandlerEcho(client, packet);
        break;
    case CMSG_ADDITION_REQUEST:
        connLog << OPCODE_STR(CMSG_ADDITION_REQUEST);

        // check minimal required packet size
        offset  = sizeof(opcode);
        //        opcode + 2 number types           + smallest numbers (2)
        minSize = offset + sizeof(eNumberTypes) * 2 + sizeof(uint8_t) * 2;

        if (minSize > payloadSize)
            connLog << "Invalid opcode length, aborting handler call"
                    << std::endl;

        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        if (minSize <= payloadSize)
            CallHandlerAdd(client, offset, payloadSize);

        break;

    case CMSG_BROADCAST_MESSAGE:
        // extract the message here
        _payload =
            std::string(buffer + sizeof(opcode), payloadSize - sizeof(opcode));

        connLog << OPCODE_STR(CMSG_BROADCAST_MESSAGE) << std::endl;
        connLog << "payload: " << _payload.c_str();
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        CallHandlerBroadcast(_payload);
        break;
    case CMSG_GET_CLIENT_LIST:
        connLog << OPCODE_STR(CMSG_GET_CLIENT_LIST);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        CallHandlerGetClientList(client);
        break;
    case CMSG_SEND_MSG_TO_CLIENT:
    {
        connLog << OPCODE_STR(CMSG_SEND_MSG_TO_CLIENT);

        // check minimal required packet size
        offset  = sizeof(opcode);
        //        opcode + Client ID
        minSize = offset + sizeof(uint64_t);

        if (minSize > payloadSize)
            connLog << "Invalid opcode length, aborting handler call"
                    << std::endl;

        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        if (minSize <= payloadSize)
            CallHandlerMsgToClient(client, offset, payloadSize);
        break;
    }
    case CMSG_PING:
        connLog << OPCODE_STR(CMSG_PING);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        CallHandlerPong(client);
        break;
    case CMSG_UPTIME:
        connLog << OPCODE_STR(CMSG_UPTIME);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        CallHandlerUptime(client);
        break;
    case CMSG_INCREMENT_COUNTER:
        connLog << OPCODE_STR(CMSG_INCREMENT_COUNTER);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        sUniverse.incrementCounter();
        break;
    case CMSG_GET_COUNTER:
        connLog << OPCODE_STR(CMSG_GET_COUNTER);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        CallHandlerGetCounter(client);
        break;
    case CMSG_GET_CHAT_ROOMS:
        connLog << OPCODE_STR(CMSG_GET_CHAT_ROOMS);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        CallHandlerGetChatRooms(client);
        break;
    case CMSG_GET_ROOM_INFO:
        connLog << OPCODE_STR(CMSG_GET_ROOM_INFO);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        CallHandlerGetRoominfo(client);
        break;
    case CMSG_JOIN_ROOM:
    {
        offset              = sizeof(opcode);

        size_t requiredSize = offset + sizeof(uint8_t) + SHA256_DIGEST_LENGTH;

        if (payloadSize != requiredSize)
            connLog << "Invalid opcode length, aborting handler call"
                    << std::endl;

        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        if (requiredSize == payloadSize)
            CallHandlerJoinRoom(client, offset, payloadSize);

        break;
    }
    case CMSG_SAY:
    {
        // extract the message here
        _payload =
            std::string(buffer + sizeof(opcode), payloadSize - sizeof(opcode));

        connLog << OPCODE_STR(CMSG_SAY) << std::endl;
        connLog << "payload: " << _payload.c_str();
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        CallHandlerSay(client, _payload);
        break;
    }
    case CMSG_LS_REMOTE:
        connLog << OPCODE_STR(CMSG_LS_REMOTE);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        CallHandlerListRemoteDirectoryContent(client);
        break;
    case CMSG_UPLOAD_FILE:
        connLog << OPCODE_STR(CMSG_UPLOAD_FILE);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        CallHandlerOnFileUpload(client);
        break;
    default:
        // Log the unknown opcode as CMSG_UNKNOWN_OPCODE
        uint16_t CMSG_UNKNOWN_OPCODE = opcode;
        connLog << OPCODE_STR(CMSG_UNKNOWN_OPCODE);
        sLog.log(LOG_FLAG_DEBUG, connLog.str());

        // Send reply to the client
        std::string errMsg = "Unknown or unhandled opcode.";
        SendMsgToSocket(client, errMsg);
        break;
    }
}

void Server::CallHandlerEcho(ClientSocket *client, Packet &packet)
{
    // extract the message
    std::string msg(packet.size() - sizeof(uint16_t), '\0');
    packet >> msg;

    // send the response packet
    Packet rpkt = INIT_PACKET(SMSG_ECHO_REQUEST);
    rpkt << msg;

    rpkt.sendToSSLClient(client->ssl);
}

void Server::CallHandlerAdd(ClientSocket *client, size_t offset,
                            int payloadSize)
{
    std::stringstream connLog;

    double sum = 0;

    while (offset < payloadSize)
    {
        // Get number types to parse
        eNumberTypes type =
            static_cast<eNumberTypes>(static_cast<uint8_t>(buffer[offset++]));

        // Retrieve number values
        Number num = read_number(buffer, offset, type);

        sLog.log(LOG_FLAG_DEBUG, std::string("Number type:") +
                                     std::to_string(type) + " -> " +
                                     number_to_string(num));

        // cast everithing to double and perform the sum
        double val =
            std::visit([](auto v) { return static_cast<double>(v); }, num);

        sum += val;
    }

    sLog.log(LOG_FLAG_DEBUG, std::string("Sum: ") + std::to_string(sum));

    // generate response packet
    Packet pkt = INIT_PACKET(SMSG_ADDITION_REQUEST);
    pkt << sum;

    SendSSLPacketToClientSocket(client, pkt);
}

void Server::CallHandlerBroadcast(std::string const stream)
{
    Packet pkt = INIT_PACKET(SMSG_BROADCAST);
    pkt << stream;

    sLog.log(LOG_FLAG_DEBUG, std::string("Known clients: ") +
                                 std::to_string(m_ClientSocket.size()));

    for (ClientSocket &client : m_ClientSocket)
    {
        if (!client.sslEnabled)
            continue;

        SendSSLPacketToClientSocket(&client, pkt);
    }
}

void Server::CallHandlerGetClientList(ClientSocket *client)
{
    // Packet:
    // [uint16 opcode]
    // [uint8  error]
    // repeated:
    //   [uint32 clientID]
    //   [uint16 usernameLength]
    //   [uint8  username bytes]

    std::string clientList;
    uint8_t error = ERR_OK;

    if (m_ClientSocket.size() > 0)
    {
        for (ClientSocket &_client : m_ClientSocket)
        {
            // append client ID
            uint64_t clientID = htobe64(_client.clientID);
            clientList.append(reinterpret_cast<const char *>(&clientID),
                              sizeof(clientID));

            // Get client username
            std::string username = _client.clientUsername;
            if (_client.clientID == client->clientID)
                username += " (you)";

            // write username
            uint16_t usernameSize =
                htons(static_cast<uint16_t>(username.size()));
            clientList.append(reinterpret_cast<const char *>(&usernameSize),
                              sizeof(usernameSize));
            clientList.append(username);
        }
    }
    else
    {
        error = ERR_NO_CLIENT_FOUND;
    }

    Packet pkt = INIT_PACKET(SMSG_CLIENT_LIST);
    size_t packetSize =
        sizeof(SMSG_CLIENT_LIST) + sizeof(error) + clientList.size();

    if (packetSize <= 4096)
    {
        pkt << error;
        pkt << clientList;
    }
    else
    {
        error = ERR_TOO_MUCH_CLIENTS;
        pkt << error;
    }

    pkt.sendToSSLClient(client->ssl);
}

void Server::CallHandlerMsgToClient(ClientSocket *client, size_t offset,
                                    int payloadSize)
{
    std::string packet, message;
    uint8_t error             = ERR_OK;
    uint64_t clientID         = 0;
    ClientSocket *foundClient = nullptr;

    // Get Client ID
    std::memcpy(&clientID, buffer + offset, sizeof(clientID));
    offset += sizeof(clientID);

    // Check client ID
    if (clientID == client->clientID)
        error = ERR_MSG_TO_SELF;
    else
    {
        for (ClientSocket &_client : m_ClientSocket)
        {
            if (_client.clientID == clientID)
            {
                foundClient = &_client;
                break;
            }
        }

        if (foundClient == nullptr)
            error = ERR_NO_CLIENT_FOUND;
    }

    // Get message
    if (error == ERR_OK)
    {
        message = std::string(reinterpret_cast<const char *>(buffer + offset),
                              payloadSize);

        if (message.empty())
            error = ERR_EMPTY_MESSAGE;
    }

    // Send status to the "from" client
    Packet pkt = INIT_PACKET(SMSG_PRIVATE_MSG_ERR);
    pkt << error;
    pkt.sendToSSLClient(client->ssl);

    // send message to foundClient if everything is correct
    if (error == ERR_OK)
    {
        // Prepare message
        Packet rpkt = INIT_PACKET(SMSG_PRIVATE_MESSAGE);
        rpkt << client->clientID;
        rpkt << message;

        // Send message to found client
        rpkt.sendToSSLClient(foundClient->ssl);
    }
}

void Server::CallHandlerPong(ClientSocket *client)
{
    Packet pkt = INIT_PACKET(SMSG_PONG);
    pkt.sendToSSLClient(client->ssl);
}

void Server::CallHandlerUptime(ClientSocket *client)
{
    Packet pkt = INIT_PACKET(SMSG_UPTIME);

    // compute uptime
    auto now   = std::chrono::steady_clock::now();
    auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(now - m_StartTime)
            .count();

    auto hours = seconds / 3600;
    seconds %= 3600;

    auto minutes = seconds / 60;
    seconds %= 60;

    // Format uptime
    pkt << "Uptime: " << std::to_string(hours) << "h "
        << std::to_string(minutes) << "m " << std::to_string(seconds) << "s";

    pkt.sendToSSLClient(client->ssl);
}

void Server::CallHandlerGetCounter(ClientSocket *client)
{
    Packet pkt       = INIT_PACKET(SMSG_COUNTER);

    uint64_t counter = sUniverse.getCounter();
    pkt << counter;

    pkt.sendToSSLClient(client->ssl);
}

void Server::CallHandlerGetChatRooms(ClientSocket *client)
{
    SendMsgToSocket(client, sChat.getChatRoomsStr());
}

void Server::CallHandlerGetRoominfo(ClientSocket *client)
{
    std::string msg;
    uint8_t roomID = client->joinedChatRoomID;

    std::cout << "client->joinedChatRoomID" << std::to_string(roomID)
              << std::endl;

    if (client->chatRoomJoined == false)
        msg += "No joined room";
    else if (!sChat.checkRoomID(roomID))
        msg += "Invalid room";
    else
    {
        std::cout << std::to_string(roomID) << std::endl;
        msg += "Joined: " + sChat.getRoomName(roomID);
    }

    SendMsgToSocket(client, msg);
}

void Server::CallHandlerJoinRoom(ClientSocket *client, size_t offset,
                                 int payloadSize)
{
    uint8_t error  = ERR_OK;
    uint8_t roomID = static_cast<uint8_t>(buffer[offset++]);
    unsigned char hash[SHA256_DIGEST_LENGTH];

    std::memcpy(hash, buffer + offset, SHA256_DIGEST_LENGTH);

    if (!sChat.checkRoomID(roomID))
        error = ERR_INVALID_ROOM;

    if (sChat.isRoomProtected(roomID))
    {
        if (!sChat.checkPasswordHash(roomID, hash))
            error = ERR_INVALID_ROOM_PASSWORD;
    }

    if (error == ERR_OK)
    {
        client->joinedChatRoomID = roomID;
        client->chatRoomJoined   = true;

        Packet pkt               = INIT_PACKET(SMSG_JOIN_CHAT_ROOM_OK);
        pkt << roomID;

        pkt.sendToSSLClient(client->ssl);
    }
    else
    {
        Packet pkt = INIT_PACKET(SMSG_JOIN_CHAT_ROOM_ERR);
        pkt << error;

        pkt.sendToSSLClient(client->ssl);
    }
}

void Server::CallHandlerSay(ClientSocket *client, std::string message)
{
    uint8_t roomID = client->joinedChatRoomID;
    unsigned short int ropcode;

    if (sChat.checkRoomID(roomID) && client->chatRoomJoined)
    {
        // the room is valid, send that everything is OK
        Packet pktOK = INIT_PACKET(SMSG_SAY_OK);
        pktOK.sendToSSLClient(client->ssl);

        // prepare message packet
        Packet pktSAY = INIT_PACKET(SMSG_SAY);
        pktSAY << message;

        // Broadcast messages to valid clients
        for (ClientSocket &_client : m_ClientSocket)
        {
            if (!_client.sslEnabled)
                continue;

            if (_client.joinedChatRoomID != roomID || !_client.chatRoomJoined)
                continue;

            pktSAY.sendToSSLClient(_client.ssl);
        }
    }
    else
    {
        // the room is not valid, send error message
        uint8_t error = ERR_INVALID_ROOM;
        if (!client->chatRoomJoined)
            error = ERR_NO_ROOM_JOINED;

        Packet pktERR = INIT_PACKET(SMSG_SAY_ERR);
        pktERR << error;
        pktERR.sendToSSLClient(client->ssl);
    }
}

void Server::CallHandlerListRemoteDirectoryContent(ClientSocket *client)
{
    uint8_t error           = FMERR_OK;
    uint8_t fileCount       = 0;

    std::vector<File> files = get_files_in_dir(fm_remote_dir);

    if (files.size() > UINT8_MAX)
        error = FMERR_TOO_MUCH_FILES;

    // Maximum packet payload available for serialized files.
    constexpr std::size_t maxSerializedSize =
        4096 - sizeof(error) - sizeof(fileCount) - sizeof(uint16_t);

    // serialize the files
    // [filename length: uint8_t]
    // [filename]
    // [size: uint64_t]
    // [md5sum: 16 bytes]
    //
    std::string fileSerializedData;
    if (error == FMERR_OK)
    {
        for (const File &file : files)
        {
            if (file.filename.size() > UINT8_MAX)
            {
                error = FMERR_FILENAME_TOO_LONG;
                break;
            }

            const uint8_t filenameLength =
                static_cast<uint8_t>(file.filename.size());

            fileSerializedData.append(
                reinterpret_cast<const char *>(&filenameLength),
                sizeof(filenameLength));

            fileSerializedData.append(file.filename.data(),
                                      file.filename.size());

            fileSerializedData.append(
                reinterpret_cast<const char *>(&file.size), sizeof(file.size));

            fileSerializedData.append(reinterpret_cast<const char *>(file.md5),
                                      sizeof(file.md5));

            // Check immediately so we don't unnecessarily serialize
            // the rest of the directory.
            if (fileSerializedData.size() > maxSerializedSize)
            {
                error = FMERR_TOO_MUCH_FILES;
                break;
            }

            fileCount++;
        }
    }

    // check if the serialized data are too big
    if (fileSerializedData.size() >=
        (4096 - sizeof(error) - sizeof(fileCount) - sizeof(uint16_t)))
        error = FMERR_TOO_MUCH_FILES;

    Packet pkt = INIT_PACKET(SMSG_LS_REMOTE);
    pkt << error;
    pkt << fileCount;

    if (error == FMERR_OK)
        pkt << fileSerializedData;

    pkt.sendToSSLClient(client->ssl);
}

void Server::CallHandlerOnFileUpload(ClientSocket *client)
{
    uint8_t error = FMERR_OK;
    uint8_t filename_length;

    if (!ReadClientSSLData(client, &filename_length, sizeof(filename_length)))
        error = FMERR_INVALID_FILENAME;

    std::string filename(filename_length, '\0');

    if (error == FMERR_OK)
        if (!ReadClientSSLData(client, filename.data(), filename.size()))
            error = FMERR_INVALID_FILENAME;

    uint64_t file_size;

    if (error == FMERR_OK)
        if (!ReadClientSSLData(client, &file_size, sizeof(file_size)))
            error = FMERR_INVALID_FILENAME;

    // Important: validate this before creating the file.
    // At minimum, reject path traversal.
    if (filename.empty() || filename == "." || filename == ".." ||
        filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos)
    {
        error = FMERR_INVALID_FILENAME;
    }

    std::filesystem::path path;
    if (error == FMERR_OK)
    {
        path = std::filesystem::path(fm_remote_dir) / filename;

        if (std::filesystem::exists(path))
            error = FMERR_REMOTE_FILE_EXISTS;
    }

    /* @todo @fixme
    make a multi-packet upload and a read-lock client-side
    if the response packet is send during an upload client-side, the client
    crashes to reproduce: send a big file (more than 3 chunks) already present
    in the remote directory
    */

    if (error == FMERR_OK)
    {
        std::ofstream file(path, std::ios::binary);

        if (!file)
            error = FMERR_CANT_CREATE_FILE;

        char buffer[FILE_CHUNK_SIZE];

        std::uintmax_t remaining = file_size;

        while (remaining > 0 && error == FMERR_OK)
        {
            const std::size_t chunk = static_cast<std::size_t>(
                std::min<std::uintmax_t>(remaining, sizeof(buffer)));

            ReadClientSSLData(client, buffer, chunk);

            file.write(buffer, chunk);

            if (!file)
                error = FMERR_WRITING_FILE;

            remaining -= chunk;
        }
    }

    // send response packet
    Packet pkt = INIT_PACKET(SMSG_UPLOAD_ERR);
    pkt << error;
    pkt.sendToSSLClient(client->ssl);
}
