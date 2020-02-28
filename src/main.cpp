#include "cxxopts/cxxopts.hpp"
#include "deal_with_duplicates.h"
#include "find_duplicates.h"
#include "parse.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;

namespace fs = std::filesystem;

int main(int argc, char *argv[])
{
    try
    {
        const auto result = parse(argc, argv);

        std::set<fs::path> paths_to_deduplicate;
        if (result.count("path"))
        {
            for (const auto &path : result["path"].as<vector<string>>())
            {
                try
                {
                    if (fs::exists(path))
                    {
                        if (!fs::is_symlink(path))
                        {
                            paths_to_deduplicate.insert(fs::canonical(path));
                        }
                    }
                    else {
                        cout << path << " does not exist\n\n";
                    }
                }
                catch(const std::exception& e)
                {
                    cerr << "Error opening file: " << e.what() << '\n';
                }
            }
        }
        else
        {
            cout << "Usage: " << argv[0] << " path1 [path2] [path3]..." 
                 << endl;
            return 0;
        }
        
        const int hash_size = result["hash"].as<int>();
        switch (hash_size)
        {
        case 1:
            deal_with_duplicates(result, find_duplicates<uint8_t>(result, 
                                                  paths_to_deduplicate));
            break;
        case 2:
            deal_with_duplicates(result, find_duplicates<uint16_t>(result, 
                                                  paths_to_deduplicate));
            break;
        case 4:
            deal_with_duplicates(result, find_duplicates<uint32_t>(result, 
                                                  paths_to_deduplicate));
            break;
        default:
            deal_with_duplicates(result, find_duplicates<uint64_t>(result, 
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
        std::cerr << e.what() << '\n';
        return 1;
    }
    catch(...)
    {
        std::cerr << "Unknown exception. Terminating program." << '\n';
        return 1;
    }

    return 0;
}
