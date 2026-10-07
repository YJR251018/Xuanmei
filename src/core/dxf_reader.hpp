// Tolerant reader for the ASCII DXF R12 subset emitted by MinimalDxfWriter.

#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "core/document.hpp"

namespace xm::core {

struct DxfReadResult {
    std::optional<GeometryDocument> document;
    std::vector<std::string> warnings;

    bool ok() const noexcept { return document.has_value(); }
};

// Content errors are reported through DxfReadResult. Fatal structural errors
// leave document empty; recoverable record errors use documented defaults.
DxfReadResult readDxf(std::istream& input);

// Opening the path is an environmental error and therefore throws. Once open,
// file content follows the same result contract as readDxf().
DxfReadResult readDxfFile(const std::filesystem::path& path);

}  // namespace xm::core
