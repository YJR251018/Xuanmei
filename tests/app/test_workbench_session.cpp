#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "app/workbench_session.hpp"
#include "dsh_test.hpp"

using xm::app::WorkbenchSession;
using xm::app::runSelfTest;

namespace {

std::filesystem::path testPath(std::string_view name)
{
    return std::filesystem::current_path() / std::string(name);
}

void removeFile(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::remove(path, error);
}

template <typename Exception, typename Function>
bool throwsContaining(Function&& function, std::string_view fragment)
{
    try {
        std::forward<Function>(function)();
    } catch (const Exception& error) {
        return std::string_view(error.what()).find(fragment) != std::string_view::npos;
    } catch (...) {
        return false;
    }
    return false;
}

}  // namespace

TEST(workbench_session, sample_round_trip_produces_the_acceptance_snapshot)
{
    const auto path = testPath("xuanmei_app_sample_round_trip.dxf");
    removeFile(path);

    const WorkbenchSession session = WorkbenchSession::createSampleRoundTrip(path);

    CHECK(std::filesystem::is_regular_file(path));
    CHECK_EQ(session.document().entities().size(), std::size_t{7});
    CHECK_EQ(session.document().layers().size(), std::size_t{3});
    CHECK_EQ(session.metrics().declaredClosedPolylineCount, std::size_t{1});
    CHECK(session.importWarnings().empty());
    CHECK(session.metrics().warnings.empty());
    CHECK_NEAR_ABS(session.metrics().totalLength, 2371.238898038, 1e-6);
    CHECK_NEAR_ABS(session.metrics().minX, 0.0, 1e-6);
    CHECK_NEAR_ABS(session.metrics().minY, -200.0, 1e-6);
    CHECK_NEAR_ABS(session.metrics().maxX, 550.0, 1e-6);
    CHECK_NEAR_ABS(session.metrics().maxY, 450.0, 1e-6);
    CHECK(session.sourcePath() == path);

    removeFile(path);
}

TEST(workbench_session, damaged_dxf_is_reported_as_a_content_error)
{
    const auto path = testPath("xuanmei_app_damaged_input.dxf");
    removeFile(path);
    {
        std::ofstream output(path, std::ios::binary);
        output << "not-a-group-code\nSECTION\n";
    }

    CHECK(throwsContaining<std::runtime_error>(
        [&] { static_cast<void>(WorkbenchSession::loadDxf(path)); }, "第 1 行"));

    removeFile(path);
}

TEST(workbench_session, reader_and_analyzer_warnings_remain_distinguishable)
{
    const auto path = testPath("xuanmei_app_warning_sources.dxf");
    removeFile(path);
    {
        std::ofstream output(path, std::ios::binary);
        output
            << "0\nSECTION\n2\nENTITIES\n"
            << "0\nTEXT\n8\n0\n1\nignored\n"
            << "0\nCIRCLE\n8\n0\n10\n0\n20\n0\n40\n-1\n"
            << "0\nENDSEC\n0\nEOF\n";
    }

    const WorkbenchSession session = WorkbenchSession::loadDxf(path);

    CHECK_EQ(session.importWarnings().size(), std::size_t{1});
    CHECK_EQ(session.metrics().warnings.size(), std::size_t{1});
    CHECK_EQ(session.warningCount(), std::size_t{2});
    CHECK_EQ(session.document().entities().size(), std::size_t{1});

    removeFile(path);
}

TEST(workbench_session, export_writes_the_current_round_tripped_document)
{
    const auto source = testPath("xuanmei_app_export_source.dxf");
    const auto destination = testPath("xuanmei_app_export_destination.dxf");
    removeFile(source);
    removeFile(destination);

    const WorkbenchSession session = WorkbenchSession::createSampleRoundTrip(source);
    session.exportDxf(destination);
    const WorkbenchSession reloaded = WorkbenchSession::loadDxf(destination);

    CHECK_EQ(reloaded.document().entities().size(), std::size_t{7});
    CHECK_NEAR_ABS(reloaded.metrics().totalLength,
                   session.metrics().totalLength, 1e-6);
    CHECK(reloaded.importWarnings().empty());

    removeFile(source);
    removeFile(destination);
}

TEST(workbench_session, selftest_reports_machine_readable_acceptance_values)
{
    const auto path = testPath("xuanmei_app_selftest.dxf");
    removeFile(path);
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runSelfTest(path, output, error);

    CHECK_EQ(exitCode, 0);
    CHECK(error.str().empty());
    CHECK(output.str().find("entities=7 layers=3 closed=1 warnings=0")
          != std::string::npos);
    CHECK(output.str().find("totalLength=2371.238898038\n")
          != std::string::npos);
    CHECK(output.str().find("bounds=550.000000 x 650.000000")
          != std::string::npos);

    removeFile(path);
}

TEST(workbench_session, selftest_returns_failure_when_output_is_not_a_file)
{
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = runSelfTest(std::filesystem::current_path(), output, error);

    CHECK_EQ(exitCode, 1);
    CHECK(output.str().empty());
    CHECK(error.str().find("selftest failed") != std::string::npos);
}
