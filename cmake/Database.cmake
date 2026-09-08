
message(STATUS "Building application database...")

# SQLite CLI requirement
find_program(OPENSSL_EXECUTABLE
    NAMES sqlite3
    REQUIRED
)

set(DB_FILE "${CMAKE_CURRENT_BINARY_DIR}/database.db")

# Cleanup DB files
file(REMOVE
    "${DB_FILE}"
    "${DB_FILE}-wal"
    "${DB_FILE}-shm"
    "${DB_FILE}-journal"
)

# Generate the SQLite database file from initial SQL files
execute_process(
    COMMAND sqlite3
            "${DB_FILE}"
            ".read ${CMAKE_SOURCE_DIR}/sql/chat_rooms.sql"

    COMMAND_ERROR_IS_FATAL ANY
)

# Set definitions for the server
target_compile_definitions(Server PRIVATE
    SQLITE_DB_FILE="${DB_FILE}"
)

message(STATUS "Building application database completed.")
