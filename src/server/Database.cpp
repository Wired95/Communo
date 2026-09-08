#include "Database.h"

#include <iostream>

void Database::loadRooms(std::unordered_map<uint8_t, ChatRoom> &chatRooms)
{
    const char *sql = "SELECT id, name, password_hash "
                      "FROM chat_rooms;";

    sqlite3_stmt *stmt = nullptr;

    // Prepare query
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "Failed to prepare query: " << sqlite3_errmsg(m_db)
                  << '\n';
    }

    // Iterate through rows
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const auto roomID = static_cast<uint8_t>(sqlite3_column_int(stmt, 0));

        const char *name =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));

        const char *passwordHash =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));

        ChatRoom room;
        room.name = name ? name : "";
        room.passwordHash = passwordHash ? passwordHash : "";

        chatRooms.emplace(roomID, std::move(room));
    }

    sqlite3_finalize(stmt);
}
