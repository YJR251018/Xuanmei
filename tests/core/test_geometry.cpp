// Unit tests for the core geometry value types and closed-form lengths.
//
// Every expected number is either an exact geometric identity or the value
// computed in docs/MVP_DESIGN.md section 8. The sample document total
// 1900 + 150*pi is asserted here as the arithmetic backbone of acceptance
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

// Counting probe for the assertion macros: proves each argument is evaluated
// exactly once, in the caller's scope.
int& evaluationCount()
{
    static int count = 0;
    return count;
}

double countedValue(double value)
{
    ++evaluationCount();
    return value;
}

// Records the order in which assertion operands are evaluated, then yields the
// value the assertion should compare.
double recordedValue(std::vector<int>& order, int tag, double value)
{
    order.push_back(tag);
    return value;
}

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

TEST(arc, sub_trillionth_degree_sweep_remains_nonzero)
{
    // A previous epsilon check treated every sweep <= 1e-12 degrees as a
    // degenerate arc. Only an exactly zero sweep is degenerate by contract.
    constexpr double sweepDegrees = 5e-13;
    const Arc arc{pt(0.0, 0.0), 100.0, 0.0, sweepDegrees};
    const double expected = xm::core::toRadians(xm::core::arcSweepDegrees(arc)) * arc.radius;

    CHECK(xm::core::length(arc) > 0.0);
    CHECK_NEAR_ABS(xm::core::length(arc), expected, 0.0);
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
    // Acceptance item A3 requires an ABSOLUTE error bound of 1e-9 mm, so these
    // use CHECK_NEAR_ABS. CHECK_NEAR would scale the bound by the magnitude
    // (about 2371 here) and could therefore pass a total that is wrong by
    // several nanometres. The closed-form sum is exact to within a few ulp of
    // 2371 (about 4.5e-13), which stays comfortably inside the bound.
    CHECK_NEAR_ABS(total, 1900.0 + 150.0 * std::numbers::pi, 1e-9);
    CHECK_NEAR_ABS(total, 2371.238898038, 1e-9);
}

// ---------------------------------------------------------------------------
// Test framework: absolute vs magnitude-scaled error bounds
//
// These exercise the underlying comparison functions directly. They are the
// regression guard for the defect where the only available assertion scaled its
// bound by the magnitude, so that an absolute requirement of 1e-9 mm could not
// actually be verified near a value of 2371.
// ---------------------------------------------------------------------------

TEST(comparison, absolute_error_within_enforces_the_raw_bound)
{
    // Exact match, and a tiny error well inside the bound.
    CHECK(::dsh_test::absoluteErrorWithin(1.0, 1.0, 1e-9));
    CHECK(::dsh_test::absoluteErrorWithin(1.0, 1.0 + 1e-12, 1e-9));
    CHECK(::dsh_test::absoluteErrorWithin(0.0, 0.0, 0.0));
    CHECK(::dsh_test::absoluteErrorWithin(1900.0, 1900.0 + 4e-13, 1e-9));
}

// The regression: an error of 1e-8 mm is exactly ten times the 1e-9 mm bound
// and must be rejected, even though the magnitude-scaled comparison accepts it.
TEST(comparison, absolute_error_within_rejects_error_over_the_bound)
{
    CHECK(!::dsh_test::absoluteErrorWithin(2371.238898038, 2371.238898038 + 1e-8, 1e-9));
    CHECK(!::dsh_test::absoluteErrorWithin(0.0, 1.0, 1e-9));
    CHECK(!::dsh_test::absoluteErrorWithin(1.0, 1.0 + 1.001e-9, 1e-9));
}

// Demonstrates why the magnitude-scaled comparison cannot serve as an absolute
// check at this magnitude: it accepts an error of 1e-8 that the 1e-9 mm
// requirement forbids. If nearlyEqual ever stops scaling, this test fails and
// the distinction documented in dsh_test.hpp needs revisiting.
TEST(comparison, magnitude_scaled_comparison_is_unsuitable_as_an_absolute_check)
{
    const double actual = 2371.238898038;
    const double expected = 2371.238898038 + 1e-8;

    CHECK(::dsh_test::nearlyEqual(actual, expected, 1e-9));
    CHECK(!::dsh_test::absoluteErrorWithin(actual, expected, 1e-9));
}

