#include "find_duplicates_base.h"

#include <iostream>
#include <filesystem>

using std::cerr;
using std::cout;

namespace fs = std::filesystem;

ScanManager::ScanManager(FileSizeTable &f)
    : count(0), size(0), file_size_table(f) {}

void ScanManager::insert(const fs::directory_entry &entry, size_t number_of_path)
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

size_t ScanManager::get_count() const {return count;}
uintmax_t ScanManager::get_size() const {return size;}

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

void print_progress(size_t curr_f_cnt, size_t tot_f_cnt, size_t step_size) {

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
