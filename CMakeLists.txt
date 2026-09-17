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
    src/common/FileUtils.cpp
    src/common/FileUtils.h
    src/common/NetworkHeaders.h
    src/common/SharedDefinitions.h
    src/common/OpCodes.h
    src/common/Logging.h
    src/common/Logging.cpp
    src/common/NumParser.h
    src/common/NumParser.cpp
    src/common/Packet.cpp
    src/common/Packet.h
    src/common/Singleton.h
)

# Server scripts
if(${BuildType} STREQUAL "server" OR ${BuildType} STREQUAL "all")

    set(SERVER_DIR ${PROJECT_SOURCE_DIR}/src/server)

    # Find all server sources.
    file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS
        ${SERVER_DIR}/*.h
        ${SERVER_DIR}/*.hpp
        ${SERVER_DIR}/*.cpp
    )

    add_executable (Server
        ${SERVER_SOURCES}
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

    set(CLIENT_DIR ${PROJECT_SOURCE_DIR}/src/client)

    # Find all cient sources.
    file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS
        ${CLIENT_DIR}/*.h
        ${CLIENT_DIR}/*.hpp
        ${CLIENT_DIR}/*.cpp
    )

    add_executable (Client
        ${CLIENT_SOURCES}
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

# Tests scripts
if(${BuildType} STREQUAL "tests" OR ${BuildType} STREQUAL "all")
    set(TESTS_DIR ${PROJECT_SOURCE_DIR}/src/tests)
    set(DEFAULT_TEST_CPP ${CMAKE_CURRENT_BINARY_DIR}/test.cpp)

    # Find all test sources.
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        ${TESTS_DIR}/*.h
        ${TESTS_DIR}/*.hpp
        ${TESTS_DIR}/*.cpp
    )

    # If the user has not provided a Main.cpp, generate a default one.
    file(GLOB_RECURSE TEST_MAIN_FILES CONFIGURE_DEPENDS
        ${TESTS_DIR}/Main.cpp
    )

    if(NOT TEST_MAIN_FILES)
        file(WRITE ${DEFAULT_TEST_CPP}
"// Auto-generated default test entry point.\n\
int main()\n\
{\n\
    return 0;\n\
}\n")
        list(APPEND TEST_SOURCES ${DEFAULT_TEST_CPP})
    endif()

    add_executable(Tests)

    target_sources(Tests PRIVATE
        ${TEST_SOURCES}
    )

    target_link_libraries(Tests
        Common
        OpenSSL::SSL
        OpenSSL::Crypto
        sqlite3
    )

    target_include_directories(Tests PUBLIC
        ${PROJECT_SOURCE_DIR}/src/common/
        ${TESTS_DIR}
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

# Datasets for the file manager
include(cmake/FileManager.cmake)
