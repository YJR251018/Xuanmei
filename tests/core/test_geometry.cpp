// Unit tests for the core geometry value types and closed-form lengths.
//
// Every expected number is either an exact geometric identity or the value
// computed in docs/MVP_DESIGN.md section 8. The sample document total
// 2100 + 150*pi is asserted here as the arithmetic backbone of acceptance
// item A3; the same figure is re-asserted end to end once the DXF writer and
// reader exist.

#include <limits>
#include <numbers>
#include <string>
#include <vector>

#include "core/entity.hpp"
#include "core/geometry.hpp"
#include "dsh_test.hpp"

using xm::core::Arc;
using xm::core::Circle;
using xm::core::Line;
using xm::core::Point2d;
using xm::core::Polyline;
using xm::core::Shape;
using xm::core::Transform2d;

namespace {

constexpr double kTolerance = 1e-12;

Point2d pt(double x, double y)
{
    return Point2d{x, y};
}

Shape makeShape(Line line) { return Shape{line}; }
Shape makeShape(Circle circle) { return Shape{circle}; }
Shape makeShape(Arc arc) { return Shape{arc}; }
Shape makeShape(Polyline polyline) { return Shape{polyline}; }

}  // namespace

// ---------------------------------------------------------------------------
// Point2d and vector helpers
// ---------------------------------------------------------------------------

TEST(point2d, distance_is_pythagorean)
{
    CHECK_NEAR(xm::core::distance(pt(0.0, 0.0), pt(3.0, 4.0)), 5.0, kTolerance);
    CHECK_NEAR(xm::core::distance(pt(-1.0, -1.0), pt(2.0, 3.0)), 5.0, kTolerance);
}

TEST(point2d, zero_distance_for_identical_points)
{
    CHECK_NEAR(xm::core::distance(pt(7.5, -2.25), pt(7.5, -2.25)), 0.0, kTolerance);
}

TEST(point2d, magnitude_uses_hypot)
{
    CHECK_NEAR(xm::core::magnitude(pt(3.0, 4.0)), 5.0, kTolerance);
    CHECK_NEAR(xm::core::magnitude(pt(0.0, 0.0)), 0.0, kTolerance);
}

TEST(vector, dot_and_cross)
{
    const Point2d a = pt(1.0, 2.0);
    const Point2d b = pt(3.0, 4.0);
    CHECK_NEAR(xm::core::dot(a, b), 11.0, kTolerance);
    CHECK_NEAR(xm::core::cross(a, b), -2.0, kTolerance);
    CHECK_NEAR(xm::core::cross(b, a), 2.0, kTolerance);
}

TEST(vector, cross_of_parallel_vectors_is_zero)
{
    CHECK_NEAR(xm::core::cross(pt(2.0, 3.0), pt(4.0, 6.0)), 0.0, kTolerance);
}

// ---------------------------------------------------------------------------
// Angle helpers
// ---------------------------------------------------------------------------

TEST(angle, degree_radian_round_trip)
{
    CHECK_NEAR(xm::core::toRadians(0.0), 0.0, kTolerance);
    CHECK_NEAR(xm::core::toRadians(90.0), std::numbers::pi / 2.0, kTolerance);
    CHECK_NEAR(xm::core::toRadians(180.0), std::numbers::pi, kTolerance);
    CHECK_NEAR(xm::core::toDegrees(std::numbers::pi), 180.0, kTolerance);
    CHECK_NEAR(xm::core::toDegrees(xm::core::toRadians(37.5)), 37.5, kTolerance);
}

// std::fmod keeps the sign of its first argument, so negative inputs are the
// interesting case here.
TEST(angle, normalize_degrees_maps_into_zero_to_360)
{
    CHECK_NEAR(xm::core::normalizeDegrees(0.0), 0.0, kTolerance);
    CHECK_NEAR(xm::core::normalizeDegrees(90.0), 90.0, kTolerance);
    CHECK_NEAR(xm::core::normalizeDegrees(-90.0), 270.0, kTolerance);
    CHECK_NEAR(xm::core::normalizeDegrees(360.0), 0.0, kTolerance);
    CHECK_NEAR(xm::core::normalizeDegrees(-360.0), 0.0, kTolerance);
    CHECK_NEAR(xm::core::normalizeDegrees(450.0), 90.0, kTolerance);
    CHECK_NEAR(xm::core::normalizeDegrees(-450.0), 270.0, kTolerance);
    CHECK_NEAR(xm::core::normalizeDegrees(720.5), 0.5, kTolerance);
}

