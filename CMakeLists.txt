# Project base

cmake_minimum_required (VERSION 3.14)

project (Communo)

set (PROJ_VERSION_MAJOR 1)
set (PROJ_VERSION_MINOR 0)

# CXX Standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenSSL Requirements
find_package(OpenSSL REQUIRED)
find_program(OPENSSL_EXECUTABLE
    NAMES openssl
    REQUIRED
)

# SQLite building
add_library(sqlite3 STATIC
    third_party/sqlite/sqlite3.c
)

target_include_directories(sqlite3
    PUBLIC
        third_party/sqlite
)

# Build configuration
set(CMAKE_CONFIGURATION_TYPES Debug Release)

if (UNIX)
    set(BuildType "all" CACHE STRING "BuildType chosen by the user at CMake configure time")
else()
    set(BuildType "client" CACHE STRING "BuildType chosen by the user at CMake configure time")
endif()

set_property(CACHE BuildType PROPERTY STRINGS server client all)

# Common scripts
add_library(Common STATIC
    src/common/DebugUtils.cpp
    src/common/DebugUtils.h
    src/common/NetworkHeaders.h
    src/common/SharedDefinitions.h
    src/common/OpCodes.h
    src/common/Logging.h
    src/common/Logging.cpp
    src/common/NumParser.h
    src/common/NumParser.cpp
    src/common/Singleton.h
)

# Server scripts
if(${BuildType} STREQUAL "server" OR ${BuildType} STREQUAL "all")
    add_executable (Server 
        src/server/Chat.cpp
        src/server/Chat.h
        src/server/Database.cpp
        src/server/Database.h
        src/server/Main.cpp
        src/server/Server.cpp
        src/server/Server.h
        src/server/Universe.cpp
        src/server/Universe.h
    )

    target_link_libraries(Server Common)

    target_link_libraries(Server
        Common
        OpenSSL::SSL
        OpenSSL::Crypto
        sqlite3
    )

    target_include_directories(Server PUBLIC 
        ${PROJECT_SOURCE_DIR}/src/common/
        ${PROJECT_SOURCE_DIR}/src/server/
    )

    if(WIN32)
        message("Warning: The server should run on UNIX environments!")
    endif()
endif()

# Client scripts
if(${BuildType} STREQUAL "client" OR ${BuildType} STREQUAL "all")
	add_executable (Client 
        src/client/Main.cpp
        src/client/Client.cpp
        src/client/Client.h
    )

    target_link_libraries(Client
        Common
        OpenSSL::SSL
        OpenSSL::Crypto
    )

    target_include_directories(Client PUBLIC 
        ${PROJECT_SOURCE_DIR}/src/common/
        ${PROJECT_SOURCE_DIR}/src/client/
    )

endif()

if(WIN32)
        target_link_libraries(Server wsock32 ws2_32)
        target_link_libraries(Client wsock32 ws2_32)
endif()

# Self-signed certificates generation
include(cmake/SelfSignedCertificates.cmake)

# Database creation and loading
include(cmake/Database.cmake)


