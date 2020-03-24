/*------------------------------------------------------------------------------
This program finds duplicate files, i.e. files with identical content.
------------------------------------------------------------------------------*/

#include "cxxopts/cxxopts.hpp"
#include "deal_with_duplicates.h"
#include "find_duplicates.h"
#include "parse.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using std::cerr;
using std::endl;
using std::string;
using std::vector;

namespace fs = std::filesystem;

vector<fs::path> extract_paths(vector<string> path_arg)
{
    vector<fs::path> paths_to_deduplicate;
    for (const auto &path : path_arg)
    {
        try
        {
            fs::path canon_path = fs::canonical(path);
            if (fs::exists(canon_path))
            {
                if (!fs::is_symlink(canon_path))
                {
                    if(std::find(paths_to_deduplicate.begin(), 
                                 paths_to_deduplicate.end(), canon_path) 
                                 != paths_to_deduplicate.end())
                    {
                        cerr << path 
                             << " is already included in the deduplication\n";
                    } else {
                        paths_to_deduplicate.push_back(
                            fs::canonical(canon_path));
                    }
                }
            }
            else {
                cerr << path << " does not exist\n\n";
            }
        }
        catch(const fs::filesystem_error &e)
        {
            cerr << e.what() << '\n';
        }
        catch(const std::exception &e)
        {
            cerr << e.what() << '\n';
        }
    }
    return paths_to_deduplicate;
}

int main(int argc, char *argv[])
{
    try
    {
        const auto cl_args = parse(argc, argv);

        vector<fs::path> paths_to_deduplicate;

        if (cl_args.count("path"))
        {
            paths_to_deduplicate = 
                extract_paths(cl_args["path"].as<vector<string>>());
        }
        else
        {
            cerr << "Usage: " << argv[0] << " path1 [path2] [path3]..." 
                 << endl;
            return 1;
        }

        if (paths_to_deduplicate.empty())
        {
            cerr << "No paths to deduplicate.\n";
            return 1;
        }
        
        const int hash_size = cl_args["hash"].as<int>();
        switch (hash_size)
        {
        case 1:
            deal_with_duplicates(cl_args, find_duplicates<uint8_t>(cl_args, 
                                                  paths_to_deduplicate));
            break;
        case 2:
            deal_with_duplicates(cl_args, find_duplicates<uint16_t>(cl_args, 
                                                  paths_to_deduplicate));
            break;
        case 4:
            deal_with_duplicates(cl_args, find_duplicates<uint32_t>(cl_args, 
                                                  paths_to_deduplicate));
            break;
        default:
            deal_with_duplicates(cl_args, find_duplicates<uint64_t>(cl_args, 
                                                  paths_to_deduplicate));
            break;
        }
    }
    catch(const EndException &e)
    {
        return(e.is_bad());
    }
    catch(const std::exception &e)
    {
        cerr << e.what() << '\n';
        return 1;
    }
    catch(...)
    {
        cerr << "Unknown exception. Terminating program." << '\n';
        return 1;
    }

    return 0;
}
