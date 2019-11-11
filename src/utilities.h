#ifndef UTILITIES_H
#define UTILITIES_H

#include <filesystem>

bool compare_files(const std::filesystem::path &p1,
                  const std::filesystem::path &p2);

uint64_t hash_file(const std::filesystem::path &p);

uint64_t hash_file(const std::filesystem::path &p, uint64_t bytes);

#endif // UTILITIES_H