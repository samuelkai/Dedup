#ifndef DEAL_WITH_DUPLICATES_H
#define DEAL_WITH_DUPLICATES_H

#include "cxxopts/cxxopts.hpp"

#include <filesystem>
#include <string>
#include <vector>

/**
 * Deal with the given duplicates according to the given parameters.
 * List, summarize or prompt for deletion.
 */
void deal_with_duplicates(const cxxopts::ParseResult &result, 
    const std::vector<std::vector<std::filesystem::path>> duplicates);

#endif // DEAL_WITH_DUPLICATES_H