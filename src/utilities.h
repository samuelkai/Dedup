#ifndef UTILITIES_H
#define UTILITIES_H

#include <filesystem>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// Possible actions to take on duplicates
enum class Action
{
    prompt_delete, no_prompt_delete, hardlink, list, summarize, symlink
};

// Possible types for command line arguments
using Arg = std::variant<
    bool, int, uintmax_t, Action, std::vector<std::filesystem::path>
>;

// Container for retrieving command line arguments
using ArgMap = std::unordered_map<std::string, Arg>;

/**
 * Stores a file's path and last modification time. When the file is asked to be
 * deleted, the time is used to check if the file has been modified after it has
 * been scanned.
 */
struct File {
    std::string path;
    std::filesystem::file_time_type m_time;
    File(std::string _path, std::filesystem::file_time_type _m_time);
};

/**
 * A vector that contains identical Files.
 */
using DuplicateVector = std::vector<File>;

/**
 * Exception that is thrown when file stream is not valid.
 */
class FileException : public std::system_error
{
    public:
        explicit FileException(std::error_code ec);
};

/**
 * Returns true if the contents of the files in the given paths are exactly
 * the same.
 */
bool compare_files(const std::string &path1,
                  const std::string &path2);

/**
 * Return the 64-bit XXHash digest of the beginning of the file in the given 
 * path. Parameter "bytes" specifies the number of bytes that are considered.
 * If bytes == 0, the whole file is hashed.
 */
uint64_t hash_file(const std::string &path, uintmax_t bytes);

/**
 * Formats the given bytes as a string with a binary prefix.
 */
std::string format_bytes(uintmax_t bytes);

#endif // UTILITIES_H
