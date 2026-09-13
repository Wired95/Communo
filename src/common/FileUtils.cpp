#include <openssl/evp.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "FileUtils.h"
#include "SharedDefinitions.h"

std::vector<File> get_files_in_dir(const char *directory)
{
    std::vector<File> files;

    for (const auto &entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file())
            continue;

        File file{};
        file.filename = entry.path().filename().string();
        file.size     = std::filesystem::file_size(entry.path());

        std::ifstream input(entry.path(), std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open file: " +
                                     entry.path().string());

        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        if (!ctx)
            throw std::runtime_error("EVP_MD_CTX_new failed");

        if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1)
        {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("EVP_DigestInit_ex failed");
        }

        char buffer[64 * 1024];

        while (input)
        {
            input.read(buffer, sizeof(buffer));
            const std::streamsize bytes_read = input.gcount();

            if (bytes_read > 0 &&
                EVP_DigestUpdate(ctx, buffer, bytes_read) != 1)
            {
                EVP_MD_CTX_free(ctx);
                throw std::runtime_error("EVP_DigestUpdate failed");
            }
        }

        unsigned int digest_length = 0;

        if (EVP_DigestFinal_ex(ctx, file.md5, &digest_length) != 1)
        {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("EVP_DigestFinal_ex failed");
        }

        EVP_MD_CTX_free(ctx);

        if (digest_length != 16)
            throw std::runtime_error("Unexpected MD5 digest length");

        files.push_back(file);
    }

    return files;
}

void print_files(const std::vector<File> &files)
{
    constexpr std::size_t md5_length = 32;

    std::size_t filename_width       = std::string("file").length();
    std::size_t size_width           = std::string("size").length();

    for (const auto &file : files)
    {
        filename_width = std::max(filename_width, file.filename.length());
        size_width = std::max(size_width, std::to_string(file.size).length());
    }

    std::cout << std::setfill(' ') << std::dec << std::left
              << std::setw(static_cast<int>(filename_width)) << "file"
              << " | " << std::right << std::setw(static_cast<int>(size_width))
              << "size"
              << " | md5sum\n";

    std::cout << std::setfill('-')
              << std::setw(static_cast<int>(filename_width)) << ""
              << "-+-" << std::setw(static_cast<int>(size_width)) << ""
              << "-+-" << std::string(md5_length, '-') << '\n';

    for (const auto &file : files)
    {
        std::cout << std::setfill(' ') << std::dec << std::left
                  << std::setw(static_cast<int>(filename_width))
                  << file.filename << " | " << std::right
                  << std::setw(static_cast<int>(size_width)) << file.size
                  << " | ";

        std::cout << std::uppercase << std::hex << std::setfill('0');

        for (unsigned char byte : file.md5)
        {
            std::cout << std::setw(2) << static_cast<unsigned int>(byte);
        }

        std::cout << std::dec << std::setfill(' ') << '\n';
    }
}

std::vector<File> deserialize_files(const std::string &payload)
{
    std::vector<File> files;

    constexpr std::size_t md5_size = 16;

    std::size_t offset             = 0;

    auto read                      = [&](void *destination, std::size_t size)
    {
        if (offset + size > payload.size())
            throw std::runtime_error("Invalid file payload");

        std::memcpy(destination, payload.data() + offset, size);
        offset += size;
    };

    uint8_t error;
    uint8_t fileCount;

    read(&error, sizeof(error));
    read(&fileCount, sizeof(fileCount));

    if (error != FMERR_OK)
        throw std::runtime_error("Remote directory listing returned an error");

    files.reserve(fileCount);

    for (uint8_t i = 0; i < fileCount; ++i)
    {
        File file{};

        // [filename length]
        uint8_t filenameLength;
        read(&filenameLength, sizeof(filenameLength));

        // [filename]
        file.filename.assign(payload.data() + offset, filenameLength);
        offset += filenameLength;

        if (offset > payload.size())
            throw std::runtime_error("Invalid filename in file payload");

        // [size]
        read(&file.size, sizeof(file.size));

        // [md5sum]
        read(file.md5, md5_size);

        files.push_back(file);
    }

    return files;
}
