// Unit and acceptance tests for the standard-library-only native analyzer.

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <string>

#include "core/document.hpp"
#include "core/native_analyzer.hpp"
#include "core/sample_document.hpp"
#include "dsh_test.hpp"

using xm::core::Arc;
using xm::core::Circle;
using xm::core::DocumentMetrics;
using xm::core::GeometryDocument;
using xm::core::IGeometryAnalyzer;
using xm::core::Line;
using xm::core::Native2dAnalyzer;
using xm::core::Point2d;
using xm::core::Polyline;
using xm::core::Transform2d;

namespace {

constexpr double kTolerance = 1e-9;

Point2d pt(double x, double y)
{
    return Point2d{x, y};
}

DocumentMetrics analyze(const GeometryDocument& document)
{
    return Native2dAnalyzer{}.analyze(document);
}

void checkBounds(const DocumentMetrics& metrics,
                 double minX, double minY, double maxX, double maxY)
{
    CHECK(metrics.hasBounds);
    CHECK_NEAR_ABS(metrics.minX, minX, kTolerance);
    CHECK_NEAR_ABS(metrics.minY, minY, kTolerance);
    CHECK_NEAR_ABS(metrics.maxX, maxX, kTolerance);
    CHECK_NEAR_ABS(metrics.maxY, maxY, kTolerance);
    CHECK_NEAR_ABS(Native2dAnalyzer::width(metrics), maxX - minX, kTolerance);
    CHECK_NEAR_ABS(Native2dAnalyzer::height(metrics), maxY - minY, kTolerance);
}

}  // namespace

TEST(native_analyzer, empty_document_has_zero_metrics)
{
    const DocumentMetrics metrics = analyze(GeometryDocument{});

    CHECK(metrics.countsByKind.empty());
    CHECK(metrics.countsByLayer.empty());
    CHECK_NEAR_ABS(metrics.totalLength, 0.0, 0.0);
    CHECK_EQ(metrics.declaredClosedPolylineCount, std::size_t{0});
    CHECK(!metrics.hasBounds);
    CHECK_NEAR_ABS(Native2dAnalyzer::width(metrics), 0.0, 0.0);
    CHECK_NEAR_ABS(Native2dAnalyzer::height(metrics), 0.0, 0.0);
    CHECK(metrics.perEntity.empty());
    CHECK(metrics.warnings.empty());
}

TEST(native_analyzer, line_metrics_include_length_and_endpoints)
{
    GeometryDocument document;
    document.add(Line{pt(-2.0, 3.0), pt(4.0, 11.0)}, "CUT");
    const DocumentMetrics metrics = analyze(document);

    CHECK_EQ(metrics.countsByKind.at("LINE"), std::size_t{1});
    CHECK_EQ(metrics.countsByLayer.at("CUT"), std::size_t{1});
    CHECK_NEAR_ABS(metrics.totalLength, 10.0, kTolerance);
    checkBounds(metrics, -2.0, 3.0, 4.0, 11.0);
    CHECK_EQ(metrics.perEntity.size(), std::size_t{1});
    CHECK_EQ(metrics.perEntity[0].id, std::uint64_t{1});
    CHECK_EQ(metrics.perEntity[0].kind, std::string("LINE"));
    CHECK_EQ(metrics.perEntity[0].layer, std::string("CUT"));
    CHECK_NEAR_ABS(metrics.perEntity[0].length, 10.0, kTolerance);
    CHECK(!metrics.perEntity[0].declaredClosed);
    CHECK(!metrics.perEntity[0].hasWarning);
}

TEST(native_analyzer, circle_uses_its_exact_extrema)
{
    GeometryDocument document;
    document.add(Circle{pt(5.0, -3.0), 2.0});
    const DocumentMetrics metrics = analyze(document);

    CHECK_NEAR_ABS(metrics.totalLength, 4.0 * std::numbers::pi, kTolerance);
    checkBounds(metrics, 3.0, -5.0, 7.0, -1.0);
}

TEST(native_analyzer, first_quadrant_arc_has_tight_bounds)
{
    GeometryDocument document;
    document.add(Arc{pt(10.0, 20.0), 5.0, 0.0, 90.0});
    const DocumentMetrics metrics = analyze(document);

    CHECK_NEAR_ABS(metrics.totalLength, 2.5 * std::numbers::pi, kTolerance);
    checkBounds(metrics, 10.0, 20.0, 15.0, 25.0);
}

TEST(native_analyzer, arc_crossing_zero_includes_only_reached_cardinal)
{
    GeometryDocument document;
    document.add(Arc{pt(0.0, 0.0), 10.0, 315.0, 45.0});
    const DocumentMetrics metrics = analyze(document);
    const double diagonal = 10.0 / std::sqrt(2.0);

    checkBounds(metrics, diagonal, -diagonal, 10.0, diagonal);
}

