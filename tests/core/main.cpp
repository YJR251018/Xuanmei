// Minimal test runner for the core suite.
//
// dsh_test.hpp owns the registry and the assertions; this file only provides
// main(), mirroring what GoogleTest's gtest_main would supply after migration.

#include <cstdio>
#include <exception>

#include "dsh_test.hpp"

int main(int argc, char** argv)
{
    try {
        return ::dsh_test::runAll(argc, argv);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "unhandled exception in test run: %s\n", error.what());
        return 2;
    } catch (...) {
        std::fprintf(stderr, "unhandled non-standard exception in test run\n");
        return 2;
    }
}
