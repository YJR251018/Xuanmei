#include "app/workbench_session.hpp"

#include <cmath>
#include <iomanip>
#include <numbers>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/dxf_reader.hpp"
#include "core/dxf_writer.hpp"
#include "core/sample_document.hpp"

namespace xm::app {
namespace {

constexpr double kRoundTripTolerance = 1e-6;

std::string joinWarnings(const std::vector<std::string>& warnings)
{
    std::ostringstream text;
    for (std::size_t index = 0; index < warnings.size(); ++index) {
        if (index != 0U) {
            text << "; ";
        }
        text << warnings[index];
    }
    return std::move(text).str();
}

void ensureParentDirectory(const std::filesystem::path& path)
{
    const std::filesystem::path parent = path.parent_path();
    if (parent.empty()) {
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
        throw std::runtime_error("无法创建输出目录 " + parent.string()
                                 + ": " + error.message());
    }
}

bool absoluteErrorWithin(double actual, double expected, double tolerance) noexcept
{
    return std::isfinite(actual) && std::isfinite(expected)
        && std::abs(actual - expected) <= tolerance;
}

void requireEqual(std::vector<std::string>& failures, std::string_view name,
                  std::size_t actual, std::size_t expected)
{
    if (actual != expected) {
        failures.push_back(std::string(name) + " expected "
                           + std::to_string(expected) + ", got "
                           + std::to_string(actual));
    }
}

void requireNear(std::vector<std::string>& failures, std::string_view name,
                 double actual, double expected, double tolerance)
{
    if (!absoluteErrorWithin(actual, expected, tolerance)) {
        std::ostringstream message;
        message << std::setprecision(17) << name << " expected " << expected
                << " +/- " << tolerance << ", got " << actual;
        failures.push_back(std::move(message).str());
    }
}

std::size_t kindCount(const core::DocumentMetrics& metrics,
                      std::string_view kind)
{
    const auto found = metrics.countsByKind.find(std::string(kind));
    return found == metrics.countsByKind.end() ? 0U : found->second;
}

std::vector<std::string> validateSample(const WorkbenchSession& session)
{
    const core::DocumentMetrics& metrics = session.metrics();
    std::vector<std::string> failures;
    requireEqual(failures, "entities", session.document().entities().size(), 7U);
    requireEqual(failures, "layers", session.document().layers().size(), 3U);
    requireEqual(failures, "LINE count", kindCount(metrics, "LINE"), 4U);
    requireEqual(failures, "CIRCLE count", kindCount(metrics, "CIRCLE"), 1U);
    requireEqual(failures, "ARC count", kindCount(metrics, "ARC"), 1U);
    requireEqual(failures, "POLYLINE count", kindCount(metrics, "POLYLINE"), 1U);
    requireEqual(failures, "declared closed polylines",
                 metrics.declaredClosedPolylineCount, 1U);
    requireEqual(failures, "warnings", session.warningCount(), 0U);
    if (!metrics.hasBounds) {
        failures.push_back("bounds are missing");
    }
    const double expectedLength = 1900.0 + 150.0 * std::numbers::pi;
    requireNear(failures, "total length", metrics.totalLength,
                expectedLength, kRoundTripTolerance);
    requireNear(failures, "minX", metrics.minX, 0.0, kRoundTripTolerance);
    requireNear(failures, "minY", metrics.minY, -200.0, kRoundTripTolerance);
    requireNear(failures, "maxX", metrics.maxX, 550.0, kRoundTripTolerance);
    requireNear(failures, "maxY", metrics.maxY, 450.0, kRoundTripTolerance);
    return failures;
}

}  // namespace

WorkbenchSession::WorkbenchSession(core::GeometryDocument document,
                                   core::DocumentMetrics metrics,
                                   std::vector<std::string> importWarnings,
                                   std::filesystem::path sourcePath)
    : document_(std::move(document)),
      metrics_(std::move(metrics)),
      importWarnings_(std::move(importWarnings)),
      sourcePath_(std::move(sourcePath))
{
}

WorkbenchSession WorkbenchSession::loadDxf(const std::filesystem::path& path)
{
    core::DxfReadResult result = core::readDxfFile(path);
    if (!result.ok()) {
        throw std::runtime_error("DXF 内容无法读取: " + joinWarnings(result.warnings));
    }

    core::GeometryDocument document = std::move(*result.document);
    core::DocumentMetrics metrics = core::Native2dAnalyzer{}.analyze(document);
    return WorkbenchSession(std::move(document), std::move(metrics),
                            std::move(result.warnings), path);
}

WorkbenchSession WorkbenchSession::createSampleRoundTrip(
    const std::filesystem::path& outputPath)
{
    ensureParentDirectory(outputPath);
    core::writeDxfFile(core::makeSampleDocument(), outputPath);
    return loadDxf(outputPath);
}

void WorkbenchSession::exportDxf(const std::filesystem::path& path) const
{
    ensureParentDirectory(path);
    core::writeDxfFile(document_, path);
}

std::size_t WorkbenchSession::warningCount() const noexcept
{
    return importWarnings_.size() + metrics_.warnings.size();
}

int runSelfTest(const std::filesystem::path& outputPath,
                std::ostream& output, std::ostream& errorOutput)
{
    try {
        const WorkbenchSession session =
            WorkbenchSession::createSampleRoundTrip(outputPath);
        const std::vector<std::string> failures = validateSample(session);
        if (!failures.empty()) {
            errorOutput << "selftest failed\n";
            for (const std::string& failure : failures) {
                errorOutput << "- " << failure << '\n';
            }
            return 1;
        }

        const core::DocumentMetrics& metrics = session.metrics();
        output << "entities=" << session.document().entities().size()
               << " layers=" << session.document().layers().size()
               << " closed=" << metrics.declaredClosedPolylineCount
               << " warnings=" << session.warningCount() << '\n';
        output << std::fixed << std::setprecision(9)
               << "totalLength=" << metrics.totalLength << '\n';
        output << std::setprecision(6)
               << "bounds=" << core::Native2dAnalyzer::width(metrics)
               << " x " << core::Native2dAnalyzer::height(metrics) << '\n';
        output << "dxf=" << outputPath.generic_string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        errorOutput << "selftest failed: " << error.what() << '\n';
        return 1;
    } catch (...) {
        errorOutput << "selftest failed: unknown error\n";
        return 1;
    }
}

}  // namespace xm::app
