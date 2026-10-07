#include "core/native_analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "core/entity.hpp"

namespace xm::core {
namespace {

constexpr const char* kTransformWarning =
    "实体带非恒等变换，尚未参与求值";
constexpr const char* kInvalidGeometryWarning =
    "实体包含无效或不可表示的几何值，已从相应统计中忽略";
constexpr const char* kDegenerateClosedPolylineWarning =
    "闭合折线退化为零长度，未形成可用轮廓";

struct BoundsAccumulator {
    bool hasBounds = false;
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;

    void include(const Point2d& point) noexcept
    {
        if (!hasBounds) {
            hasBounds = true;
            minX = maxX = point.x;
            minY = maxY = point.y;
            return;
        }

        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
    }

    void include(const BoundsAccumulator& other) noexcept
    {
        if (!other.hasBounds) {
            return;
        }
        include(Point2d{other.minX, other.minY});
        include(Point2d{other.maxX, other.maxY});
    }
};

enum class BoundsStatus {
    none,
    contributed,
    invalid,
};

bool isFinite(const Point2d& point) noexcept
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

Point2d pointOnCircle(const Point2d& center, double radius, double degrees) noexcept
{
    const double radians = toRadians(degrees);
    return Point2d{
        center.x + radius * std::cos(radians),
        center.y + radius * std::sin(radians),
    };
}

Point2d cardinalPoint(const Point2d& center, double radius, int degrees) noexcept
{
    switch (degrees) {
    case 0:
        return Point2d{center.x + radius, center.y};
    case 90:
        return Point2d{center.x, center.y + radius};
    case 180:
        return Point2d{center.x - radius, center.y};
    default:
        return Point2d{center.x, center.y - radius};
    }
}

bool angleIsOnSweep(double cardinalDegrees, double startDegrees, double sweepDegrees) noexcept
{
    const double offset = normalizeDegrees(cardinalDegrees - startDegrees);
    return offset <= sweepDegrees;
}

BoundsStatus boundsOf(const Line& line, BoundsAccumulator& bounds) noexcept
{
    if (!isFinite(line.from) || !isFinite(line.to)) {
        return BoundsStatus::invalid;
    }
    bounds.include(line.from);
    bounds.include(line.to);
    return BoundsStatus::contributed;
}

BoundsStatus boundsOf(const Circle& circle, BoundsAccumulator& bounds) noexcept
{
    if (!isFinite(circle.center) || !std::isfinite(circle.radius) || circle.radius <= 0.0) {
        return BoundsStatus::invalid;
    }

    const Point2d lower{circle.center.x - circle.radius, circle.center.y - circle.radius};
    const Point2d upper{circle.center.x + circle.radius, circle.center.y + circle.radius};
    if (!isFinite(lower) || !isFinite(upper)) {
        return BoundsStatus::invalid;
    }
    bounds.include(lower);
    bounds.include(upper);
    return BoundsStatus::contributed;
}

BoundsStatus boundsOf(const Arc& arc, BoundsAccumulator& bounds) noexcept
{
    if (!isFinite(arc.center) || !std::isfinite(arc.radius) || arc.radius <= 0.0
        || !std::isfinite(arc.startDegrees) || !std::isfinite(arc.endDegrees)) {
        return BoundsStatus::invalid;
    }

    const double sweep = arcSweepDegrees(arc);
    const double start = normalizeDegrees(arc.startDegrees);
    const double end = normalizeDegrees(arc.endDegrees);
    if (!std::isfinite(sweep) || !std::isfinite(start) || !std::isfinite(end)) {
        return BoundsStatus::invalid;
    }
    if (sweep == 0.0) {
        return BoundsStatus::none;
    }

    BoundsAccumulator candidate;
    candidate.include(pointOnCircle(arc.center, arc.radius, start));
    candidate.include(pointOnCircle(arc.center, arc.radius, end));
    for (const int cardinal : {0, 90, 180, 270}) {
        if (angleIsOnSweep(static_cast<double>(cardinal), start, sweep)) {
            candidate.include(cardinalPoint(arc.center, arc.radius, cardinal));
        }
    }

    const Point2d lower{candidate.minX, candidate.minY};
    const Point2d upper{candidate.maxX, candidate.maxY};
    if (!isFinite(lower) || !isFinite(upper)) {
        return BoundsStatus::invalid;
    }
    bounds.include(candidate);
    return BoundsStatus::contributed;
}

BoundsStatus boundsOf(const Polyline& polyline, BoundsAccumulator& bounds) noexcept
{
    if (polyline.points.empty()) {
        return BoundsStatus::none;
    }
    for (const Point2d& point : polyline.points) {
        if (!isFinite(point)) {
            return BoundsStatus::invalid;
        }
    }
    for (const Point2d& point : polyline.points) {
        bounds.include(point);
    }
    return BoundsStatus::contributed;
}

BoundsStatus boundsOf(const Shape& shape, BoundsAccumulator& bounds) noexcept
{
    return std::visit(
        [&bounds](const auto& concrete) noexcept { return boundsOf(concrete, bounds); },
        shape);
}

bool isDeclaredClosed(const Shape& shape) noexcept
{
    return std::visit(
        [](const auto& concrete) noexcept {
            using T = std::decay_t<decltype(concrete)>;
            if constexpr (std::is_same_v<T, Polyline>) {
                return concrete.closed;
            } else {
                return false;
            }
        },
        shape);
}

void addGeneratedWarning(DocumentMetrics& result, EntityMetric& metric, const char* warning)
{
    result.warnings.push_back(AnalysisWarning{metric.id, warning});
    metric.hasWarning = true;
}

}  // namespace

DocumentMetrics Native2dAnalyzer::analyze(const GeometryDocument& document) const
{
    DocumentMetrics result;
    result.warnings.reserve(document.warnings().size() + document.entities().size());
    for (const std::string& warning : document.warnings()) {
        result.warnings.push_back(AnalysisWarning{std::nullopt, warning});
    }
    result.perEntity.reserve(document.entities().size());

    BoundsAccumulator documentBounds;
    for (const GeometryEntity& entity : document.entities()) {
        EntityMetric metric;
        metric.id = entity.id;
        metric.kind = kindName(entity.shape);
        metric.layer = entity.layer;
        metric.declaredClosed = isDeclaredClosed(entity.shape);
        metric.hasWarning = !entity.warnings.empty();

        ++result.countsByKind[metric.kind];
        ++result.countsByLayer[metric.layer];
        if (metric.declaredClosed) {
            ++result.declaredClosedPolylineCount;
        }

        for (const std::string& warning : entity.warnings) {
            result.warnings.push_back(AnalysisWarning{entity.id, warning});
        }
        if (!entity.transform.isIdentity()) {
            addGeneratedWarning(result, metric, kTransformWarning);
        }

        BoundsAccumulator entityBounds;
        const BoundsStatus boundsStatus = boundsOf(entity.shape, entityBounds);
        const double entityLength = length(entity.shape);
        if (boundsStatus == BoundsStatus::invalid || !std::isfinite(entityLength)
            || entityLength < 0.0) {
            addGeneratedWarning(result, metric, kInvalidGeometryWarning);
        } else {
            metric.length = entityLength;
            result.totalLength += entityLength;
            if (boundsStatus == BoundsStatus::contributed) {
                documentBounds.include(entityBounds);
            }
            if (metric.declaredClosed && entityLength == 0.0) {
                addGeneratedWarning(result, metric, kDegenerateClosedPolylineWarning);
            }
        }

        result.perEntity.push_back(std::move(metric));
    }

    if (documentBounds.hasBounds) {
        result.hasBounds = true;
        result.minX = documentBounds.minX;
        result.minY = documentBounds.minY;
        result.maxX = documentBounds.maxX;
        result.maxY = documentBounds.maxY;
    }
    return result;
}

double Native2dAnalyzer::width(const DocumentMetrics& metrics) noexcept
{
    return metrics.hasBounds ? metrics.maxX - metrics.minX : 0.0;
}

double Native2dAnalyzer::height(const DocumentMetrics& metrics) noexcept
{
    return metrics.hasBounds ? metrics.maxY - metrics.minY : 0.0;
}

}  // namespace xm::core