TEST(comparison, absolute_error_within_rejects_non_finite_operands_and_bounds)
{
    const double nan_value = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    CHECK(!::dsh_test::absoluteErrorWithin(nan_value, nan_value, 1e-9));
    CHECK(!::dsh_test::absoluteErrorWithin(nan_value, 1.0, 1e-9));
    CHECK(!::dsh_test::absoluteErrorWithin(1.0, nan_value, 1e-9));
    CHECK(!::dsh_test::absoluteErrorWithin(infinity, infinity, 1e-9));
    CHECK(!::dsh_test::absoluteErrorWithin(1.0, infinity, 1e-9));

    // Non-finite bounds are rejected rather than treated as "always within
    // range", matching Transform2d::isIdentity, so a meaningless bound is
    // reported instead of silently passing every comparison.
    CHECK(!::dsh_test::absoluteErrorWithin(5.0, 5.0, infinity));
    CHECK(!::dsh_test::absoluteErrorWithin(1.0, 1.0, -1.0));
    CHECK(!::dsh_test::absoluteErrorWithin(1.0, 1.0, nan_value));
}

// Every CHECK_* operand -- including the tolerance argument of the two
// three-operand macros -- must be evaluated exactly once.
//
// This does NOT reproduce the old defect: the old macro form also evaluated
// each operand exactly once (measured with a standalone probe). It guards the
// property against a future rewrite of the macros, one that expands an operand
// more than once, so that such a rewrite fails here instead of silently running
// a test expression repeatedly.
TEST(comparison, check_macros_evaluate_each_argument_exactly_once)
{
    evaluationCount() = 0;
    CHECK_NEAR_ABS(countedValue(1.0), countedValue(1.0), countedValue(1e-9));
    CHECK_EQ(evaluationCount(), 3);

    evaluationCount() = 0;
    CHECK_NEAR(countedValue(1.0), countedValue(1.0), countedValue(1e-9));
    CHECK_EQ(evaluationCount(), 3);

    evaluationCount() = 0;
    CHECK_EQ(countedValue(1.0), countedValue(1.0));
    CHECK_EQ(evaluationCount(), 2);
}

// The macros must evaluate their operands left to right and snapshot each one,
// which is what the original macro form did with separate statements. Handing
// the operands to a helper as ordinary function arguments would leave the order
// unspecified instead, and this test is the guard for that: measured on MSVC,
// the plain-argument form evaluates right to left at BOTH /Od and /O2, while
// the closure form used here is left to right at both, so replacing the
// closures with plain arguments makes the assertions below fail.
TEST(comparison, check_macros_evaluate_operands_left_to_right)
{
    std::vector<int> order;

    CHECK_EQ(recordedValue(order, 1, 1.0), recordedValue(order, 2, 1.0));
    const std::vector<int> equal_order{1, 2};
    CHECK(order == equal_order);

    order.clear();
    CHECK_NEAR(recordedValue(order, 3, 1.0), recordedValue(order, 4, 1.0),
               recordedValue(order, 5, 1e-9));
    const std::vector<int> near_order{3, 4, 5};
    CHECK(order == near_order);

    order.clear();
    CHECK_NEAR_ABS(recordedValue(order, 6, 1.0), recordedValue(order, 7, 1.0),
                   recordedValue(order, 8, 1e-9));
    const std::vector<int> near_abs_order{6, 7, 8};
    CHECK(order == near_abs_order);
}

