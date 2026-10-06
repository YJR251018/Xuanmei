#include "core/sample_document.hpp"

#include <utility>

namespace xm::core {
namespace {

Point2d at(double x, double y)
{
    return Point2d{x, y};
}

}  // namespace

GeometryDocument makeSampleDocument()
{
    GeometryDocument document;

    document.addLayer(Layer{"OUTLINE", 7, true});
    document.addLayer(Layer{"HOLES", 7, true});
    document.addLayer(Layer{"SLOT", 7, true});

    // Entity 1-4: the 300 mm square, counter-clockwise from the origin.
    document.add(Line{at(0.0, 0.0), at(300.0, 0.0)}, "OUTLINE");
    document.add(Line{at(300.0, 0.0), at(300.0, 300.0)}, "OUTLINE");
    document.add(Line{at(300.0, 300.0), at(0.0, 300.0)}, "OUTLINE");
    document.add(Line{at(0.0, 300.0), at(0.0, 0.0)}, "OUTLINE");

    // Entity 5: a full circle. A full circle is always a Circle, never an Arc
    // with equal angles (see entity.hpp).
    document.add(Circle{at(450.0, 150.0), 50.0}, "HOLES");

    // Entity 6: a quarter arc, sweeping counter-clockwise from 0 to 90 degrees.
    document.add(Arc{at(450.0, 350.0), 100.0, 0.0, 90.0}, "HOLES");

    // Entity 7: a closed four-vertex slot. Straight segments only; the MVP
    // model has no bulge/arc segments in polylines.
    Polyline slot;
    slot.points = {at(0.0, -200.0), at(200.0, -200.0), at(200.0, -50.0), at(0.0, -50.0)};
    slot.closed = true;
    document.add(std::move(slot), "SLOT");

    return document;
}

}  // namespace xm::core
