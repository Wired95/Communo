
message(STATUS "Building self-signed certificates...")

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

message(STATUS "Building self-signed certificates completed.")
