#include "UploadMgr.h"
#include "Logging.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

#include <openssl/rand.h>

UploadToken UploadMgr::initiateNewUpload(const std::string filename,
                                         uint64_t size, eFileManagerErr &fmErr)
{
    fmErr = FMERR_OK;

    // validate filename
    // Important: validate this before creating the file.
    // At minimum, reject path traversal.
    if (filename.empty() || filename == "." || filename == ".." ||
        filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos)
    {
        fmErr = FMERR_INVALID_FILENAME;
    }

    // Check file existence
    std::filesystem::path path;
    if (fmErr == FMERR_OK)
    {
        path = std::filesystem::path(fm_remote_dir) / filename;

        if (std::filesystem::exists(path))
            fmErr = FMERR_REMOTE_FILE_EXISTS;
    }

    // Check for available space
    const auto space = std::filesystem::space(path.parent_path());
    if (fmErr == FMERR_OK)
    {
        if (space.available < size)
            fmErr = FMERR_NOT_ENOUGH_SPACE;
    }

    // generate new upload if possible
    if (fmErr == FMERR_OK)
    {
        Upload upload;
        upload.file.size     = size;
        upload.file.filename = filename;
        upload.status        = UPLOAD_NOT_STARTED;
        upload.totalChunks =
            (upload.file.size + FILE_CHUNK_SIZE - 1) / FILE_CHUNK_SIZE;

        UploadToken token = generateUploadToken();

        std::ofstream createFile(path, std::ios::binary | std::ios::trunc);

        if (!createFile)
        {
            sLog.log(LOG_FLAG_ERROR, "Failed to create dest file");
            fmErr = FMERR_CANT_CREATE_FILE;
            return UploadToken();
        }

        upload.fileStream.open(path,
                               std::ios::in | std::ios::out | std::ios::binary);

        if (!upload.fileStream)
        {
            sLog.log(LOG_FLAG_ERROR, "Failed to open dest file");
            fmErr = FMERR_WRITING_FILE;
            return UploadToken();
        }

        m_uploads.emplace(token, std::move(upload));

        return token;
    }
    else
        return UploadToken();
}

eUploadStatus UploadMgr::getUploadStatus(UploadToken const token)
{
    auto it = m_uploads.find(token);

    if (it != m_uploads.end())
    {
        Upload &upload = it->second;
        return upload.status;
    }
    else
        return UPLOAD_NOT_FOUND;
}

eUploadStatus UploadMgr::getUploadStatus(std::string const token)
{
    UploadToken uToken = stringToUploadToken(token);
    return this->getUploadStatus(uToken);
}

void UploadMgr::processChunk(UploadToken const token, uint64_t chunkID,
                             UploadChunk const &chunk)
{
    auto it = m_uploads.find(token);

    if (it == m_uploads.end())
    {
        sLog.log(LOG_FLAG_ERROR, "Unknown upload token");
        return;
    }

    Upload &upload = it->second;

    if (upload.status == UPLOAD_COMPLETE)
    {
        sLog.log(LOG_FLAG_ERROR, "Upload completed");
        return;
    }
    else
        upload.status = UPLOAD_IN_PROGRESS;

    if (chunkID >= upload.totalChunks)
    {
        sLog.log(LOG_FLAG_ERROR, "Invalid chunk ID");
        return;
    }

    const std::uint64_t offset     = chunkID * FILE_CHUNK_SIZE;

    const std::uint64_t remaining  = upload.file.size - offset;

    const std::size_t bytesToWrite = static_cast<std::size_t>(
        std::min<std::uint64_t>(remaining, FILE_CHUNK_SIZE));

    upload.fileStream.seekp(static_cast<std::streamoff>(offset));

    if (!upload.fileStream)
    {
        sLog.log(LOG_FLAG_ERROR, "Failed to seek upload file");
        return;
    }

    upload.fileStream.write(reinterpret_cast<const char *>(chunk.data()),
                            static_cast<std::streamsize>(bytesToWrite));

    if (!upload.fileStream)
    {
        sLog.log(LOG_FLAG_ERROR, "Failed to write upload chunk");
        return;
    }
    else
        upload.receivedChunks.insert(chunkID);

    if (upload.receivedChunks.size() == upload.totalChunks)
    {
        upload.fileStream.flush();

        if (!upload.fileStream)
        {
            sLog.log(LOG_FLAG_ERROR, "Failed to flush upload file");
            return;
        }

        upload.fileStream.close();

        if (!upload.fileStream)
        {
            sLog.log(LOG_FLAG_ERROR, "Failed to close upload file");
            return;
        }

        upload.status = UPLOAD_COMPLETE;
    }
}

void UploadMgr::processChunk(std::string const token, uint64_t chunkID,
                             UploadChunk const &chunk)
{
    UploadToken uToken = stringToUploadToken(token);
    return this->processChunk(uToken, chunkID, chunk);
}

UploadToken UploadMgr::generateUploadToken()
{
    UploadToken token{};

    if (RAND_bytes(token.data(), token.size()) != 1)
        throw std::runtime_error("Failed to generate upload token");

    return token;
}

std::string UploadMgr::uploadTokenToString(const UploadToken &token)
{
    std::ostringstream oss;

    oss << std::hex << std::setfill('0');

    for (const auto byte : token)
    {
        oss << std::setw(2) << static_cast<unsigned int>(byte);
    }

    return oss.str();
}

UploadToken UploadMgr::stringToUploadToken(const std::string &str)
{
    if (str.size() != UPLOAD_TOKEN_LENGTH * 2)
        throw std::invalid_argument("Invalid upload token length");

    UploadToken token{};

    for (std::size_t i = 0; i < token.size(); ++i)
    {
        const char high = str[i * 2];
        const char low  = str[i * 2 + 1];

        auto hexValue   = [](char c) -> std::uint8_t
        {
            if (c >= '0' && c <= '9')
                return static_cast<std::uint8_t>(c - '0');

            if (c >= 'a' && c <= 'f')
                return static_cast<std::uint8_t>(c - 'a' + 10);

            if (c >= 'A' && c <= 'F')
                return static_cast<std::uint8_t>(c - 'A' + 10);

            throw std::invalid_argument("Invalid character in upload token");
        };

        token[i] =
            static_cast<std::uint8_t>((hexValue(high) << 4) | hexValue(low));
    }

    return token;
}
