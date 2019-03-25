#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
#include "gather_hashes.h"
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
    if (argc < 2)
    {
        cout << "Usage: dedup PATH\n";
        return 1;
    }

    const uint64_t testi = 123123ull;
    const uint64_t * p = &testi;

    cout << sizeof(testi) << endl;
    cout << &p[0] << " " << &p[1] << endl;

    std::clock_t start = std::clock();

    fs::path path(argv[1]);
    unordered_map<uint64_t, vector<string>> dedup_table;

    gather_hashes(path, dedup_table);
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

        gather_hashes(fs::path(input), dedup_table);
    }

    cout << endl
         << "Contents of the deduplication table:\n"
         << endl;

    for (const auto &pair : dedup_table)
    {
        cout << "Key: " << pair.first << "\nValues:\n";
        for (const auto &path : pair.second)
        {
            cout << path << endl;
        }
        cout << endl;
    }

    double time = (std::clock() - start) / (double)CLOCKS_PER_SEC;
    std::cout << "Execution took " << time << " seconds." << std::endl;

}