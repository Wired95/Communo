#ifndef _DATABASE_H_
#define _DATABASE_H_

#include "Chat.h"

#include <sqlite3.h>
#include <stdexcept>

// Given by CMake
inline constexpr const char *sqlite_db_file = SQLITE_DB_FILE;

class Database
{
  public:
    Database()
    {
        if (sqlite3_open(sqlite_db_file, &m_db) != SQLITE_OK)
        {
            std::string error = sqlite3_errmsg(m_db);
            sqlite3_close(m_db);
            m_db = nullptr;
            throw std::runtime_error(error);
        }

        // Wait up to 5 seconds if the database is locked
        sqlite3_busy_timeout(m_db, 5000);
    }

    ~Database()
    {
        if (m_db)
            sqlite3_close(m_db);
    }

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    void loadRooms(std::unordered_map<uint8_t, ChatRoom> &chatRooms);

  private:
    sqlite3 *m_db = nullptr;
};

// Define Database singleton
static Singleton2<Database> __Database;
#define sDatabase __Database.getInstance()

#endif // _DATABASE_H_
