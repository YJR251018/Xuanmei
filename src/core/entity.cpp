#include "core/entity.hpp"

#include <type_traits>

namespace xm::core {

double length(const Line& line) noexcept
{
    return distance(line.from, line.to);
}

double length(const Circle& circle) noexcept
{
    if (!(circle.radius > 0.0)) {
        return 0.0;
    }
    return 2.0 * std::numbers::pi * circle.radius;
}

double length(const Arc& arc) noexcept
{
    if (!(arc.radius > 0.0)) {
        return 0.0;
    }

    const double sweep = arcSweepDegrees(arc);
    // Only an exactly zero sweep is degenerate. Using an arbitrary epsilon
    // here would silently discard valid, very short arcs and contradict the
    // closed-form length contract.
    if (sweep == 0.0) {
        return 0.0;
    }
    return toRadians(sweep) * arc.radius;
}

double length(const Polyline& polyline) noexcept
{
    const auto& points = polyline.points;
    if (points.size() < 2) {
        return 0.0;
    }

    double total = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        total += distance(points[i - 1], points[i]);
    }
    if (polyline.closed) {
        total += distance(points.back(), points.front());
    }
    return total;
}

double length(const Shape& shape) noexcept
{
    return std::visit(
        [](const auto& concrete) noexcept { return length(concrete); },
        shape);
}

const char* kindName(const Shape& shape) noexcept
{
    return std::visit(
        [](const auto& concrete) noexcept -> const char* {
            using T = std::decay_t<decltype(concrete)>;
            if constexpr (std::is_same_v<T, Line>) {
                return "LINE";
            } else if constexpr (std::is_same_v<T, Circle>) {
                return "CIRCLE";
            } else if constexpr (std::is_same_v<T, Arc>) {
                return "ARC";
            } else {
                return "LWPOLYLINE";
            }
        },
        shape);
}

}  // namespace xm::core
