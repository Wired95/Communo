#ifndef _UPLOAD_MGR_H_
#define _UPLOAD_MGR_H_

#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "FileUtils.h"
#include "SharedDefinitions.h"
#include "Singleton.h"

struct Upload
{
    File file;
    eUploadStatus status;
    std::ofstream fileStream;

    std::uint64_t totalChunks;
    std::unordered_set<std::uint64_t> receivedChunks;
};

using UploadToken = std::array<std::uint8_t, UPLOAD_TOKEN_LENGTH>;
using UploadChunk = std::array<std::uint8_t, FILE_CHUNK_SIZE>;

class UploadMgr
{
  public:
    UploadToken initiateNewUpload(const std::string filename, uint64_t size,
                                  eFileManagerErr &fmErr);

    eUploadStatus getUploadStatus(UploadToken const token);
    eUploadStatus getUploadStatus(std::string const token);

    void processChunk(UploadToken const token, uint64_t chunkID,
                      UploadChunk const &chunk);
    void processChunk(std::string const token, uint64_t chunkID,
                      UploadChunk const &chunk);

    std::string uploadTokenToString(const UploadToken &token);
    UploadToken stringToUploadToken(const std::string &str);

  private:
    UploadToken generateUploadToken();

    // hash function to index the uploads by their UploadToken
    struct UploadTokenHash
    {
        std::size_t operator()(const UploadToken &token) const noexcept
        {
            std::size_t hash = 0;

            for (const auto byte : token)
            {
                hash ^= static_cast<std::size_t>(byte) + 0x9e3779b9 +
                        (hash << 6) + (hash >> 2);
            }

            return hash;
        }
    };

    std::unordered_map<UploadToken, Upload, UploadTokenHash> m_uploads;
};

// Define UploadMgr singleton
static Singleton2<UploadMgr> __UploadMgr;
#define sUploadMgr __UploadMgr.getInstance()

#endif // _UPLOAD_MGR_H_
