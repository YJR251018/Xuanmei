// Dependency-free writer for the project's deliberately small ASCII DXF
// subset. The emitted file declares R12 (AC1009); an internal Polyline is
// therefore encoded as the legacy POLYLINE/VERTEX/SEQEND sequence rather than
// being mislabeled as the later LWPOLYLINE entity.

#pragma once

#include <filesystem>
#include <iosfwd>

#include "core/document.hpp"

namespace xm::core {

class IDxfWriter {
public:
    virtual ~IDxfWriter() = default;
    virtual void write(const GeometryDocument& document, std::ostream& output) const = 0;
};

class MinimalDxfWriter final : public IDxfWriter {
public:
    // Serializes a validated millimetre document. Invalid geometry,
    // non-identity transforms and values that could break DXF line pairing are
    // rejected with std::invalid_argument before anything is written.
    void write(const GeometryDocument& document, std::ostream& output) const override;
};

// Convenience entry point. The document is serialized before the destination
// is opened, so validation failures cannot truncate an existing file.
void writeDxfFile(const GeometryDocument& document, const std::filesystem::path& path);

}  // namespace xm::core
