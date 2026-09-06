#include "Chat.h"

#include <iostream>
#include <cstring>

#include <openssl/sha.h>

std::string Chat::getChatRoomsStr() const
{
    std::string rooms;
    for (auto it = m_ChatRooms.begin(); it != m_ChatRooms.end(); ++it)
    {
        const auto& [roomID, room] = *it;

        rooms += '[' + std::to_string(roomID) + "] " + room.name;
        if (room.passwordHash.empty())
            rooms += " (open)";
        else
            rooms += " (protected)";

        if (std::next(it) != m_ChatRooms.end())
            rooms += '\n';
    }
    return rooms;
}

bool Chat::checkPasswordHash(uint8_t roomID, const unsigned char* hash)
{
    bool valid = false;

    // Get room password
    unsigned char roomHash[SHA256_DIGEST_LENGTH];
    const std::string& passwordHash = m_ChatRooms[roomID].passwordHash;
    std::memcpy(roomHash, passwordHash.data(), SHA256_DIGEST_LENGTH);

    // Compare hash
    if (std::memcmp(roomHash, hash, SHA256_DIGEST_LENGTH) == 0)
        valid = true;

    return valid;
}
