#include "core/document.hpp"

#include <utility>

namespace xm::core {

std::uint64_t GeometryDocument::add(Shape shape, std::string layer,
                                    Transform2d transform,
                                    std::vector<std::string> warnings)
{
    const std::uint64_t id = nextId_;
    GeometryEntity entity;
    entity.id = id;
    entity.layer = std::move(layer);
    entity.shape = std::move(shape);
    entity.transform = transform;
    entity.warnings = std::move(warnings);

    entities_.push_back(std::move(entity));
    ++nextId_;
    return id;
}

void GeometryDocument::addLayer(Layer layer)
{
    layers_.push_back(std::move(layer));
}

void GeometryDocument::addWarning(std::string message)
{
    warnings_.push_back(std::move(message));
}

}  // namespace xm::core
