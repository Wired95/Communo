#ifndef _FILEUTILS_H_
#define _FILEUTILS_H_

#include <cstdint>
#include <string>
#include <vector>

struct File
{
    std::string filename;
    uint64_t size;
    unsigned char md5[16];
};

std::vector<File> get_files_in_dir(const char *directory);

void print_files(const std::vector<File> &files);

std::vector<File> deserialize_files(const std::string &payload);

#endif // _FILEUTILS_H_
