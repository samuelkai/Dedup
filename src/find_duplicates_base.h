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

// /**
//  * Prints the progress on finding duplicates.
//  */
// template <typename T>
// void print_progress(DedupManager<T> &op);

class ScanManager {
    // size_t can store the maximum size of a theoretically possible object 
    // of any type. Because we store Files in objects, we use size_t.
    size_t count;
    // uintmax_t is the maximum width unsigned integer type. We use it in 
    // order to not limit file size by type choice.
    uintmax_t size;
    FileSizeTable &file_size_table;

    public:
        ScanManager(FileSizeTable &f);

        void insert(const std::filesystem::directory_entry &entry,
            size_t number_of_path);

        size_t get_count() const;
        uintmax_t get_size() const;
};

/**
 * Traverses the given path and collects information about files.
 * Directories are traversed recursively if wanted.
 */
void scan_path(const std::filesystem::path &path, bool recurse, ScanManager &sm, 
               size_t number_of_path);

/**
 * Prints the progress on finding duplicates.
 */
void print_progress(size_t curr_f_cnt, size_t tot_f_cnt, size_t step_size);

#endif // FIND_DUPLICATES_BASE_H