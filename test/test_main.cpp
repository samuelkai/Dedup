#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

struct MyListener : Catch::TestEventListenerBase {

    using TestEventListenerBase::TestEventListenerBase; // inherit constructor
    
    void testRunEnded( Catch::TestRunStats const& testRunStats __attribute__((unused))) override {
        fs::path test_dir_path = fs::temp_directory_path() / "dedup_test98437524";

        fs::remove_all(test_dir_path);
    }
};
CATCH_REGISTER_LISTENER( MyListener )