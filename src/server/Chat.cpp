#include "Chat.h"

#include <iostream>
#include <cstring>

#include <openssl/sha.h>

std::string Chat::getChatRoomsStr() const
{
    std::string rooms;
    for (size_t i = 0; i < MAX_CHAT_ROOMS; ++i)
    {
        const auto& room = m_ChatRooms[i];

        rooms += '[' + std::to_string(room.id) + "] " + room.name;
        if (room.password.empty() || room.password == "")
            rooms += " (open)";
        else
            rooms += " (protected)";

        if ((i + 1) < MAX_CHAT_ROOMS)
            rooms += '\n';
    }
    return rooms;
}

bool Chat::checkPasswordHash(uint8_t roomID, const unsigned char* hash)
{
    bool valid = false;

    // Get room password
    unsigned char roomHash[SHA256_DIGEST_LENGTH];
    const std::string& password = m_ChatRooms[roomID].password;
    SHA256(reinterpret_cast<const unsigned char*>(password.data()), password.size(), roomHash);
    
    // Compare hash
    if (std::memcmp(roomHash, hash, SHA256_DIGEST_LENGTH) == 0)
        valid = true;

    return valid;
}
