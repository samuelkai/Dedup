/*------------------------------------------------------------------------------
This program finds duplicate files, i.e., files with identical content.
------------------------------------------------------------------------------*/

#include "deal_with_duplicates.h"
#include "find_duplicates.h"
#include "parse.h"
#include "utilities.h"

#include <iostream>
#include <variant>

using std::cerr;

int main(int argc, char *argv[])
{
    try
    {
        const ArgMap cl_args = parse(argc, argv);
        
        const int hash_size = std::get<int>(cl_args.at("hash"));
        const Action action = std::get<Action>(cl_args.at("action"));
        switch (hash_size)
        {
        case 1:
            deal_with_duplicates(
                action, std::move(find_duplicates<uint8_t>(cl_args)));
            break;
        case 2:
            deal_with_duplicates(
                action, std::move(find_duplicates<uint16_t>(cl_args)));
            break;
        case 4:
            deal_with_duplicates(
                action, std::move(find_duplicates<uint32_t>(cl_args)));
            break;
        default:
            deal_with_duplicates(
                action, std::move(find_duplicates<uint64_t>(cl_args)));
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
