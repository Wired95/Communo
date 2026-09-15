#include "Packet.h"
#include "Logging.h"

#include <arpa/inet.h>

#include <sstream>

Packet::Packet(eOpcodes opcode, std::string_view name)
    : m_opcode(opcode), m_name(name)
{
    uint16_t ropcode = htons(opcode);
    m_data.append(reinterpret_cast<const char *>(&ropcode), sizeof(ropcode));
}

Packet &Packet::operator<<(const Number num)
{
    // Write number type
    eNumberTypes type = get_number_type(num);
    m_data.push_back(static_cast<char>(type));

    // Write number bytes
    append_number(m_data, num);

    return *this;
}

void Packet::sendToSSLClient(SSL *ssl)
{
    // Log the packet sending
    std::stringstream connLog;
    connLog << "Sending opcode: " << this->fancy_name();
    connLog << " (size:" << this->size() << ")";
    sLog.log(LOG_FLAG_DEBUG, connLog.str());

    // Send the packet to the client socket
    int sent = SSL_write(ssl, this->data(), static_cast<int>(this->size()));

    // check return codes
    if (sent <= 0)
    {
        int sslError = SSL_get_error(ssl, sent);

        std::stringstream errorLog;
        errorLog << "SSL_write failed for " << this->fancy_name()
                 << ", SSL error: " << sslError;

        sLog.log(LOG_FLAG_DEBUG, errorLog.str());
        return;
    }
}
