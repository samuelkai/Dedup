#include "find_duplicates_base.h"

#include <iostream>
#include <filesystem>

using std::cerr;
using std::cout;
using std::endl;

namespace fs = std::filesystem;

/**
 * Manages the scanning that is done before deduplication. Files are counted and
 * their paths and last modification times are collected.
 */
class ScanManager {
        // size_t can store the maximum size of a theoretically possible object 
        // of any type. Because we store Files in objects, we use size_t.
        size_t count;
        // uintmax_t is the maximum width unsigned integer type. We use it in 
        // order to not limit file size by type choice.
        uintmax_t size;
        FileSizeTable &file_size_table;
    public:
        ScanManager(FileSizeTable &f)
            : count(0), size(0), file_size_table(f) {};

        void insert(const fs::directory_entry &entry, size_t number_of_path)
        {
            try
            {
                fs::path path = entry.path();
                // Symlinks and empty files are skipped
                if (fs::is_regular_file(fs::symlink_status(path))
                    && !fs::is_empty(path))
                {
                    auto file_size = entry.file_size();
                    // If a file's hard link count is 1, it doesn't have extra
                    // hard links
                    if (fs::hard_link_count(path) > 1)
                    {
                        for (auto &file : file_size_table[file_size])
                        {
                            if (fs::equivalent(path, fs::path(file.path)))
                            {
                                // The file to be inserted is an extra hard link
                                // to an already inserted file, so skip it
                                return;
                            }
                        }
                    }
                    
                    file_size_table[file_size]
                        .push_back(File(path.string(), 
                                        entry.last_write_time(),
                                        number_of_path));
                    ++count;
                    size += file_size;
                }
            }
            catch(const fs::filesystem_error &e)
            {
                cerr << e.what() << '\n';
            }
            catch(const std::runtime_error &e)
            {
                cerr << e.what() << " [" << entry.path().string() << "]\n";
            }
            catch(const std::exception& e)
            {
                cerr << e.what() << '\n';
            }
        }

        size_t get_count() const {return count;}
        uintmax_t get_size() const {return size;}
};

/**
 * Traverses the given path and collects information about files.
 * Directories are traversed recursively if wanted.
 */
void scan_path(const fs::path &path, bool recurse, ScanManager &sm, 
               size_t number_of_path)
{
    if (fs::is_directory(path))
    {
        // Directories that cannot be accessed are skipped
        if (recurse)
        {
            for (const auto &p : fs::recursive_directory_iterator(path, 
                fs::directory_options::skip_permission_denied))
            {
                sm.insert(p, number_of_path);
            }
        }
        else
        {
            for (const auto &p : fs::directory_iterator(path, 
                fs::directory_options::skip_permission_denied))
            {
                sm.insert(p, number_of_path);
            }
        }
    }
    else
    {
        sm.insert(fs::directory_entry(path), number_of_path);
    }
}

size_t scan_all_paths(FileSizeTable &file_size_table, const ArgMap &cl_args)
{
    cout << "Counting number and size of files in given paths..." << endl;
    ScanManager sm = ScanManager(file_size_table);
    
    const bool recurse = std::get<bool>(cl_args.at("recurse"));
    size_t number_of_path = 0; // Used in deciding which file to keep when 
                               // deleting or linking without prompting
    for (const auto &path : 
         std::get<std::vector<fs::path>>(cl_args.at("paths")))
    {
        try
        {
            scan_path(path, recurse, sm, number_of_path);
        }
        catch(const std::exception &e)
        {
            cerr << e.what() << '\n';
        }
        ++number_of_path;
    }
    
    const size_t total_count = sm.get_count();
    const uintmax_t total_size = sm.get_size();
    cout << "Counted " << total_count << " files occupying "
            << format_bytes(total_size) << "." << endl;

    return total_count;
}

size_t skip_files_with_unique_size(FileSizeTable &file_size_table)
{
    size_t no_fls_with_uniq_sz = 0;

    auto same_size_iter = file_size_table.begin();
    auto end_iter = file_size_table.end();

    for(; same_size_iter != end_iter; )
    {
        if (same_size_iter->second.size() == 1) // Unique size
        {
            same_size_iter = file_size_table.erase(same_size_iter);
            ++no_fls_with_uniq_sz;
        }
        else
        {
            ++same_size_iter;
        }
    }

    cout << "Discarded " << no_fls_with_uniq_sz << " files with unique "
    "size from deduplication.\n";

    return no_fls_with_uniq_sz;
}

void print_progress(size_t curr_f_cnt, size_t tot_f_cnt, size_t step_size)
{
    if (step_size > 0 
        && (curr_f_cnt % step_size == 0 || curr_f_cnt == tot_f_cnt))
    {
        const float progress = static_cast<float>(curr_f_cnt) 
            / static_cast<float>(tot_f_cnt);
        cout << "\r" << "File " << curr_f_cnt << "/" << tot_f_cnt 
            << " (" << int(progress * 100.0) << " %)";
        cout.flush();
    }
}
