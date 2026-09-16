#ifndef _PACKET_H_
#define _PACKET_H_

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string_view>

#include <openssl/ssl.h>

#include "NumParser.h"
#include "OpCodes.h"

#define INIT_PACKET(X) Packet(X, #X)

class Packet
{
  public:
    Packet() = delete;

    Packet(eOpcodes opcode, std::string_view name);

    Packet(const void *buffer, std::size_t size)
        : m_data(static_cast<const char *>(buffer), size), m_offset(0)
    {
    }

    template <typename T> Packet &operator<<(const T &var)
    {
        m_data.append(reinterpret_cast<const char *>(&var), sizeof(var));

        return *this;
    }

    Packet &operator<<(std::string_view str)
    {
        m_data.append(str.data(), str.size());

        return *this;
    }

    Packet &operator<<(std::string str)
    {
        m_data.append(str.data(), str.size());

        return *this;
    }

    Packet &operator<<(const char *str)
    {
        return operator<<(std::string_view(str));
    }

    Packet &operator<<(const Number num);

    template <typename T> Packet &operator>>(T &value)
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Packet::operator>> requires a trivially copyable type");

        if (m_offset + sizeof(T) > m_data.size())
            throw std::runtime_error("Packet: not enough data");

        std::memcpy(&value, m_data.data() + m_offset, sizeof(T));

        m_offset += sizeof(T);

        return *this;
    }

    Packet &operator>>(std::string &value)
    {
        const std::size_t size = value.size();

        if (m_offset + size > m_data.size())
            throw std::runtime_error("Packet: not enough data");

        std::memcpy(value.data(), m_data.data() + m_offset, size);

        m_offset += size;

        return *this;
    }

    Packet &operator>>(eNumberTypes &type);
    Packet &operator>>(Number &num);

    const std::byte *data() const
    {
        return reinterpret_cast<const std::byte *>(this->m_data.data());
    }

    std::size_t size() const { return this->m_data.size(); }

    std::string fancy_name() const
    {
        std::ostringstream ss;
        ss << "[" << m_name << " (0x" << std::uppercase << std::hex
           << static_cast<uint16_t>(m_opcode) << ")]";
        return ss.str();
    }

    bool canRead() const { return this->size() > this->m_offset; }

    void sendToSSLClient(SSL *ssl);

  private:
    uint16_t m_opcode;
    std::string_view m_name;
    std::string m_data;
    std::size_t m_offset = 0;

    void read(void *destination, std::size_t size)
    {
        if (m_offset + size > m_data.size())
            throw std::runtime_error("Packet: not enough data");

        std::memcpy(destination, m_data.data() + m_offset, size);
        m_offset += size;
    }
};

#endif // _PACKET_H_
