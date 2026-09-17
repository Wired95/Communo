#ifndef _SHAREDDEFS_H_
#define _SHAREDDEFS_H_

/* Shared Definitions
 * 1. Network configuration
 * 2. Server daemon settings
 * 3. SSL Settings
 * 4. Chat settings
 * 5. File management settings
 */

/// 1. Network configuration

#define PORT 36987
#define SERVER_IP "127.0.0.1"

/// 2. Server daemon settings

#define SERVER_DAEMON_ID std::string("communo-server")
#define SERVER_DAEMON_NAME std::string("Communo Server")

/// 3. SSL Settings

inline constexpr const char *tls_ca_file   = TLS_CA_FILE;
inline constexpr const char *tls_key_file  = TLS_KEY_FILE;
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
    ERR_USERNAME_TOO_LONG,
    ERR_USERNAME_INVALID_CHAR,
};

#define MAX_USERNAME_LENGTH 32

/// 5. File management settings

inline constexpr const char *fm_local_dir  = FM_LOCAL_DIR;
inline constexpr const char *fm_remote_dir = FM_REMOTE_DIR;

// Warning: for simple messages, keep the message length
//          below 16kb - data header
// todo: append packet / message length to the packet
//       for wider packets and server post-processing
#define FILE_CHUNK_SIZE (14 * 1024) // 14 kb

#define UPLOAD_TOKEN_LENGTH 32

enum eFileManagerErr : uint8_t
{
    FMERR_OK,
    FMERR_TOO_MUCH_FILES,
    FMERR_FILENAME_TOO_LONG,
    FMERR_INVALID_FILENAME,
    FMERR_NOT_ENOUGH_SPACE,
    FMERR_CANT_CREATE_FILE,
    FMERR_REMOTE_FILE_EXISTS,
    FMERR_WRITING_FILE,
};

enum eUploadStatus : uint8_t
{
    UPLOAD_NOT_STARTED,
    UPLOAD_NOT_FOUND,
    UPLOAD_IN_PROGRESS,
    UPLOAD_COMPLETE,
};

#endif // _SHAREDDEFS_H_
