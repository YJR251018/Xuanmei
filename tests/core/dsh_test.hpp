// Minimal assertion framework for the core test suite.
//
// Deliberately has no external dependency: ADR-002 requires this stage to avoid
// unnecessary dependencies, and vcpkg is not installed on the development
// machine yet. The TEST(...) / CHECK / REQUIRE structure is intentionally the
// same as GoogleTest and Catch2 so the cases below can be migrated without
// being rewritten once vcpkg lands.
//
// Not thread-safe; the runner is single-threaded.

#pragma once

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace dsh_test {

using TestFunction = void (*)();

struct TestCase {
    const char* name;
    TestFunction function;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> cases;
    return cases;
}

inline int& currentFailureCount()
{
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* name, TestFunction function)
    {
        registry().push_back(TestCase{name, function});
    }
};

inline void reportFailure(const char* file, int line, const std::string& message)
{
    ++currentFailureCount();
    std::fprintf(stderr, "  FAIL %s:%d\n       %s\n", file, line, message.c_str());
}

// Absolute-or-relative comparison: the bound is `tolerance`, scaled by the
// magnitude once |v| exceeds 1, so that values large enough for a pure absolute
// bound to fall below double resolution stay comparable. The scale factor is at
// most |v|, so the effective ABSOLUTE bound is always `tolerance * |v|`; pass a
// tolerance at or below the required absolute precision to stay exact.
inline bool nearlyEqual(double actual, double expected, double tolerance)
{
    const double scale = std::fmax(1.0, std::fmax(std::fabs(actual), std::fabs(expected)));
    return std::fabs(actual - expected) <= tolerance * scale;
}

inline int runAll(int argc, char** argv)
{
    const std::string filter = (argc > 1) ? argv[1] : std::string{};
    int executed = 0;
    int failedCases = 0;

    for (const TestCase& test : registry()) {
        if (!filter.empty() && std::string(test.name).find(filter) == std::string::npos) {
            continue;
        }
        const int before = currentFailureCount();
        std::printf("[ RUN  ] %s\n", test.name);
        test.function();
        ++executed;
        if (currentFailureCount() != before) {
            ++failedCases;
            std::printf("[ FAIL ] %s\n", test.name);
        } else {
            std::printf("[  OK  ] %s\n", test.name);
        }
    }

    std::printf("----------------------------------------------------------------\n");
    std::printf("ran %d test(s), %d passed, %d failed, %d failed assertion(s)\n",
                executed,
                executed - failedCases,
                failedCases,
                currentFailureCount());
    return failedCases == 0 ? 0 : 1;
}

}  // namespace dsh_test

#define DSH_TEST_CONCAT_INNER(a, b) a##b
#define DSH_TEST_CONCAT(a, b) DSH_TEST_CONCAT_INNER(a, b)

#define TEST(suite, name)                                                      \
    static void DSH_TEST_CONCAT(suite, DSH_TEST_CONCAT(_, name))();            \
    static const ::dsh_test::Registrar DSH_TEST_CONCAT(                        \
        registrar_, DSH_TEST_CONCAT(suite, DSH_TEST_CONCAT(_, name))){         \
        #suite "." #name,                                                      \
        &DSH_TEST_CONCAT(suite, DSH_TEST_CONCAT(_, name))};                    \
    static void DSH_TEST_CONCAT(suite, DSH_TEST_CONCAT(_, name))()

// Always evaluated, even in a release build.
#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            ::dsh_test::reportFailure(__FILE__, __LINE__,                      \
                                      std::string("expected: ") + #condition); \
        }                                                                      \
    } while (false)

// Evaluated regardless of `condition` so that side effects still happen.
#define CHECK_EQ(actual, expected)                                             \
    do {                                                                       \
        const auto dsh_actual = (actual);                                      \
        const auto dsh_expected = (expected);                                  \
        if (!(dsh_actual == dsh_expected)) {                                   \
            std::ostringstream dsh_message;                                    \
            dsh_message << std::setprecision(17) << #actual " == " #expected   \
                        << " (actual: " << dsh_actual                          \
                        << ", expected: " << dsh_expected << ")";              \
            ::dsh_test::reportFailure(__FILE__, __LINE__, dsh_message.str());  \
        }                                                                      \
    } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                                \
    do {                                                                       \
        const double dsh_actual = static_cast<double>(actual);                 \
        const double dsh_expected = static_cast<double>(expected);             \
        const double dsh_tolerance = static_cast<double>(tolerance);           \
        if (!::dsh_test::nearlyEqual(dsh_actual, dsh_expected, dsh_tolerance)) {\
            std::ostringstream dsh_message;                                    \
            dsh_message << std::setprecision(17) << #actual " ~= " #expected   \
                        << " (actual: " << dsh_actual                          \
                        << ", expected: " << dsh_expected                      \
                        << ", tolerance: " << dsh_tolerance                    \
                        << " [" #tolerance "])";                               \
            ::dsh_test::reportFailure(__FILE__, __LINE__, dsh_message.str());  \
        }                                                                      \
    } while (false)
