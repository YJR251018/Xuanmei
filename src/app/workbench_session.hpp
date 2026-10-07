// Application service that owns one loaded document and its derived metrics.
//
// This layer deliberately has no Qt dependency. The GUI and --selftest both
// use the same workflow instead of duplicating DXF or analysis logic.

#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

#include "core/document.hpp"
#include "core/native_analyzer.hpp"

namespace xm::app {

class WorkbenchSession {
public:
    static WorkbenchSession loadDxf(const std::filesystem::path& path);
    static WorkbenchSession createSampleRoundTrip(
        const std::filesystem::path& outputPath);

    void exportDxf(const std::filesystem::path& path) const;

    const core::GeometryDocument& document() const noexcept { return document_; }
    const core::DocumentMetrics& metrics() const noexcept { return metrics_; }
    const std::vector<std::string>& importWarnings() const noexcept
    {
        return importWarnings_;
    }
    const std::filesystem::path& sourcePath() const noexcept { return sourcePath_; }
    std::size_t warningCount() const noexcept;

private:
    WorkbenchSession(core::GeometryDocument document,
                     core::DocumentMetrics metrics,
                     std::vector<std::string> importWarnings,
                     std::filesystem::path sourcePath);

    core::GeometryDocument document_;
    core::DocumentMetrics metrics_;
    std::vector<std::string> importWarnings_;
    std::filesystem::path sourcePath_;
};

// Runs the exact startup round trip and validates the frozen MVP acceptance
// figures. Returns 0 on success and 1 on any content or environmental failure.
int runSelfTest(const std::filesystem::path& outputPath,
                std::ostream& output, std::ostream& errorOutput);

}  // namespace xm::app
