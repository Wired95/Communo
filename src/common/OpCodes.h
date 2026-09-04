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
    CMSG_BROADCAST_MESSAGE  = 0x0003, // todo
    CMSG_GET_CLIENT_LIST    = 0x0004, // todo
    CMSG_SEND_MSG_TO_CLIENT = 0x0005, // todo
    CMSG_UPTIME             = 0x0006, // todo
    CMSG_PING               = 0x0007, // todo
    CMSG_INCREMENT_COUNTER  = 0x0008, // todo
    CMSG_GET_COUNTER        = 0x0009, // todo

    // Server messages
    SMSG_MOTD               = 0x0FFF,
    SMSG_MESSAGE            = 0x1000,

    SMSG_ECHO_REQUEST       = 0x1001,
    SMSG_ADDITION_REQUEST   = 0x1002,
    SMSG_BROADCAST          = 0x1003, // todo
    SMSG_CLIENT_LIST        = 0x1004, // todo
    SMSG_PRIVATE_MESSAGE    = 0x1005, // todo
    SMSG_UPTIME             = 0x1006, // todo
    SMSG_PONG               = 0x1007, // todo
    SMSG_COUNTER            = 0x1008, // todo

    OPCODE_MAX              = (0x7FFF+1),
};

#endif // _OPCODES_H_
