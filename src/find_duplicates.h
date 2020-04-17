#ifndef FIND_DUPLICATES_H
#define FIND_DUPLICATES_H

#include "utilities.h"

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
std::vector<DuplicateVector> find_duplicates(const ArgMap &cl_args);

#endif // FIND_DUPLICATES_H
