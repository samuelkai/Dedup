#ifndef FIND_DUPLICATES_H
#define FIND_DUPLICATES_H

#include "utilities.h"

#include "cxxopts/cxxopts.hpp"

#include <filesystem>
#include <set>
#include <vector>

/**
 * Finds duplicate files from the given paths.
 * 
 * Path can be a file or a directory.
 * Directories can be searched recursively, according to the given parameter.
 * 
 * Returns a vector whose elements are vectors of duplicate files.
 */
template <typename T>
std::vector<DuplicateVector> find_duplicates(
    const cxxopts::ParseResult &cl_args, 
    const std::set<std::filesystem::path> &paths_to_deduplicate);

#endif // FIND_DUPLICATES_H
