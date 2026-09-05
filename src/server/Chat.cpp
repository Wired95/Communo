#include "Chat.h"

#include <iostream>

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
