// Native closed-form metrics for GeometryDocument.
//
// This implementation is deliberately standard-library-only. It is the
// stage-A analyzer behind the IGeometryAnalyzer boundary and can later be
// replaced by an OpenCASCADE-backed implementation without changing callers.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/document.hpp"

namespace xm::core {

struct EntityMetric {
    std::uint64_t id = 0;
    std::string kind;
    std::string layer;
    double length = 0.0;
    bool closed = false;
    bool hasWarning = false;
};

struct DocumentMetrics {
    std::map<std::string, std::size_t> countsByKind;
    std::map<std::string, std::size_t> countsByLayer;
    double totalLength = 0.0;
    std::size_t closedCount = 0;
    bool hasBounds = false;
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    std::vector<EntityMetric> perEntity;
    std::vector<std::string> warnings;
};

class IGeometryAnalyzer {
public:
    virtual ~IGeometryAnalyzer() = default;
    virtual DocumentMetrics analyze(const GeometryDocument& document) const = 0;
};

class Native2dAnalyzer final : public IGeometryAnalyzer {
public:
    DocumentMetrics analyze(const GeometryDocument& document) const override;

    static double width(const DocumentMetrics& metrics) noexcept;
    static double height(const DocumentMetrics& metrics) noexcept;
};

}  // namespace xm::core
