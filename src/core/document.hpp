// GeometryDocument: the project's own intermediate 2D model.
//
// This is the boundary described in docs/DECISIONS.md ADR-001: formats
// (libcdr, DXF) and toolkits (Qt, OpenCASCADE) convert into this model and back
// out, and none of their types may appear here. The core layer is Qt-free.
//
// Slice 2 provides the container only. Metrics live in native_analyzer (slice
// 3), DXF reading and writing in slices 4 and 5.

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/entity.hpp"

namespace xm::core {

struct Layer {
    std::string name;
    int colorIndex = 7;   // DXF ACI colour index; 7 is the default white/black
    bool visible = true;
};

class GeometryDocument {
public:
    static constexpr const char* kUnitMillimeter = "mm";

    // Appends an entity and returns its assigned id.
    //
    // Ids are allocated monotonically starting at 1, so the first added entity
    // has id 1 and the n-th has id n. `layer` defaults to "0" and `transform`
    // to the identity.
    std::uint64_t add(Shape shape, std::string layer = "0",
                      Transform2d transform = Transform2d::identity());

    // Appends a layer. This is a plain append with no de-duplication: the
    // caller is responsible for adding each layer name once. The signature
    // carries no result, so a silent no-op on a duplicate would be
    // indistinguishable from success; keeping it an append makes the behaviour
    // observable through layers().
    void addLayer(Layer layer);

    // Document-level warning, not bound to a single entity.
    void addWarning(std::string message);

    const std::vector<GeometryEntity>& entities() const { return entities_; }
    const std::vector<Layer>& layers() const { return layers_; }
    const std::vector<std::string>& warnings() const { return warnings_; }

    // Kept as a value return so callers cannot retain an alias invalidated by
    // a later setUnit() call.
    std::string unit() const { return unit_; }
    void setUnit(std::string unit) { unit_ = std::move(unit); }

private:
    std::uint64_t nextId_ = 1;
    std::vector<GeometryEntity> entities_;
    std::vector<Layer> layers_;
    std::vector<std::string> warnings_;
    std::string unit_ = kUnitMillimeter;
};

}  // namespace xm::core
