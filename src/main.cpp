#include "deal_with_duplicates.h"
#include "find_duplicates.h"
#include "cxxopts/cxxopts.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::string;
using std::vector;

namespace fs = std::filesystem;

struct EndException : public std::exception
{
	const char* what() const noexcept override
    {
        return "End program";
    }
};

cxxopts::ParseResult parse(int argc, char* argv[])
{
    try
    {

        const vector<int> hash_sizes = {1,2,4,8};
        const int DEFAULT_HASH_SIZE = 8;
        string hash_sizes_str;
        for (size_t i = 0; i < hash_sizes.size(); ++i) {
            hash_sizes_str += std::to_string(hash_sizes[i]);
            if (i+1 != hash_sizes.size())
            {
                hash_sizes_str += ", ";
            }
        }

        cxxopts::Options options(argv[0], " - find duplicate files");
        options
            .positional_help("path1 [path2] [path3]...")
            .show_positional_help();

        options.add_options()
            ("file", "File", cxxopts::value<vector<string>>(), "FILE");

        options.add_options("Optional")
            ("h,help", "Print help")
            ("l,list", "List found duplicates, don't prompt for deduplication", 
                cxxopts::value<bool>()->default_value("false"))
            ("s,summarize", "Print only a summary of found duplicates, "
                "don't prompt for deduplication",
                cxxopts::value<bool>()->default_value("false"))
            ("r,recursive", "Search the paths for duplicates recursively",
                cxxopts::value<bool>()->default_value("false"))
            ("b,bytes", "Number of bytes from the beginning of each file that "
                "are used in hash calculation. "
                "0 means that the whole file is hashed.",
                cxxopts::value<uint64_t>()->default_value("4096"), "N")
            ("a,hash", "Hash digest size in bytes, valid values are " + 
                hash_sizes_str, cxxopts::value<int>()->default_value(
                std::to_string(DEFAULT_HASH_SIZE)), "N")
        ;

        options.parse_positional({"file"});

        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            cout << options.help({"Optional"}) << endl;
            throw EndException();
        }

        if (result.count("hash"))
        {
            int size = result["hash"].as<int>();
            if (std::find(hash_sizes.begin(), hash_sizes.end(), size)
                == hash_sizes.end())
            {
                cout << "Invalid argument hash: must be one of "
                     << hash_sizes_str << endl;
                throw EndException();
            }
        }

        return result;
    }
    catch (const cxxopts::OptionException& e)
    {
        std::cerr << "error parsing options: " << e.what() << "\n";
        exit(1);
    }

}

int main(int argc, char *argv[])
{
    try
    {
        auto result = parse(argc, argv);

        std::set<fs::path> paths_to_deduplicate;
        if (result.count("file"))
        {
            for (const auto &path : result["file"].as<vector<string>>())
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
        }
        else
        {
            cout << "Usage: " << argv[0] << " path1 [path2] [path3]..." 
                 << endl;
            return 0;
        }
        
        int hash_size = result["hash"].as<int>();
        vector<vector<fs::path>> duplicates; 
        switch (hash_size)
        {
        case 1:
            duplicates = find_duplicates<uint8_t>(result, paths_to_deduplicate);
            deal_with_duplicates(result, duplicates);
            break;
        case 2:
            duplicates = find_duplicates<uint16_t>(result, paths_to_deduplicate);
            deal_with_duplicates(result, duplicates);
            break;
        case 4:
            duplicates = find_duplicates<uint32_t>(result, paths_to_deduplicate);
            deal_with_duplicates(result, duplicates);
            break;
        default:
            duplicates = find_duplicates<uint64_t>(result, paths_to_deduplicate);
            deal_with_duplicates(result, duplicates);
            break;
        }
    }
    catch(const EndException& e)
    {
        return(0);
    }
    
    return 0;
}
