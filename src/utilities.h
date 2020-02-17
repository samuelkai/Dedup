#ifndef UTILITIES_H
#define UTILITIES_H

#include <filesystem>
#include <string>

/**
 * Exception that compare_files throws when it can't open a file.
 */
class FileException : public std::runtime_error
{
    public:
        FileException(std::string path);
};

/**
 * Returns true if the contents of the files in the given paths are exactly
 * the same.
 */
bool compare_files(const std::filesystem::path &path1,
                  const std::filesystem::path &path2);

/**
 * Return the 64-bit XXHash digest of the beginning of the file in the given 
 * path. Parameter "bytes" specifies the number of bytes that are considered.
 * If bytes == 0, the whole file is hashed.
 */
uint64_t hash_file(const std::filesystem::path &path, uint64_t bytes);

std::string format_bytes(uintmax_t bytes);

#endif // UTILITIES_H
