#include "cxxopts/cxxopts.hpp"
#include "parse.h"

#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::string;
using std::vector;

EndException::EndException(int bad) : _bad(bad) {}

int EndException::is_bad() const noexcept
{
    return _bad;
}

/**
 * Parse command line arguments
 */
cxxopts::ParseResult parse(int argc, char* argv[])
{
    try
    {
        // Possible sizes for the hash digest in bytes
        const vector<int> hash_sizes = {1,2,4,8};
        constexpr int DEFAULT_HASH_SIZE = 8;

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

        // Positional argument that won't be printed in help
        options.add_options("")
            ("path", "Path", cxxopts::value<vector<string>>(), "PATH");

        // For example: "l, list" means that the argument is given with "-l" or
        // "--list". Next is the description to be displayed in help. Next is 
        // the value type and default value. The possible last string is 
        // displayed in help after the argument, as in "--bytes N".
        options.add_options("Optional")
            ("a,hash", "Hash digest size in bytes, valid values are " + 
                hash_sizes_str, cxxopts::value<int>()->default_value(
                std::to_string(DEFAULT_HASH_SIZE)), "N")
            ("b,bytes", "Number of bytes from the beginning of each file that "
                "are used in hash calculation. "
                "0 means that the whole file is hashed.",
                cxxopts::value<uint64_t>()->default_value("4096"), "N")
            ("h,help", "Print this help")
            ("l,list", "List found duplicates, don't prompt for deduplication", 
                cxxopts::value<bool>()->default_value("false"))
            ("r,recurse", "Search the paths for duplicates recursively",
                cxxopts::value<bool>()->default_value("false"))
            ("s,summarize", "Print only a summary of found duplicates, "
                "don't prompt for deduplication",
                cxxopts::value<bool>()->default_value("false"))
        ;

        options.parse_positional({"path"});

        const auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            cout << options.help({"Optional"}) << endl;
            throw EndException(0);
        }

        if (result.count("hash"))
        {
            const int size = result["hash"].as<int>();
            if (std::find(hash_sizes.begin(), hash_sizes.end(), size)
                == hash_sizes.end())
            {
                cout << "Invalid argument hash: must be one of "
                     << hash_sizes_str << endl;
                throw EndException(1);
            }
        }

        return result;
    }
    catch (const cxxopts::OptionException& e)
    {
        std::cerr << "error parsing options: " << e.what() << '\n';
        throw EndException(1);
    }

}
