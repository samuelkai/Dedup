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

int main(int argc, char *argv[])
{
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

    for (const auto &pair : dedup_table)
    {
        cout << "Key: " << pair.first << "\nValues:\n";
        for (const auto &path : pair.second)
        {
            cout << path << endl;
        }
        cout << endl;
    }

    std::cout << "Gathering of hashes took " << elapsed_time.count() << " milliseconds." << std::endl;
}
