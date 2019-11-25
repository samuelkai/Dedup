#ifndef FIND_DUPLICATES_H
#define FIND_DUPLICATES_H

#include "cxxopts/cxxopts.hpp"

#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

template <typename T>
using DedupTable = std::unordered_map<T,std::vector<std::vector<std::string>>>;

/*
* Calculates hash values of files and stores them
* in the deduplication table.
* 
* Path can be a file or a directory.
* If it is a file, its hash is calculated.
* If it is a directory, the hash of each file in the
* directory (non-recursively) will be calculated.
*/
template <typename T>
void gather_hashes(const std::filesystem::path &path,
                   DedupTable<T> &dedup_table, uint64_t bytes,
                   bool recursive);

template <typename T>
void find_duplicates(const cxxopts::ParseResult &result, const std::set<std::filesystem::path> &paths_to_deduplicate);

#endif // FIND_DUPLICATES_H