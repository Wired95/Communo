#ifndef _SHAREDDEFS_H_
#define _SHAREDDEFS_H_

/* Shared Definitions
 * 1. Network configuration
 * 2. Server daemon settings
 * 3. SSL Settings
 * 4. Chat settings
 */

/// 1. Network configuration

#define PORT 36987
#define SERVER_IP "127.0.0.1"

/// 2. Server daemon settings

#define SERVER_DAEMON_ID std::string("communo-server")
#define SERVER_DAEMON_NAME std::string("Communo Server")

/// 3. SSL Settings

inline constexpr const char *tls_ca_file = TLS_CA_FILE;
inline constexpr const char *tls_key_file = TLS_KEY_FILE;
inline constexpr const char *tls_cert_file = TLS_CERT_FILE;

/// 4. Chat settings

#include <cstdint> // for uint8_t

enum eChatErr : uint8_t
{
    ERR_OK,
    ERR_INVALID_ROOM,
    ERR_INVALID_ROOM_PASSWORD,
    ERR_INVALID_PACKET,
    ERR_NO_ROOM_JOINED,
    ERR_NO_CLIENT_FOUND,
    ERR_MSG_TO_SELF,
    ERR_TOO_MUCH_CLIENTS,
    ERR_EMPTY_MESSAGE,
};

#endif // _SHAREDDEFS_H_
