#include "cxxopts/cxxopts.hpp"
#include "parse.h"
#include "utilities.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;

namespace fs = std::filesystem;

EndException::EndException(int bad) : _bad(bad) {}

int EndException::is_bad() const noexcept
{
    return _bad;
}

// Turn the string paths given in command line to fs::paths and check their 
// validity.
vector<fs::path> extract_paths(vector<string> path_arg)
{
    vector<fs::path> paths_to_deduplicate;
    for (const auto &path : path_arg)
    {
        try
        {
            if (!fs::is_symlink(path))
            {
                fs::path canon_path = fs::canonical(path);
                if (fs::exists(canon_path))
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
                else {
                    cerr << path << " does not exist\n\n";
                }
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

/**
 * Parse command line arguments.
 */
ArgMap parse(int argc, char* argv[])
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

        cxxopts::Options options(argv[0], " - find and delete duplicate files");
        options
            .positional_help("path1 [path2] [path3]...\nBy default, the user "
            "is prompted to select which duplicates to delete.")
            .show_positional_help();

        // Positional argument that won't be printed in help
        options.add_options("")
            ("path", "Path", cxxopts::value<vector<string>>(), "PATH");

        // For example: "l, list" means that the argument is given with "-l" or
        // "--list". Next is the description to be displayed in help. Next is 
        // the value type and default value. The possible last string is 
        // displayed in help after the argument, as in "--bytes N".

        options.add_options("Action")
            ("d,delete", "Delete duplicate files without prompting, keeping "
                "only one file in each set of duplicates. Files in paths given "
                "earlier in the argument list have higher precedence to be "
                "kept. If there are duplicates in the same folder, it is "
                "undefined which file is kept. Must be specified twice in "
                "order to avoid accidental use.", 
                cxxopts::value<bool>()->default_value("false"))
            ("k,hardlink", "Hard link all duplicate files without prompting.",
                cxxopts::value<bool>()->default_value("false"))
            ("l,list", "List found duplicates, don't prompt for deduplication", 
                cxxopts::value<bool>()->default_value("false"))
            ("s,summarize", "Print only a summary of found duplicates, "
                "don't prompt for deduplication",
                cxxopts::value<bool>()->default_value("false"))
        ;

        options.add_options("Other")
            ("a,hash", "Hash digest size in bytes, valid values are " + 
                hash_sizes_str, cxxopts::value<int>()->default_value(
                std::to_string(DEFAULT_HASH_SIZE)), "N")
            ("b,bytes", "Number of bytes from the beginning of each file that "
                "are used in hash calculation. "
                "0 means that the whole file is hashed.",
                cxxopts::value<uintmax_t>()->default_value("4096"), "N")
            ("h,help", "Print this help")
            ("r,recurse", "Search the paths for duplicates recursively",
                cxxopts::value<bool>()->default_value("false"))
        ;

        options.parse_positional({"path"});

        const auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            cout << options.help({"Action", "Other"}) << endl;
            throw EndException(0);
        }

        ArgMap cl_args;

        // Path(s) must be specified.
        if (result.count("path"))
        {
            const auto paths = 
                extract_paths(result["path"].as<vector<string>>());
            if (paths.empty())
            {
                cerr << "No paths to deduplicate.\n";
                throw EndException(1);
            }   
            else
            {
                cl_args["paths"] = std::move(paths);
            }
        }
        else
        {
            cerr << "Usage: " << argv[0] << " path1 [path2] [path3]..." 
                 << endl;
            throw EndException(1);
        }

        // Parse actions. If no action is specified, the default of prompting 
        // which files to delete is used. The no-prompt delete action must be
        // specified twice. Only one action can be specified.

        cl_args["action"] = Action::prompt_delete;
        bool action_specified = false; // Enforce one action only
        switch (result.count("delete"))
        {
        case 0:
            break;
        case 1:
            cerr << "Argument delete must be specified twice in order to avoid "
            "accidental use.\n";
            throw EndException(1);
        default:
            cl_args["action"] = Action::no_prompt_delete;
            action_specified = true;
            break;
        }

        std::unordered_map<string, Action> actions = {
            {"hardlink", Action::hardlink}, 
            {"list", Action::list}, 
            {"summarize", Action::summarize}
        };

        for (const auto &a : actions)
        {
            if (result.count(a.first))
            {
                if (!action_specified)
                {
                    action_specified = true;
                    cl_args["action"] = a.second;
                }
                else
                {
                    cerr << "Only one action (delete, hardlink, list, "
                            "summarize) can be specified\n";
                    throw EndException(1);
                }
                
            }
        }

        // If hash is specified, it must be one of the predetermined values.
        // If it is not specified, the default is used.
        if (result.count("hash"))
        {
            const int size = result["hash"].as<int>();
            if (std::find(hash_sizes.begin(), hash_sizes.end(), size)
                == hash_sizes.end())
            {
                cerr << "Invalid argument hash: must be one of "
                     << hash_sizes_str << '\n';
                throw EndException(1);
            }
        }
        cl_args["hash"] = result["hash"].as<int>();
        
        cl_args["bytes"] = result["bytes"].as<uintmax_t>();
        cl_args["recurse"] = result.count("recurse") > 0 ? true : false;

        return cl_args;
    }
    catch (const cxxopts::OptionException& e)
    {
        cerr << "error parsing options: " << e.what() << '\n';
        throw EndException(1);
    }

}