TEST(native_analyzer, narrow_arc_excludes_unreached_cardinals)
{
    GeometryDocument document;
    document.add(Arc{pt(0.0, 0.0), 10.0, 10.0, 20.0});
    const DocumentMetrics metrics = analyze(document);

    checkBounds(metrics,
                10.0 * std::cos(xm::core::toRadians(20.0)),
                10.0 * std::sin(xm::core::toRadians(10.0)),
                10.0 * std::cos(xm::core::toRadians(10.0)),
                10.0 * std::sin(xm::core::toRadians(20.0)));
}

TEST(native_analyzer, semicircle_includes_all_reached_cardinals)
{
    GeometryDocument document;
    document.add(Arc{pt(2.0, 3.0), 4.0, 90.0, 270.0});
    const DocumentMetrics metrics = analyze(document);

    checkBounds(metrics, -2.0, -1.0, 2.0, 7.0);
}

TEST(native_analyzer, zero_sweep_arc_is_empty)
{
    GeometryDocument document;
    document.add(Arc{pt(100.0, 200.0), 50.0, 45.0, 45.0});
    const DocumentMetrics metrics = analyze(document);

    CHECK_NEAR_ABS(metrics.totalLength, 0.0, 0.0);
    CHECK(!metrics.hasBounds);
    CHECK(metrics.warnings.empty());
}

TEST(native_analyzer, sub_trillionth_degree_arc_remains_measurable)
{
    GeometryDocument document;
    document.add(Arc{pt(0.0, 0.0), 10.0, 0.0, 5e-13});
    const DocumentMetrics metrics = analyze(document);

    CHECK(metrics.totalLength > 0.0);
    CHECK(metrics.hasBounds);
    CHECK(metrics.maxY > 0.0);
    CHECK(metrics.warnings.empty());
}

TEST(native_analyzer, closed_polyline_sets_declared_count_and_vertex_bounds)
{
    GeometryDocument document;
    document.add(Polyline{{pt(-1.0, -2.0), pt(4.0, -2.0), pt(4.0, 3.0)}, true});
    const DocumentMetrics metrics = analyze(document);

    CHECK_EQ(metrics.declaredClosedPolylineCount, std::size_t{1});
    CHECK(metrics.perEntity[0].declaredClosed);
    checkBounds(metrics, -1.0, -2.0, 4.0, 3.0);
}

TEST(native_analyzer, degenerate_closed_polyline_is_declared_closed_but_warned)
{
    GeometryDocument document;
    document.add(Polyline{{pt(2.0, 3.0), pt(2.0, 3.0)}, true});
    const DocumentMetrics metrics = analyze(document);

    CHECK_EQ(metrics.declaredClosedPolylineCount, std::size_t{1});
    CHECK(metrics.perEntity[0].declaredClosed);
    CHECK(metrics.perEntity[0].hasWarning);
    CHECK_EQ(metrics.warnings.size(), std::size_t{1});
    CHECK(metrics.warnings[0].entityId.has_value());
    CHECK_EQ(*metrics.warnings[0].entityId, std::uint64_t{1});
    CHECK_EQ(metrics.warnings[0].message,
             std::string("闭合折线退化为零长度，未形成可用轮廓"));
}

TEST(native_analyzer, single_point_polyline_has_zero_size_bounds)
{
    GeometryDocument document;
    document.add(Polyline{{pt(7.0, -9.0)}, false});
    const DocumentMetrics metrics = analyze(document);

    CHECK_NEAR_ABS(metrics.totalLength, 0.0, 0.0);
    checkBounds(metrics, 7.0, -9.0, 7.0, -9.0);
}

TEST(native_analyzer, empty_polyline_has_no_bounds)
{
    GeometryDocument document;
    document.add(Polyline{});
    const DocumentMetrics metrics = analyze(document);

    CHECK_EQ(metrics.countsByKind.at("POLYLINE"), std::size_t{1});
    CHECK(!metrics.hasBounds);
    CHECK(metrics.warnings.empty());
}

TEST(native_analyzer, mixed_document_unions_bounds_and_counts)
{
    GeometryDocument document;
    document.add(Line{pt(-5.0, 1.0), pt(0.0, 1.0)}, "A");
    document.add(Circle{pt(10.0, 10.0), 2.0}, "B");
    document.add(Line{pt(20.0, -4.0), pt(21.0, -4.0)}, "A");
    const DocumentMetrics metrics = analyze(document);

    CHECK_EQ(metrics.countsByKind.at("LINE"), std::size_t{2});
    CHECK_EQ(metrics.countsByKind.at("CIRCLE"), std::size_t{1});
    CHECK_EQ(metrics.countsByLayer.at("A"), std::size_t{2});
    CHECK_EQ(metrics.countsByLayer.at("B"), std::size_t{1});
    checkBounds(metrics, -5.0, -4.0, 21.0, 12.0);
}

TEST(native_analyzer, negative_radius_is_counted_but_excluded_from_geometry_metrics)
{
    GeometryDocument document;
    document.add(Circle{pt(4.0, 5.0), -2.0}, "BAD");
    const DocumentMetrics metrics = analyze(document);

    CHECK_EQ(metrics.countsByKind.at("CIRCLE"), std::size_t{1});
    CHECK_NEAR_ABS(metrics.totalLength, 0.0, 0.0);
    CHECK(!metrics.hasBounds);
    CHECK(metrics.perEntity[0].hasWarning);
    CHECK_EQ(metrics.warnings.size(), std::size_t{1});
}