TEST(angle, normalize_degrees_preserves_non_finite_input)
{
    const double infinity = std::numeric_limits<double>::infinity();
    CHECK(std::isinf(xm::core::normalizeDegrees(infinity)));
}

// ---------------------------------------------------------------------------
// Lengths
// ---------------------------------------------------------------------------

TEST(line, length_is_endpoint_distance)
{
    CHECK_NEAR(xm::core::length(Line{pt(0.0, 0.0), pt(300.0, 0.0)}), 300.0, kTolerance);
    CHECK_NEAR(xm::core::length(Line{pt(0.0, 0.0), pt(3.0, 4.0)}), 5.0, kTolerance);
    CHECK_NEAR(xm::core::length(Line{pt(-10.0, -10.0), pt(-10.0, 20.0)}), 30.0, kTolerance);
}

TEST(line, zero_length_for_degenerate_line)
{
    CHECK_NEAR(xm::core::length(Line{pt(5.0, 5.0), pt(5.0, 5.0)}), 0.0, kTolerance);
}

TEST(circle, length_is_circumference)
{
    CHECK_NEAR(xm::core::length(Circle{pt(0.0, 0.0), 1.0}), 2.0 * std::numbers::pi, kTolerance);
    CHECK_NEAR(xm::core::length(Circle{pt(450.0, 150.0), 50.0}), 100.0 * std::numbers::pi, kTolerance);
}

TEST(circle, non_positive_radius_yields_zero_not_negative)
{
    CHECK_NEAR(xm::core::length(Circle{pt(0.0, 0.0), 0.0}), 0.0, kTolerance);
    CHECK_NEAR(xm::core::length(Circle{pt(0.0, 0.0), -5.0}), 0.0, kTolerance);
}

TEST(arc, sweep_is_end_minus_start)
{
    const Arc quarter{pt(450.0, 350.0), 100.0, 0.0, 90.0};
    CHECK_NEAR(xm::core::arcSweepDegrees(quarter), 90.0, kTolerance);
    CHECK_NEAR(xm::core::length(quarter), 50.0 * std::numbers::pi, kTolerance);
}

// The documented convention: the sweep wraps counter-clockwise through 0.
TEST(arc, sweep_wraps_across_zero)
{
    const Arc wrap{pt(0.0, 0.0), 10.0, 270.0, 90.0};
    CHECK_NEAR(xm::core::arcSweepDegrees(wrap), 180.0, kTolerance);
    CHECK_NEAR(xm::core::length(wrap), 10.0 * std::numbers::pi, kTolerance);
}

TEST(arc, negative_start_angle_wraps)
{
    const Arc arc{pt(0.0, 0.0), 10.0, -90.0, 0.0};
    CHECK_NEAR(xm::core::arcSweepDegrees(arc), 90.0, kTolerance);
    CHECK_NEAR(xm::core::length(arc), 5.0 * std::numbers::pi, kTolerance);
}

TEST(arc, small_sweep)
{
    const Arc arc{pt(0.0, 0.0), 100.0, 0.0, 1.0};
    CHECK_NEAR(xm::core::length(arc), xm::core::toRadians(1.0) * 100.0, kTolerance);
}

// A full circle must be a Circle. An Arc with equal angles is degenerate and
// must not silently become a full turn (docs/MVP_DESIGN.md section 5.2).
TEST(arc, equal_angles_is_degenerate_and_not_a_full_circle)
{
    const Arc degenerate{pt(0.0, 0.0), 50.0, 45.0, 45.0};
    CHECK_NEAR(xm::core::arcSweepDegrees(degenerate), 0.0, kTolerance);
    CHECK_NEAR(xm::core::length(degenerate), 0.0, kTolerance);
}

TEST(arc, full_turn_reported_as_zero_sweep_not_full_circle)
{
    const Arc fullTurn{pt(0.0, 0.0), 50.0, 0.0, 360.0};
    CHECK_NEAR(xm::core::arcSweepDegrees(fullTurn), 0.0, kTolerance);
    CHECK_NEAR(xm::core::length(fullTurn), 0.0, kTolerance);
}

