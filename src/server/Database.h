#ifndef _DATABASE_H_
#define _DATABASE_H_

// Given by CMake
inline constexpr const char* sqlite_db_file = SQLITE_DB_FILE;

class Database
{
public:
    Database() : m_DatabaseReady(false) {}

private:
    bool m_DatabaseReady;
};

#endif // _DATABASE_H_