// Regression guard for that shadowing defect, stated as ordinary correct code:
// with the old macro form these three arguments referred to the macro's own
// just-declared, still-uninitialised temporaries (undefined behaviour) instead
// of to the variables below.
//
// Measured with a standalone probe compiled at the project's /W4: the old form
// reported C4456 (declaration hides previous local) and C4700 (uninitialised
// local used), which /WX turns into a build failure, and at run time it
// compared against stack garbage -- the tolerance read back as 6.95e-310.
TEST(comparison, check_macros_read_caller_variables_named_like_old_temporaries)
{
    const double dsh_actual = 2.0;
    const double dsh_expected = 2.0;
    const double dsh_tolerance = 1e-9;

    CHECK_NEAR_ABS(dsh_actual, dsh_expected, dsh_tolerance);
    CHECK_NEAR(dsh_actual, dsh_expected, dsh_tolerance);
    CHECK_EQ(dsh_actual, dsh_expected);
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

TEST(transform, identity_default_tolerance_accepts_small_deviation)
{
    CHECK((Transform2d{1.0 + 1e-13, 0.0, 0.0, 1.0, 0.0, 0.0}.isIdentity()));

    // Outside the tolerance: all six coefficients -- linear terms and
    // translation components alike -- are compared with the same ABSOLUTE
    // difference of 1e-12. Nothing here is scaled by magnitude.
    CHECK(!(Transform2d{1.0 + 1e-9, 0.0, 0.0, 1.0, 0.0, 0.0}.isIdentity()));
    CHECK(!(Transform2d{1.0, 0.0, 0.0, 1.0, 1e-9, 0.0}.isIdentity()));

    // A zero tolerance accepts only the exact identity.
    CHECK((Transform2d{1.0, 0.0, 0.0, 1.0, 0.0, 0.0}.isIdentity(0.0)));
    CHECK(!(Transform2d{1.0 + 1e-15, 0.0, 0.0, 1.0, 0.0, 0.0}.isIdentity(0.0)));
}

// Every coefficient position is covered, because the original defect was that
// a non-finite entry skipped the comparison and fell through to "identity".
//
// Note: every operand below is wrapped in double parentheses. A Transform2d
// brace-initialiser contains commas, which the preprocessor would otherwise
// read as extra CHECK arguments.
TEST(transform, identity_rejects_nan_coefficients)
{
    const double nan_value = std::numeric_limits<double>::quiet_NaN();
    CHECK(!(Transform2d{nan_value, 0.0, 0.0, 1.0, 0.0, 0.0}.isIdentity()));
    CHECK(!(Transform2d{1.0, nan_value, 0.0, 1.0, 0.0, 0.0}.isIdentity()));
    CHECK(!(Transform2d{1.0, 0.0, nan_value, 1.0, 0.0, 0.0}.isIdentity()));
    CHECK(!(Transform2d{1.0, 0.0, 0.0, nan_value, 0.0, 0.0}.isIdentity()));
    CHECK(!(Transform2d{1.0, 0.0, 0.0, 1.0, nan_value, 0.0}.isIdentity()));
    CHECK(!(Transform2d{1.0, 0.0, 0.0, 1.0, 0.0, nan_value}.isIdentity()));
}

TEST(transform, identity_rejects_infinite_coefficients)
{
    const double infinity = std::numeric_limits<double>::infinity();
    CHECK(!(Transform2d{infinity, 0.0, 0.0, 1.0, 0.0, 0.0}.isIdentity()));
    CHECK(!(Transform2d{-infinity, 0.0, 0.0, 1.0, 0.0, 0.0}.isIdentity()));
    CHECK(!(Transform2d{1.0, infinity, 0.0, 1.0, 0.0, 0.0}.isIdentity()));
    CHECK(!(Transform2d{1.0, 0.0, infinity, 1.0, 0.0, 0.0}.isIdentity()));
    CHECK(!(Transform2d{1.0, 0.0, 0.0, infinity, 0.0, 0.0}.isIdentity()));
    CHECK(!(Transform2d{1.0, 0.0, 0.0, 1.0, infinity, 0.0}.isIdentity()));
    CHECK(!(Transform2d{1.0, 0.0, 0.0, 1.0, 0.0, -infinity}.isIdentity()));
}

TEST(transform, identity_rejects_invalid_tolerance)
{
    const double nan_value = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    // The argument is rejected, not the matrix: an exactly identity matrix must
    // not be reported as the identity under a meaningless tolerance.
    CHECK(!Transform2d::identity().isIdentity(-1.0));
    CHECK(!Transform2d::identity().isIdentity(nan_value));
    CHECK(!Transform2d::identity().isIdentity(infinity));
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