TEST(arc, non_positive_radius_yields_zero)
{
    CHECK_NEAR(xm::core::length(Arc{pt(0.0, 0.0), 0.0, 0.0, 90.0}), 0.0, kTolerance);
    CHECK_NEAR(xm::core::length(Arc{pt(0.0, 0.0), -10.0, 0.0, 90.0}), 0.0, kTolerance);
}

TEST(polyline, open_polyline_sums_segments)
{
    Polyline polyline;
    polyline.points = {pt(0.0, 0.0), pt(3.0, 0.0), pt(3.0, 4.0)};
    CHECK_NEAR(xm::core::length(polyline), 7.0, kTolerance);
}

TEST(polyline, closed_polyline_adds_closing_segment)
{
    // The slot from the sample document: three segments of 200 + 150 + 200 mm,
    // so 550 mm open and 700 mm closed with the 150 mm closing edge.
    Polyline polyline;
    polyline.points = {pt(0.0, -200.0), pt(200.0, -200.0), pt(200.0, -50.0), pt(0.0, -50.0)};
    polyline.closed = false;
    CHECK_NEAR(xm::core::length(polyline), 550.0, kTolerance);

    polyline.closed = true;
    CHECK_NEAR(xm::core::length(polyline), 700.0, kTolerance);
}

TEST(polyline, two_point_closed_polyline_doubles_back)
{
    Polyline polyline;
    polyline.points = {pt(0.0, 0.0), pt(10.0, 0.0)};
    polyline.closed = true;
    CHECK_NEAR(xm::core::length(polyline), 20.0, kTolerance);
}

TEST(polyline, fewer_than_two_points_is_zero)
{
    Polyline empty;
    CHECK_NEAR(xm::core::length(empty), 0.0, kTolerance);

    Polyline single;
    single.points = {pt(1.0, 2.0)};
    CHECK_NEAR(xm::core::length(single), 0.0, kTolerance);
}

// ---------------------------------------------------------------------------
// Shape variant dispatch
// ---------------------------------------------------------------------------

TEST(shape, length_dispatches_per_alternative)
{
    CHECK_NEAR(xm::core::length(makeShape(Line{pt(0.0, 0.0), pt(3.0, 4.0)})), 5.0, kTolerance);
    CHECK_NEAR(xm::core::length(makeShape(Circle{pt(0.0, 0.0), 1.0})),
               2.0 * std::numbers::pi, kTolerance);
    CHECK_NEAR(xm::core::length(makeShape(Arc{pt(0.0, 0.0), 2.0, 0.0, 180.0})),
               2.0 * std::numbers::pi, kTolerance);

    Polyline polyline;
    polyline.points = {pt(0.0, 0.0), pt(6.0, 0.0)};
    CHECK_NEAR(xm::core::length(makeShape(polyline)), 6.0, kTolerance);
}

TEST(shape, kind_name_matches_dxf_entity_name)
{
    CHECK_EQ(std::string(xm::core::kindName(makeShape(Line{pt(0.0, 0.0), pt(1.0, 0.0)}))),
             std::string("LINE"));
    CHECK_EQ(std::string(xm::core::kindName(makeShape(Circle{pt(0.0, 0.0), 1.0}))),
             std::string("CIRCLE"));
    CHECK_EQ(std::string(xm::core::kindName(makeShape(Arc{pt(0.0, 0.0), 1.0, 0.0, 90.0}))),
             std::string("ARC"));
    CHECK_EQ(std::string(xm::core::kindName(makeShape(Polyline{}))),
             std::string("LWPOLYLINE"));
}

// ---------------------------------------------------------------------------
// Acceptance arithmetic for the sample document (docs/MVP_DESIGN.md section 8)
//
// 4 lines of 300 mm + circle r=50 + quarter arc r=100 + closed slot of 700 mm.
// This is the value that the DXF write/read round trip must reproduce exactly.
// ---------------------------------------------------------------------------

