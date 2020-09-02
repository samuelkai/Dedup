#ifndef FIND_DUPLICATES_H
#define FIND_DUPLICATES_H

#include "utilities.h"

#include <vector>

template <typename T>
std::vector<DuplicateVector> find_duplicates_map(const ArgMap &cl_args);

template <typename T>
std::vector<DuplicateVector> find_duplicates_vector(const ArgMap &cl_args);

std::vector<DuplicateVector> find_duplicates_vector_no_hash(const ArgMap &cl_args);

/**
 * Finds duplicate files from the given paths.
 * 
 * Path can be a file or a directory.
 * Directories can be searched recursively, according to the given parameter.
 * 
 * Returns a vector whose elements are vectors of duplicate files.
 */
inline std::vector<DuplicateVector> find_duplicates(const ArgMap &cl_args)
{
    return find_duplicates_vector_no_hash(cl_args);
}

#endif // FIND_DUPLICATES_H
