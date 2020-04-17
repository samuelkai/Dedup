#ifndef DEAL_WITH_DUPLICATES_H
#define DEAL_WITH_DUPLICATES_H

#include "utilities.h"

#include <vector>

/**
 * Deal with the given duplicates using the given action.
 */
void deal_with_duplicates(Action action, 
    const std::vector<DuplicateVector> &duplicates);

#endif // DEAL_WITH_DUPLICATES_H
