#ifndef FIND_DUPLICATES_H
#define FIND_DUPLICATES_H

#include "cxxopts/cxxopts.hpp"

#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

template <typename T>
/**
 * Stores file paths and hashes of file contents.
 * The key is the hash of the beginning N bytes of a file,
 *  where N is a program argument.
 *  The key type T is one of {uint8_t, uint16_t, uint32_t, uint64_t}.
 * The value contains the paths of all files that produce the same hash.
 *  Because files can differ after the first N bytes, the outer vector contains
 *  inner vectors that contain files whose whole content is the same.
 */
using DedupTable = std::unordered_map<T,std::vector
                                        <std::vector
                                        <std::filesystem::path>>>;


/**
 * Calculates hash values of files and stores them
 * in the deduplication table.
 * 
 * Path can be a file or a directory.
 * If it is a file, its hash is calculated.
 * If it is a directory, the hash of each regular file in the directory 
 * (recursively or not, depending on the argument) will be calculated.
 */
template <typename T>
std::vector<std::vector<std::filesystem::path>> find_duplicates(
    const cxxopts::ParseResult &result, 
    const std::set<std::filesystem::path> &paths_to_deduplicate);

#endif // FIND_DUPLICATES_H
