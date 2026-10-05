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
//
// Because the bound grows with the magnitude, this function CANNOT verify an
// absolute error bound. Use absoluteErrorWithin / CHECK_NEAR_ABS whenever the
// requirement is stated as an absolute error (for example the 1e-9 mm bound on
// the sample document total length).
inline bool nearlyEqual(double actual, double expected, double tolerance)
{
    const double scale = std::fmax(1.0, std::fmax(std::fabs(actual), std::fabs(expected)));
    return std::fabs(actual - expected) <= tolerance * scale;
}

// Strict absolute-error comparison: succeeds only when
// |actual - expected| <= tolerance, with the bound NOT scaled by magnitude.
//
// Rejected without comparing when `tolerance` is not a real non-negative
// number: a negative bound can never be met, and a NaN bound would silently
// make `difference <= tolerance` false rather than reporting the bad argument.
// Also rejects any non-finite operand or difference, so infinite inputs cannot
// slip through `inf <= tolerance`.
inline bool absoluteErrorWithin(double actual, double expected, double tolerance)
{
    if (!std::isfinite(tolerance) || tolerance < 0.0) {
        return false;
    }
    const double difference = std::fabs(actual - expected);
    return std::isfinite(difference) && difference <= tolerance;
}

// ---------------------------------------------------------------------------
// Assertion helpers
//
// The CHECK_* macros hand each operand to these functions as a nullary closure.
// Two reasons, both about what a macro must not do:
//
//   * A macro that declares its own temporary (the original form,
//     `const double dsh_actual = static_cast<double>(actual);`) puts that name
//     in scope inside the temporary's own initialiser. A caller expression that
//     mentions the same name therefore reads the just-declared, uninitialised
//     temporary instead of the caller's variable.
//   * Passing the operands as ordinary function arguments would fix that, but
//     it would also leave the evaluation ORDER unspecified, whereas the
//     original macro evaluated `actual` and then `expected` as separate
//     statements. Invoking the closures in order inside the helper restores
//     that guarantee and its snapshot semantics, while still declaring no name
//     in the caller's scope.
//
// The `...Text` parameters are the stringised source expressions, so failure
// messages keep naming what the test actually wrote.
// ---------------------------------------------------------------------------

template <typename ActualFn, typename ExpectedFn>
inline void checkEqual(ActualFn actualFn, ExpectedFn expectedFn,
                       const char* actualText, const char* expectedText,
                       const char* file, int line)
{
    const auto actual = actualFn();
    const auto expected = expectedFn();
    if (!(actual == expected)) {
        std::ostringstream message;
        message << std::setprecision(17) << actualText << " == " << expectedText
                << " (actual: " << actual << ", expected: " << expected << ")";
        reportFailure(file, line, message.str());
    }
}

template <typename ActualFn, typename ExpectedFn, typename ToleranceFn>
inline void checkNear(ActualFn actualFn, ExpectedFn expectedFn,
                      ToleranceFn toleranceFn, const char* actualText,
                      const char* expectedText, const char* toleranceText,
                      const char* file, int line)
{
    const double actual = static_cast<double>(actualFn());
    const double expected = static_cast<double>(expectedFn());
    const double tolerance = static_cast<double>(toleranceFn());
    if (!nearlyEqual(actual, expected, tolerance)) {
        std::ostringstream message;
        message << std::setprecision(17) << actualText << " ~= " << expectedText
                << " (actual: " << actual << ", expected: " << expected
                << ", tolerance: " << tolerance << " [" << toleranceText << "])";
        reportFailure(file, line, message.str());
    }
}

template <typename ActualFn, typename ExpectedFn, typename ToleranceFn>
inline void checkNearAbs(ActualFn actualFn, ExpectedFn expectedFn,
                         ToleranceFn toleranceFn, const char* actualText,
                         const char* expectedText, const char* toleranceText,
                         const char* file, int line)
{
    const double actual = static_cast<double>(actualFn());
    const double expected = static_cast<double>(expectedFn());
    const double tolerance = static_cast<double>(toleranceFn());
    if (!absoluteErrorWithin(actual, expected, tolerance)) {
        std::ostringstream message;
        message << std::setprecision(17) << "|" << actualText << " - "
                << expectedText << "| <= " << tolerance
                << " (actual: " << actual << ", expected: " << expected
                << ", abs error: " << std::fabs(actual - expected)
                << ", tolerance: " << tolerance << " [" << toleranceText << "])";
        reportFailure(file, line, message.str());
    }
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

// Each operand is evaluated exactly once, unconditionally, in the caller's
// scope and in the order `actual`, `expected`, `tolerance` (see the assertion
// helpers above for why the operands are passed as closures).
#define CHECK_EQ(actual, expected)                                             \
    ::dsh_test::checkEqual([&] { return (actual); },                           \
                           [&] { return (expected); }, #actual, #expected,     \
                           __FILE__, __LINE__)

#define CHECK_NEAR(actual, expected, tolerance)                                \
    ::dsh_test::checkNear([&] { return (actual); },                            \
                          [&] { return (expected); },                          \
                          [&] { return (tolerance); }, #actual, #expected,     \
                          #tolerance, __FILE__, __LINE__)

// Strict absolute-error assertion. Use this instead of CHECK_NEAR whenever the
// requirement is an absolute error bound, because CHECK_NEAR scales its bound
// by the magnitude of the values.
#define CHECK_NEAR_ABS(actual, expected, tolerance)                            \
    ::dsh_test::checkNearAbs([&] { return (actual); },                         \
                             [&] { return (expected); },                       \
                             [&] { return (tolerance); }, #actual, #expected,  \
                             #tolerance, __FILE__, __LINE__)