TEST(native_analyzer, non_finite_coordinates_do_not_poison_document_metrics)
{
    GeometryDocument document;
    const double infinity = std::numeric_limits<double>::infinity();
    document.add(Line{pt(0.0, 0.0), pt(infinity, 1.0)}, "BAD");
    document.add(Line{pt(1.0, 1.0), pt(4.0, 5.0)}, "GOOD");
    const DocumentMetrics metrics = analyze(document);

    CHECK_NEAR_ABS(metrics.totalLength, 5.0, kTolerance);
    checkBounds(metrics, 1.0, 1.0, 4.0, 5.0);
    CHECK(metrics.perEntity[0].hasWarning);
    CHECK_NEAR_ABS(metrics.perEntity[0].length, 0.0, 0.0);
    CHECK(!metrics.perEntity[1].hasWarning);
}

TEST(native_analyzer, non_identity_transform_is_not_applied_and_is_reported)
{
    GeometryDocument document;
    document.add(Line{pt(0.0, 0.0), pt(3.0, 4.0)}, "0",
                 Transform2d::translation(100.0, 200.0));
    const DocumentMetrics metrics = analyze(document);

    CHECK_NEAR_ABS(metrics.totalLength, 5.0, kTolerance);
    checkBounds(metrics, 0.0, 0.0, 3.0, 4.0);
    CHECK(metrics.perEntity[0].hasWarning);
    CHECK_EQ(metrics.warnings.size(), std::size_t{1});
    CHECK(metrics.warnings[0].entityId.has_value());
    CHECK_EQ(*metrics.warnings[0].entityId, std::uint64_t{1});
    CHECK_EQ(metrics.warnings[0].message,
             std::string("实体带非恒等变换，尚未参与求值"));
}

TEST(native_analyzer, document_and_entity_warnings_are_aggregated_in_order)
{
    GeometryDocument document;
    document.addWarning("document warning");
    document.add(Line{pt(0.0, 0.0), pt(1.0, 0.0)}, "0",
                 Transform2d::identity(), {"entity warning"});
    const DocumentMetrics metrics = analyze(document);

    CHECK_EQ(metrics.warnings.size(), std::size_t{2});
    CHECK(!metrics.warnings[0].entityId.has_value());
    CHECK_EQ(metrics.warnings[0].message, std::string("document warning"));
    CHECK(metrics.warnings[1].entityId.has_value());
    CHECK_EQ(*metrics.warnings[1].entityId, std::uint64_t{1});
    CHECK_EQ(metrics.warnings[1].message, std::string("entity warning"));
    CHECK(metrics.perEntity[0].hasWarning);
}

TEST(native_analyzer, identical_generated_warnings_keep_their_entity_ids)
{
    GeometryDocument document;
    document.add(Circle{pt(0.0, 0.0), -1.0});
    document.add(Circle{pt(5.0, 5.0), -2.0});
    const DocumentMetrics metrics = analyze(document);

    CHECK_EQ(metrics.warnings.size(), std::size_t{2});
    CHECK(metrics.warnings[0].entityId.has_value());
    CHECK(metrics.warnings[1].entityId.has_value());
    CHECK_EQ(*metrics.warnings[0].entityId, std::uint64_t{1});
    CHECK_EQ(*metrics.warnings[1].entityId, std::uint64_t{2});
    CHECK_EQ(metrics.warnings[0].message, metrics.warnings[1].message);
}

TEST(native_analyzer, implementation_is_usable_through_interface)
{
    std::unique_ptr<IGeometryAnalyzer> analyzer = std::make_unique<Native2dAnalyzer>();
    GeometryDocument document;
    document.add(Line{pt(0.0, 0.0), pt(0.0, 2.0)});

    const DocumentMetrics metrics = analyzer->analyze(document);
    CHECK_NEAR_ABS(metrics.totalLength, 2.0, kTolerance);
}

TEST(native_analyzer, sample_document_matches_all_acceptance_metrics)
{
    const DocumentMetrics metrics = analyze(xm::core::makeSampleDocument());

    CHECK_EQ(metrics.perEntity.size(), std::size_t{7});
    CHECK_EQ(metrics.countsByKind.at("LINE"), std::size_t{4});
    CHECK_EQ(metrics.countsByKind.at("CIRCLE"), std::size_t{1});
    CHECK_EQ(metrics.countsByKind.at("ARC"), std::size_t{1});
    CHECK_EQ(metrics.countsByKind.at("POLYLINE"), std::size_t{1});
    CHECK_EQ(metrics.countsByLayer.size(), std::size_t{3});
    CHECK_EQ(metrics.declaredClosedPolylineCount, std::size_t{1});
    CHECK_NEAR_ABS(metrics.totalLength,
                   1900.0 + 150.0 * std::numbers::pi,
                   kTolerance);
    checkBounds(metrics, 0.0, -200.0, 550.0, 450.0);
    CHECK(metrics.warnings.empty());
}
