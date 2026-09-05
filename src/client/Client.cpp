#include "DebugUtils.h"
#include "Client.h"
#include "OpCodes.h"

#include <stdio.h> 
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>
#include <vector>
#include <poll.h>
#include <cerrno>
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>
#include <cstdlib>

Client::~Client()
{
    int iResult = shutdown(m_Sock, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        std::cout << "shutdown failed with error: " << GET_WIN_CONNEC_ERR_CODE << std::endl;
    }

    CLOSE_SOCKET(m_Sock);
    CLEANUP_SOCK;

    std::cout << "Freeing socket: " << m_Sock << std::endl;

    if (m_ctx)
    {
        SSL_CTX_free(m_ctx);
    }

    if (m_ssl)
    {
        SSL_shutdown(m_ssl);
        SSL_free(m_ssl);
    }
}

bool Client::initClientConnection()
{
    INIT_SOCK;
    struct sockaddr_in serv_addr;
    
    if ((m_Sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        throw CommunoException(CommunoException::err_socket_creation, false, std::to_string(m_Sock));
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form 
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return false;
    }

    if (connect(m_Sock, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) < 0)
    {
        std::cout << "Connection Failed: " << GET_WIN_CONNEC_ERR_CODE << std::endl;
        return false;
    }

    return true;
}

SSL* create_tls_connection(
    SSL_CTX* ctx,
    int socket_fd,
    const char* hostname)
{
    SSL* ssl = SSL_new(ctx);

    if (ssl == nullptr) {
        ERR_print_errors_fp(stderr);
        return nullptr;
    }

    // Attach the existing TCP socket.
    if (SSL_set_fd(ssl, socket_fd) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        return nullptr;
    }

    // Tell OpenSSL which hostname we're connecting to.
    //
    // This is important because certificate validation should verify
    // that the certificate is actually issued for this server.
    if (SSL_set1_host(ssl, hostname) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        return nullptr;
    }

    // Perform the TLS handshake.
    if (SSL_connect(ssl) != 1) {
        int error = SSL_get_error(ssl, -1);

        std::cerr << "TLS handshake failed, error = "
                  << error << '\n';

        ERR_print_errors_fp(stderr);

        SSL_free(ssl);
        return nullptr;
    }

    // Verify that the certificate verification performed by OpenSSL
    // succeeded.
    long verify_result = SSL_get_verify_result(ssl);

    if (verify_result != X509_V_OK) {
        std::cerr
            << "Server certificate verification failed: "
            << X509_verify_cert_error_string(verify_result)
            << '\n';

        SSL_free(ssl);
        return nullptr;
    }

    std::cout << "TLS handshake successful\n";
    std::cout << "TLS version: "
              << SSL_get_version(ssl)
              << '\n';

    return ssl;
}

bool Client::initTLS()
{
    // Create a client-side TLS context.
    m_ctx = SSL_CTX_new(TLS_client_method());

    if (m_ctx == nullptr) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Require TLS 1.3.
    if (SSL_CTX_set_min_proto_version(m_ctx, TLS1_3_VERSION) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(m_ctx);
        return false;
    }

    // Require certificate verification.
    SSL_CTX_set_verify(m_ctx, SSL_VERIFY_PEER, nullptr);

    // Load the CA trust store.
    //
    // tls_ca_file should contain the CA certificate(s) that
    // the server certificate chains back to.
    //
    // The second argument is the CA file.
    // The third argument can specify a directory containing
    // hashed CA certificates.
    if (SSL_CTX_load_verify_locations(
            m_ctx,
            tls_ca_file,
            nullptr) != 1) {

        std::cerr << "Failed to load CA trust store\n";
        ERR_print_errors_fp(stderr);

        SSL_CTX_free(m_ctx);
        return false;
    }
    else
    {
        std::cout << "CA certificate loaded" << std::endl;
    }

    // Disable TLS compression.
    SSL_CTX_set_options(m_ctx, SSL_OP_NO_COMPRESSION);

    // start SSL handshake
    m_ssl = create_tls_connection(
        m_ctx,
        m_Sock,
        "localhost"
    );

    if (m_ssl == nullptr) {
        close(m_Sock);
        SSL_CTX_free(m_ctx);
        return false;
    }

    return true;
}

void Client::processReplyFromServerIfAny()
{
    char buffer[4096] = {0};

    struct pollfd pfd;
    pfd.fd = static_cast<int>(m_Sock);
    pfd.events = POLLIN;
    pfd.revents = 0;

    // Don't block: just check whether data is available.
    int pollResult = poll(&pfd, 1, 0);

    if (pollResult < 0)
    {
        if (errno == EINTR)
            return;

        std::cerr << "poll() failed: "
                  << std::strerror(errno)
                  << '\n';
        return;
    }

    if (pollResult == 0)
    {
        // No data available.
        return;
    }

    if (pfd.revents & POLLNVAL)
    {
        std::cerr << "Invalid socket\n";
        return;
    }

    if (pfd.revents & POLLERR)
    {
        std::cerr << "Socket error\n";
        exit(1);
        return;
    }

    if (!(pfd.revents & (POLLIN | POLLHUP)))
        return;

    // TLS read
    int valread = SSL_read(
        m_ssl,
        buffer,
        static_cast<int>(sizeof(buffer))
    );

    if (valread <= 0)
    {
        int sslError = SSL_get_error(m_ssl, valread);

        switch (sslError)
        {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                // Non-blocking SSL connection: try again later.
                return;

            case SSL_ERROR_ZERO_RETURN:
                std::cout << "Server disconnected\n";
                return;

            default:
                std::cerr << "SSL_read() failed. SSL error: "
                          << sslError << '\n';
                ERR_print_errors_fp(stderr);
                return;
        }
    }

    // We received valread bytes.
    if (valread >= static_cast<int>(sizeof(uint16_t)))
    {
        uint16_t opcode;

        std::memcpy(&opcode, buffer, sizeof(opcode));
        opcode = ntohs(opcode);

        std::string payload(
            buffer + sizeof(opcode),
            valread - sizeof(opcode)
        );

        switch (opcode)
        {
            case SMSG_ECHO_REQUEST:
                std::cout << "\rReceived echo "
                          << OPCODE_STR(SMSG_ECHO_REQUEST)
                          << ": " << payload
                          << '\n' << std::flush;
                break;

            case SMSG_MESSAGE:
                std::cout << "\rReceived message "
                          << OPCODE_STR(SMSG_MESSAGE)
                          << ": " << payload
                          << '\n' << std::flush;
                break;

            case SMSG_MOTD:
                std::cout << "\rReceived MOTD "
                          << OPCODE_STR(SMSG_MOTD)
                          << ": " << payload
                          << '\n' << std::flush;
                break;
            case SMSG_ADDITION_REQUEST:
            {
                double value;

                if (payload.size() < sizeof(double)) {
                    // invalid / incomplete payload
                    throw std::runtime_error("Payload too small for double");
                }

                std::memcpy(&value, payload.data(), sizeof(double));

                std::cout << "\rReceived Result "
                          << OPCODE_STR(SMSG_ADDITION_REQUEST)
                          << ": " << std::to_string(value)
                          << '\n' << std::flush;
                break;
            }
            case SMSG_BROADCAST:
            {
                std::cout << "\rReceived broadcast "
                          << OPCODE_STR(SMSG_BROADCAST)
                          << ": " << payload
                          << '\n' << std::flush;
                break;
            }
            case SMSG_PONG:
            {
                auto now = std::chrono::steady_clock::now();
                auto ping_us = std::chrono::duration_cast<std::chrono::microseconds>(now - m_pingStart).count();
                std::cout << "Ping: "
                    << ping_us / 1000.0
                    << " ms\n";
                break;
            }
            case SMSG_UPTIME:
                std::cout << "\rReceived uptime "
                          << OPCODE_STR(SMSG_UPTIME)
                          << " -> " << payload
                          << '\n' << std::flush;
                break;
            case SMSG_COUNTER:
            {
                uint64_t value;
                std::memcpy(&value, payload.data(), sizeof(uint64_t));
                std::cout << "\rReceived counter "
                          << OPCODE_STR(SMSG_COUNTER)
                          << " -> " << value
                          << '\n' << std::flush;
                break;
            }
            default:
                std::cout << "\rReceived unknown opcode: "
                          << opcode
                          << std::flush;
                break;
        }
    }
    else
    {
        std::cout << "Invalid packet from server received\n";
    }
}

void Client::sendSSLPacketToServer(const std::string& packet)
{
    int sent = SSL_write(
        m_ssl,
        packet.data(),
        static_cast<int>(packet.size())
    );

    if (sent <= 0)
    {
        int sslError = SSL_get_error(m_ssl, sent);

        std::cerr << "SSL_write() failed. SSL error: "
                  << sslError << '\n';

        ERR_print_errors_fp(stderr);
        return;
    }

    if (sent != static_cast<int>(packet.size()))
    {
        std::cerr << "SSL_write() sent only "
                  << sent << " of "
                  << packet.size()
                  << " bytes\n";
    }
}

void Client::sendEchoRequest(std::string msg)
{
    std::string packet;

    uint16_t opcode = htons(CMSG_ECHO_REQUEST);
    packet.append(
        reinterpret_cast<const char*>(&opcode),
        sizeof(opcode)
    );

    packet += msg;

    sendSSLPacketToServer(packet);
}

void Client::sendAdditionRequest(const std::vector<Number> numbers)
{
    std::string packet;

    if (numbers.size() < 2)
    {
        std::cerr << "Not enough numbers to send, aborting server call..." << std::endl;
        return;
    }

    // Write opcode
    uint16_t opcode = htons(CMSG_ADDITION_REQUEST);
    packet.append(
        reinterpret_cast<const char*>(&opcode),
        sizeof(opcode)
    );

    // Write packet like [type1][num1 bytes][type2][num2 bytes]...
    for (Number num : numbers)
    {
        // Write number type
        eNumberTypes type = get_number_type(num);
        packet.push_back(static_cast<char>(type));

        // Write number bytes
        append_number(packet, num);
    }

    if (packet.size() > 4096)
    {
        std::cerr << "Too much numbers to send, aborting server call..." << std::endl;
        return;
    }

    sendSSLPacketToServer(packet);
}

void Client::sendBroadcast(std::string const msg)
{
    std::string packet;

    uint16_t opcode = htons(CMSG_BROADCAST_MESSAGE);
    packet.append(
        reinterpret_cast<const char*>(&opcode),
        sizeof(opcode)
    );

    packet += msg;

    sendSSLPacketToServer(packet);
}

void Client::sendPing()
{
    std::string packet;
    uint16_t opcode = htons(CMSG_PING);
    packet.append(
        reinterpret_cast<const char*>(&opcode),
        sizeof(opcode)
    );

    // Store initial ping start
    m_pingStart = std::chrono::steady_clock::now();

    sendSSLPacketToServer(packet);
}

void Client::sendUptime()
{
    std::string packet;
    uint16_t opcode = htons(CMSG_UPTIME);
    packet.append(
        reinterpret_cast<const char*>(&opcode),
        sizeof(opcode)
    );

    sendSSLPacketToServer(packet);
}

void Client::sendIncrementCounter()
{
    std::string packet;
    uint16_t opcode = htons(CMSG_INCREMENT_COUNTER);
    packet.append(
        reinterpret_cast<const char*>(&opcode),
        sizeof(opcode)
    );

    sendSSLPacketToServer(packet);
}

void Client::sendGetCounter()
{
    std::string packet;
    uint16_t opcode = htons(CMSG_GET_COUNTER);
    packet.append(
        reinterpret_cast<const char*>(&opcode),
        sizeof(opcode)
    );

    sendSSLPacketToServer(packet);
}
