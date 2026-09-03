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
)

# Server scripts
if(${BuildType} STREQUAL "server" OR ${BuildType} STREQUAL "all")
    add_executable (Server 
        src/server/Main.cpp
        src/server/Server.cpp
        src/server/Server.h
    )

    target_link_libraries(Server Common)

    target_link_libraries(Server
        Common
        OpenSSL::SSL
        OpenSSL::Crypto
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

# Certificates generation



set(TLS_DIR "${CMAKE_CURRENT_BINARY_DIR}/tls")
file(MAKE_DIRECTORY "${TLS_DIR}")

# CA key + self-signed CA certificate
execute_process(
    COMMAND "${OPENSSL_EXECUTABLE}"
            req -x509 -newkey rsa:2048 -nodes
            -keyout "${TLS_DIR}/ca.key"
            -out "${TLS_DIR}/server-ca.pem"
            -days 3650
            -subj "/CN=My Local CA"
    COMMAND_ERROR_IS_FATAL ANY
)

# Server key + CSR
execute_process(
    COMMAND "${OPENSSL_EXECUTABLE}"
            req -new -newkey rsa:2048 -nodes
            -keyout "${TLS_DIR}/server.key"
            -out "${TLS_DIR}/server.csr"
            -subj "/CN=localhost"
    COMMAND_ERROR_IS_FATAL ANY
)

# Sign server certificate with CA
execute_process(
    COMMAND "${OPENSSL_EXECUTABLE}"
            x509 -req
            -in "${TLS_DIR}/server.csr"
            -CA "${TLS_DIR}/server-ca.pem"
            -CAkey "${TLS_DIR}/ca.key"
            -CAcreateserial
            -out "${TLS_DIR}/server.cert"
            -days 3650
            -sha256
    COMMAND_ERROR_IS_FATAL ANY
)

file(REMOVE
    "${TLS_DIR}/server.csr"
    "${TLS_DIR}/server-ca.pem.srl"
)

set(TLS_CA     "${TLS_DIR}/server-ca.pem")
set(TLS_KEY    "${TLS_DIR}/server.key")
set(TLS_CERT   "${TLS_DIR}/server.cert")

target_compile_definitions(Common PRIVATE
    TLS_CA_FILE="${TLS_CA}"
    TLS_KEY_FILE="${TLS_KEY}"
    TLS_CERT_FILE="${TLS_CERT}"
)

target_compile_definitions(Server PRIVATE
    TLS_CA_FILE="${TLS_CA}"
    TLS_KEY_FILE="${TLS_KEY}"
    TLS_CERT_FILE="${TLS_CERT}"
)

target_compile_definitions(Client PRIVATE
    TLS_CA_FILE="${TLS_CA}"
    TLS_KEY_FILE="${TLS_KEY}"
    TLS_CERT_FILE="${TLS_CERT}"
)





