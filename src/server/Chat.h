#ifndef _CHAT_H_
#define _CHAT_H_

#include "SharedDefinitions.h"
#include "Singleton.h"

#include <cstdint>
#include <string>
#include <unordered_map>

struct ChatRoom
{
    ChatRoom() {}

    ChatRoom(std::string _name, std::string _passwordHash)
        : name(_name), passwordHash(_passwordHash)
    {
    }
    std::string name;
    std::string passwordHash; // todo : store hash in memory instead
};

class Chat
{
  public:
    Chat() {}

    void loadChatRooms();

    std::string getChatRoomsStr() const;

    std::string getRoomName(uint8_t roomID) const
    {
        return m_ChatRooms.at(roomID).name;
    }

    bool checkRoomID(uint8_t roomID)
    {
        return m_ChatRooms.find(roomID) != m_ChatRooms.end();
    }

    bool isRoomProtected(uint8_t roomID)
    {
        return !m_ChatRooms[roomID].passwordHash.empty();
    }

    bool checkPasswordHash(uint8_t roomID, const unsigned char *hash);

  private:
    std::unordered_map<uint8_t, ChatRoom> m_ChatRooms;
};

// Define Chat singleton
static Singleton2<Chat> __Chat;
#define sChat __Chat.getInstance()

#endif // _CHAT_H_
