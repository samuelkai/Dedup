#ifndef GATHER_HASHES_H
#define GATHER_HASHES_H
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>

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
std::unordered_map<std::string, std::vector<std::string>> &dedup_table);

#endif /* GATHER_HASHES_H */