#ifndef _OPCODES_H_
#define _OPCODES_H_

#define OPCODE_STR(x) \
    std::string("[") << #x << " (0x" \
              << std::uppercase << std::hex \
              << std::setw(4) << std::setfill('0') \
              << static_cast<uint16_t>(x) \
              << ")]"

enum eOpcodes : uint16_t
{
    OPCODE_START            = 0x0000,

    // Client messages
    CMSG_ECHO_REQUEST       = 0x0001,
    CMSG_ADDITION_REQUEST   = 0x0002,

    // Server messages
    SMSG_MOTD               = 0x0FFF,
    SMSG_MESSAGE            = 0x1000,

    SMSG_ECHO_REQUEST       = 0x1001,
    SMSG_ADDITION_REQUEST   = 0x1002,

    

    OPCODE_MAX              = (0x7FFF+1),
};

#endif // _OPCODES_H_
