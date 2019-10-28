#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
#include "gather_hashes.h"
#include "catch2/catch.hpp"
using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::unordered_map;
using std::vector;
namespace ch = std::chrono;
namespace fs = std::filesystem;

bool is_positive_integer(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

void prompt_duplicate_deletions(unordered_map<uint64_t, vector<string>> duplicates)
{
    cout << "\n" << "Found " << duplicates.size() << " files that have duplicates:\n\n";
    for (const auto &pair : duplicates)
    {
        cout << "Key: " << pair.first << "\nValues:\n";
        for (int i = 0; i < pair.second.size(); i++)
        {
            cout << "[" << i << "] " << pair.second[i] << "\n";
        }
        cout << endl;

        string input;
        do
        {
            cout << "Select the file to keep" << endl;
            getline(cin, input);
        } while (!is_positive_integer(input));
        int kept = std::stoi(input);

        for (int i = 0; i < pair.second.size(); i++)
        {
            if (i != kept)
            {
                try
                {
                    if(!fs::remove(pair.second[i]))
                    {
                        std::cerr << "File \"" << pair.second[i] << "\" not found, could not delete it" << "\n";
                    }
                }
                catch(const fs::filesystem_error& e)
                {
                    std::cerr << e.what() << '\n';
                }                 
            }
        }
    }
}

int main(int argc, char *argv[])
{
    // TODO: Virheenkäsittely syötteisiin jos ei anna polkua
    string input;
    if (argc < 2)
    {
        cout << "Input a path (file or directory) to "
        "be deduplicated" << endl;
        getline(cin, input);
    }
    else
    {
        input = argv[1];
    }
    fs::path path = fs::canonical(input);

    unordered_map<uint64_t, vector<string>> dedup_table;
    auto start_time = ch::steady_clock::now();

    gather_hashes(path, dedup_table);

    ch::duration<double, std::milli> elapsed_time = ch::steady_clock::now() - start_time;

    while (true)
    {
        cout << "Input another path to be included in "
        "the deduplication or finish with N" << endl;

        string input;
        getline(cin, input);
        if (input == "N")
        {
            break;
        }

        auto additional_time = ch::steady_clock::now();
        gather_hashes(fs::canonical(input), dedup_table);
        elapsed_time += ch::steady_clock::now() - additional_time;
    }

    unordered_map<uint64_t, vector<string>> duplicates;

    for (const auto &pair : dedup_table)
    {
        if (pair.second.size() > 1)
        {
            duplicates.insert(pair);
        }
    }

    if (duplicates.size() == 0)
    {
        cout << "Didn't find any duplicates." << endl;
    } else {
        prompt_duplicate_deletions(duplicates);
    }

    std::cout << "Gathering of hashes took " << elapsed_time.count() << " milliseconds." << std::endl;
}
