#ifndef DEAL_WITH_DUPLICATES_H
#define DEAL_WITH_DUPLICATES_H

#include "cxxopts/cxxopts.hpp"

#include <string>
#include <vector>

void deal_with_duplicates(const cxxopts::ParseResult &result, const std::vector<std::vector<std::string>> duplicates);

#endif // DEAL_WITH_DUPLICATES_H