#ifndef GATHER_HASHES_H
#define GATHER_HASHES_H

#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>

using DedupTable = std::unordered_map<uint64_t, std::vector<std::string>>;

/*
* Calculates hash values of files and stores them
* in the deduplication table.
* 
* Path can be a file or a directory.
* If it is a file, its hash is calculated.
* If it is a directory, the hash of each file in the
* directory (non-recursively) will be calculated.
*/
void gather_hashes(const std::filesystem::path path, 
DedupTable &dedup_table);

#endif /* GATHER_HASHES_H */