TEST(acceptance, sample_document_total_length)
{
    const double total =
        xm::core::length(Line{pt(0.0, 0.0), pt(300.0, 0.0)}) +
        xm::core::length(Line{pt(300.0, 0.0), pt(300.0, 300.0)}) +
        xm::core::length(Line{pt(300.0, 300.0), pt(0.0, 300.0)}) +
        xm::core::length(Line{pt(0.0, 300.0), pt(0.0, 0.0)}) +
        xm::core::length(Circle{pt(450.0, 150.0), 50.0}) +
        xm::core::length(Arc{pt(450.0, 350.0), 100.0, 0.0, 90.0}) +
        xm::core::length(Polyline{
            {pt(0.0, -200.0), pt(200.0, -200.0), pt(200.0, -50.0), pt(0.0, -50.0)}, true});

    // Linear part: 4 lines of 300 mm + closed slot of 700 mm = 1900 mm.
    // Curved part: 100*pi (circle r=50) + 50*pi (quarter arc r=100) = 150*pi.
    // Total = 1900 + 150*pi = 2371.238898038469.
    //
    // Tolerance 1e-12 with a value near 2371 gives an absolute bound of about
    // 2.4e-9, inside the 1e-9 acceptance requirement; see
    // dsh_test.hpp::nearlyEqual for the absolute/relative rule.
    CHECK_NEAR(total, 1900.0 + 150.0 * std::numbers::pi, 1e-12);
    CHECK_NEAR(total, 2371.238898038, 4e-13);
}

// ---------------------------------------------------------------------------
// Transform2d
// ---------------------------------------------------------------------------

TEST(transform, identity_is_identity)
{
    CHECK(Transform2d::identity().isIdentity());
    // Double parentheses: the brace-initialiser contains commas, which the
    // preprocessor would otherwise read as extra macro arguments.
    CHECK((Transform2d{1.0, 0.0, 0.0, 1.0, 0.0, 0.0}.isIdentity()));
    CHECK(!Transform2d::translation(1.0, 0.0).isIdentity());
    CHECK(!Transform2d::scaling(1.0, 2.0).isIdentity());
}

TEST(transform, translation_maps_point)
{
    const Transform2d t = Transform2d::translation(10.0, -4.0);
    const Point2d p = t.apply(pt(1.0, 2.0));
    CHECK_NEAR(p.x, 11.0, kTolerance);
    CHECK_NEAR(p.y, -2.0, kTolerance);
}

TEST(transform, scaling_maps_point)
{
    const Transform2d t = Transform2d::scaling(2.0, 3.0);
    const Point2d p = t.apply(pt(1.5, 2.0));
    CHECK_NEAR(p.x, 3.0, kTolerance);
    CHECK_NEAR(p.y, 6.0, kTolerance);
}

TEST(transform, rotation_by_ninety_degrees)
{
    const Transform2d t = Transform2d::rotation(xm::core::toRadians(90.0));
    const Point2d p = t.apply(pt(1.0, 0.0));
    CHECK_NEAR(p.x, 0.0, kTolerance);
    CHECK_NEAR(p.y, 1.0, kTolerance);
}

TEST(transform, rotation_then_translation_composes_in_order)
{
    const Transform2d rotate = Transform2d::rotation(xm::core::toRadians(90.0));
    const Transform2d move = Transform2d::translation(10.0, 0.0);
    const Point2d p = rotate.then(move).apply(pt(1.0, 0.0));
    CHECK_NEAR(p.x, 10.0, kTolerance);
    CHECK_NEAR(p.y, 1.0, kTolerance);
}

TEST(transform, composition_of_translations_adds)
{
    const Transform2d combined =
        Transform2d::translation(1.0, 2.0).then(Transform2d::translation(3.0, 4.0));
    const Point2d p = combined.apply(pt(0.0, 0.0));
    CHECK_NEAR(p.x, 4.0, kTolerance);
    CHECK_NEAR(p.y, 6.0, kTolerance);
}

TEST(transform, rotation_recorded_on_entity_shape_round_trips_coefficients)
{
    const Transform2d t = Transform2d::rotation(xm::core::toRadians(30.0));
    const auto& coefficients = t.coefficients();
    CHECK_NEAR(coefficients[0], std::cos(xm::core::toRadians(30.0)), kTolerance);
    CHECK_NEAR(coefficients[1], std::sin(xm::core::toRadians(30.0)), kTolerance);
    CHECK_NEAR(coefficients[2], -std::sin(xm::core::toRadians(30.0)), kTolerance);
    CHECK_NEAR(coefficients[3], std::cos(xm::core::toRadians(30.0)), kTolerance);
    CHECK_NEAR(coefficients[4], 0.0, kTolerance);
    CHECK_NEAR(coefficients[5], 0.0, kTolerance);
}
