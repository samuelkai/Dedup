#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

struct MyListener : Catch::TestEventListenerBase {

    using TestEventListenerBase::TestEventListenerBase; // inherit constructor
    
    /**
     * Remove temporary directory before test case
     * (possible remnant from failed test).
     */
    void testCaseStarting( Catch::TestCaseInfo 
        const& testCaseInfo __attribute__((unused))) override
    {
        const fs::path test_dir_path = 
            fs::temp_directory_path() / "dedup_test98437524";
        
        fs::remove_all(test_dir_path);
    }

    /**
     * Remove temporary directory after test case.
     */
    void testCaseEnded(Catch::TestCaseStats 
        const& testCaseStats __attribute__((unused))) override
    {
        const fs::path test_dir_path = 
            fs::temp_directory_path() / "dedup_test98437524";

        fs::remove_all(test_dir_path);
    }
};
CATCH_REGISTER_LISTENER(MyListener)