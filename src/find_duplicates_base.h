#ifndef FIND_DUPLICATES_BASE_H
#define FIND_DUPLICATES_BASE_H

#include "find_duplicates.h"

#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <vector>

/**
 * Stores Files grouped by their size. Used during the file scanning phase.
 */
using FileSizeTable = std::unordered_map<uintmax_t, std::vector<File>>;

/**
 * Scans all the paths that were given as command line arguments.
 */
size_t scan_all_paths(FileSizeTable &file_size_table, const ArgMap &cl_args);

/**
 * Files with unique size can't have duplicates. This function removes them
 * from the deduplication.
 */
size_t skip_files_with_unique_size(FileSizeTable &file_size_table);

/**
 * Prints the progress on finding duplicates.
 */
void print_progress(size_t curr_f_cnt, size_t tot_f_cnt, size_t step_size);

#endif // FIND_DUPLICATES_BASE_H