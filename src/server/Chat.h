#ifndef _CHAT_H_
#define _CHAT_H_

#include "Singleton.h"
#include <string>
#include <array>
#include <cstdint>


enum eChatRooms : uint8_t
{
    ROOM_GENERAL    = 0,
    ROOM_FUN        = 1,
    ROOM_PRIVATE    = 2,

    MAX_CHAT_ROOMS,
    ROOM_NONE,
};

struct ChatRoom
{
    ChatRoom() : id(0) {}

    ChatRoom(uint8_t _id, std::string _name, std::string _password) :
        id(_id), name(_name), password(_password) {}
    uint8_t id;
    std::string name;
    std::string password;
};

class Chat
{
public:
    Chat() {}

    void loadChatRooms()
    {
        m_ChatRooms[ROOM_GENERAL]   = { ROOM_GENERAL,   "General",  "" };
        m_ChatRooms[ROOM_FUN]       = { ROOM_FUN,       "Fun",      "" };
        m_ChatRooms[ROOM_PRIVATE]   = { ROOM_PRIVATE,   "Private",  "Password" };
    }

    std::string getChatRoomsStr() const;

    bool checkRoomID(uint8_t roomID)
    {
        return roomID >= ROOM_GENERAL && roomID < MAX_CHAT_ROOMS;
    }

    bool checkPassword(uint8_t roomID, std::string pwd)
    {
        return checkRoomID(roomID) && (
            m_ChatRooms[roomID].password == "" || pwd == m_ChatRooms[roomID].password);
    }

private:
    std::array<ChatRoom, MAX_CHAT_ROOMS> m_ChatRooms;
};

// Define Chat singleton
static Singleton2<Chat> __Chat;
#define sChat           __Chat.getInstance()

#endif // _CHAT_H_