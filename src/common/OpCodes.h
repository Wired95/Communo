#ifndef _OPCODES_H_
#define _OPCODES_H_

#define OPCODE_STR(x)                                                          \
    std::string("[") << #x << " (0x" << std::uppercase << std::hex             \
                     << std::setw(4) << std::setfill('0')                      \
                     << static_cast<uint16_t>(x) << ")]"

enum eOpcodes : uint16_t
{
    OPCODE_START             = 0x0000,

    // Client messages
    CMSG_ECHO_REQUEST        = 0x0001,

    CMSG_ADDITION_REQUEST    = 0x0002,

    CMSG_BROADCAST_MESSAGE   = 0x0003,

    CMSG_GET_CLIENT_LIST     = 0x0004,
    CMSG_SEND_MSG_TO_CLIENT  = 0x0005,
    CMSG_CHANGE_USERNAME     = 0x0006, // todo (server-side)

    CMSG_UPTIME              = 0x0007,
    CMSG_PING                = 0x0008,

    CMSG_INCREMENT_COUNTER   = 0x0009,
    CMSG_GET_COUNTER         = 0x000A,

    CMSG_GET_CHAT_ROOMS      = 0x000B,
    CMSG_GET_ROOM_INFO       = 0x000C,
    CMSG_JOIN_ROOM           = 0x000D,
    CMSG_SAY                 = 0x000E,

    CMSG_LS_REMOTE           = 0x000F,
    CMSG_UPLOAD_FILE         = 0x0010,
    CMSG_DOWNLOAD_FILE       = 0x0011, // todo

    // Server messages
    SMSG_MOTD                = 0x0FFF,
    SMSG_MESSAGE             = 0x1000,

    SMSG_ECHO_REQUEST        = 0x1001,

    SMSG_ADDITION_REQUEST    = 0x1002,

    SMSG_BROADCAST           = 0x1003,

    SMSG_CLIENT_LIST         = 0x1004,
    SMSG_PRIVATE_MSG_ERR     = 0x1005,
    SMSG_PRIVATE_MESSAGE     = 0x1006,
    CMSG_CHANGE_USERNAME_ERR = 0x1007, // todo

    SMSG_UPTIME              = 0x1008,
    SMSG_PONG                = 0x1009,

    SMSG_COUNTER             = 0x100A,

    SMSG_JOIN_CHAT_ROOM_OK   = 0x100B,
    SMSG_JOIN_CHAT_ROOM_ERR  = 0x100C,
    SMSG_SAY_OK              = 0x100D,
    SMSG_SAY_ERR             = 0x100E,
    SMSG_SAY                 = 0x100F,

    SMSG_LS_REMOTE           = 0x1010,
    SMSG_UPLOAD_ERR          = 0x1011, // todo
    SMSG_DOWNLOAD            = 0x1012, // todo

    OPCODE_MAX               = (0x7FFF + 1),
};

#endif // _OPCODES_H_
