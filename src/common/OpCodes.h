#ifndef _OPCODES_H_
#define _OPCODES_H_

#define OPCODE_STR(x) \
    std::string("[") << #x << " (0x" \
              << std::uppercase << std::hex \
              << std::setw(4) << std::setfill('0') \
              << static_cast<uint16_t>(x) \
              << ")]"

enum eOpcodes : unsigned short int
{
    OPCODE_START            = 0x0000,

    // Client messages
    CMSG_ECHO_REQUEST       = 0x0001,

    // Server messages
    SMSG_ECHO_REQUEST       = 0x0FFF,
    SMSG_MESSAGE            = 0x1000,
    SMSG_MOTD               = 0x1001,
    

    OPCODE_MAX              = (0x7FFF+1),
};

#endif // _OPCODES_H_
