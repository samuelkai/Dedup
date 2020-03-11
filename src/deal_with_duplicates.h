#ifndef DEAL_WITH_DUPLICATES_H
#define DEAL_WITH_DUPLICATES_H

#include "utilities.h"

#include "cxxopts/cxxopts.hpp"

#include <filesystem>
#include <vector>

/**
 * Deal with the given duplicates according to the given parameters.
 * List, summarize or prompt for deletion.
 */
void deal_with_duplicates(const cxxopts::ParseResult &cl_args, 
    const std::vector<DuplicateVector> &duplicates);

#endif // DEAL_WITH_DUPLICATES_H
