// Unit tests for GeometryDocument, Layer and the built-in sample document.
//
// The sample document's parameters are normative (docs/MVP_DESIGN.md section
// 8), so these tests assert them exactly rather than approximately where the
// value is exactly representable. The acceptance figure A3, total length
// 1900 + 150*pi mm, is asserted here using the existing closed-form length()
// with an ABSOLUTE error bound of 1e-9 (internal tier, section 7.1).

#include <numbers>
#include <string>
#include <variant>
#include <vector>

#include "core/document.hpp"
#include "core/entity.hpp"
#include "core/sample_document.hpp"
#include "dsh_test.hpp"

using xm::core::Arc;
using xm::core::Circle;
using xm::core::GeometryDocument;
using xm::core::GeometryEntity;
using xm::core::Layer;
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

// The shape helpers fall back to the first alternative (default-constructed)
// rather than using std::get, so a mismatched Shape fails this test through the
// normal assertion instead of throwing std::bad_variant_access out of the whole
// run. Each use below asserts the accessor's expected kind first.
const Line& asLine(const Shape& shape)
{
    static const Line fallback{};
    const Line* line = std::get_if<Line>(&shape);
    return line != nullptr ? *line : fallback;
}

const Circle& asCircle(const Shape& shape)
{
    static const Circle fallback{};
    const Circle* circle = std::get_if<Circle>(&shape);
    return circle != nullptr ? *circle : fallback;
}

const Arc& asArc(const Shape& shape)
{
    static const Arc fallback{};
    const Arc* arc = std::get_if<Arc>(&shape);
    return arc != nullptr ? *arc : fallback;
}

const Polyline& asPolyline(const Shape& shape)
{
    static const Polyline fallback{};
    const Polyline* polyline = std::get_if<Polyline>(&shape);
    return polyline != nullptr ? *polyline : fallback;
}

bool holdsKind(const Shape& shape, const char* expected)
{
    return std::string(xm::core::kindName(shape)) == std::string(expected);
}

double totalLength(const GeometryDocument& document)
{
    double total = 0.0;
    for (const GeometryEntity& entity : document.entities()) {
        total += xm::core::length(entity.shape);
    }
    return total;
}

}  // namespace

// ---------------------------------------------------------------------------
// GeometryDocument: identity allocation and defaults
// ---------------------------------------------------------------------------

TEST(document, new_document_is_empty_with_millimetre_unit)
{
    const GeometryDocument document;

    CHECK(document.entities().empty());
    CHECK(document.layers().empty());
    CHECK(document.warnings().empty());
    CHECK_EQ(document.unit(), std::string("mm"));
    CHECK_EQ(std::string(GeometryDocument::kUnitMillimeter), std::string("mm"));
}

TEST(document, ids_start_at_one_and_are_contiguous)
{
    GeometryDocument document;

    const std::uint64_t first = document.add(Line{pt(0.0, 0.0), pt(1.0, 0.0)});
    const std::uint64_t second = document.add(Circle{pt(0.0, 0.0), 1.0});
    const std::uint64_t third = document.add(Arc{pt(0.0, 0.0), 1.0, 0.0, 90.0});
    const std::uint64_t fourth = document.add(Polyline{});

    CHECK_EQ(first, std::uint64_t{1});
    CHECK_EQ(second, std::uint64_t{2});
    CHECK_EQ(third, std::uint64_t{3});
    CHECK_EQ(fourth, std::uint64_t{4});

    const std::vector<GeometryEntity>& entities = document.entities();
    CHECK_EQ(entities.size(), std::size_t{4});
    for (std::size_t i = 0; i < entities.size(); ++i) {
        CHECK_EQ(entities[i].id, i + 1);
    }
}

TEST(document, add_applies_documented_defaults)
{
    GeometryDocument document;
    document.add(Line{pt(0.0, 0.0), pt(1.0, 0.0)});

    const GeometryEntity& entity = document.entities().front();
    CHECK_EQ(entity.layer, std::string("0"));
    CHECK(entity.transform.isIdentity());
    CHECK(entity.warnings.empty());
}

TEST(document, add_returns_the_id_stored_on_the_entity)
{
    GeometryDocument document;
    const std::uint64_t returned = document.add(Circle{pt(2.0, 3.0), 4.0}, "HOLES");
    CHECK_EQ(returned, std::uint64_t{1});
    CHECK_EQ(document.entities().front().id, returned);
}

TEST(document, add_preserves_explicit_layer_and_transform)
{
    GeometryDocument document;
    const Transform2d move = Transform2d::translation(10.0, 20.0);
    document.add(Circle{pt(0.0, 0.0), 5.0}, "HOLES", move);

    const GeometryEntity& entity = document.entities().front();
    CHECK_EQ(entity.layer, std::string("HOLES"));
    CHECK(!entity.transform.isIdentity());

    const Point2d moved = entity.transform.apply(pt(1.0, 2.0));
    CHECK_NEAR(moved.x, 11.0, kTolerance);
    CHECK_NEAR(moved.y, 22.0, kTolerance);
}

TEST(document, add_preserves_explicit_entity_warnings)
{
    GeometryDocument document;
    document.add(Line{pt(0.0, 0.0), pt(1.0, 0.0)}, "CUT",
                 Transform2d::identity(), {"first", "second"});

    const GeometryEntity& entity = document.entities().front();
    CHECK_EQ(entity.warnings.size(), std::size_t{2});
    CHECK_EQ(entity.warnings[0], std::string("first"));
    CHECK_EQ(entity.warnings[1], std::string("second"));
}

TEST(document, add_does_not_disturb_earlier_entities)
{
    GeometryDocument document;
    document.add(Line{pt(0.0, 0.0), pt(300.0, 0.0)}, "OUTLINE");
    document.add(Circle{pt(450.0, 150.0), 50.0}, "HOLES");

    const GeometryEntity& first = document.entities().front();
    CHECK_EQ(first.id, std::uint64_t{1});
    CHECK_EQ(first.layer, std::string("OUTLINE"));
    CHECK_EQ(std::string(xm::core::kindName(first.shape)), std::string("LINE"));
    CHECK_NEAR(xm::core::length(first.shape), 300.0, kTolerance);
}

TEST(document, add_layer_appends_in_order)
{
    GeometryDocument document;
    document.addLayer(Layer{"OUTLINE", 7, true});
    document.addLayer(Layer{"HOLES", 3, false});

    const std::vector<Layer>& layers = document.layers();
    CHECK_EQ(layers.size(), std::size_t{2});
    CHECK_EQ(layers[0].name, std::string("OUTLINE"));
    CHECK_EQ(layers[0].colorIndex, 7);
    CHECK(layers[0].visible);
    CHECK_EQ(layers[1].name, std::string("HOLES"));
    CHECK_EQ(layers[1].colorIndex, 3);
    CHECK(!layers[1].visible);
}

TEST(layer, defaults_are_colour_seven_and_visible)
{
    const Layer layer;
    CHECK(layer.name.empty());
    CHECK_EQ(layer.colorIndex, 7);
    CHECK(layer.visible);
}

TEST(document, warnings_are_collected_in_order)
{
    GeometryDocument document;
    CHECK(document.warnings().empty());

    document.addWarning("第一个警告");
    document.addWarning("second warning");

    const std::vector<std::string>& warnings = document.warnings();
    CHECK_EQ(warnings.size(), std::size_t{2});
    CHECK_EQ(warnings[0], std::string("第一个警告"));
    CHECK_EQ(warnings[1], std::string("second warning"));
}

TEST(document, unit_is_millimetre_by_default_and_can_be_replaced)
{
    GeometryDocument document;
    CHECK_EQ(document.unit(), std::string("mm"));

    document.setUnit("inch");
    CHECK_EQ(document.unit(), std::string("inch"));

    document.setUnit("mm");
    CHECK_EQ(document.unit(), std::string("mm"));
}

// ---------------------------------------------------------------------------
// Sample document (docs/MVP_DESIGN.md section 8)
// ---------------------------------------------------------------------------

TEST(sample_document, has_exactly_three_layers_in_order)
{
    const GeometryDocument document = xm::core::makeSampleDocument();

    const std::vector<Layer>& layers = document.layers();
    CHECK_EQ(layers.size(), std::size_t{3});
    CHECK_EQ(layers[0].name, std::string("OUTLINE"));
    CHECK_EQ(layers[1].name, std::string("HOLES"));
    CHECK_EQ(layers[2].name, std::string("SLOT"));
    CHECK(document.warnings().empty());
    CHECK_EQ(document.unit(), std::string("mm"));
}

TEST(sample_document, has_exactly_seven_entities_with_contiguous_ids)
{
    const GeometryDocument document = xm::core::makeSampleDocument();

    const std::vector<GeometryEntity>& entities = document.entities();
    CHECK_EQ(entities.size(), std::size_t{7});
    for (std::size_t i = 0; i < entities.size(); ++i) {
        CHECK_EQ(entities[i].id, static_cast<std::uint64_t>(i) + 1);
    }
}

TEST(sample_document, entity_kinds_and_layers_match_the_specification)
{
    const GeometryDocument document = xm::core::makeSampleDocument();
    const std::vector<GeometryEntity>& entities = document.entities();

    const char* expectedKinds[7] = {
        "LINE", "LINE", "LINE", "LINE", "CIRCLE", "ARC", "LWPOLYLINE"};
    const char* expectedLayers[7] = {
        "OUTLINE", "OUTLINE", "OUTLINE", "OUTLINE", "HOLES", "HOLES", "SLOT"};

    for (std::size_t i = 0; i < entities.size(); ++i) {
        CHECK_EQ(std::string(xm::core::kindName(entities[i].shape)),
                 std::string(expectedKinds[i]));
        CHECK_EQ(entities[i].layer, std::string(expectedLayers[i]));
    }
}

// Entities 1-4 form the 300 mm square, in order, each 300 mm long.
TEST(sample_document, entities_one_to_four_are_the_square)
{
    const GeometryDocument document = xm::core::makeSampleDocument();
    const std::vector<GeometryEntity>& entities = document.entities();

    const Point2d expected[4][2] = {
        {pt(0.0, 0.0), pt(300.0, 0.0)},
        {pt(300.0, 0.0), pt(300.0, 300.0)},
        {pt(300.0, 300.0), pt(0.0, 300.0)},
        {pt(0.0, 300.0), pt(0.0, 0.0)},
    };

    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(holdsKind(entities[i].shape, "LINE"));
        const Line& line = asLine(entities[i].shape);
        CHECK(line.from == expected[i][0]);
        CHECK(line.to == expected[i][1]);
        CHECK_NEAR(xm::core::length(line), 300.0, kTolerance);
    }
}

TEST(sample_document, entity_five_is_the_circle)
{
    const GeometryDocument document = xm::core::makeSampleDocument();
    CHECK(holdsKind(document.entities()[4].shape, "CIRCLE"));
    const Circle& circle = asCircle(document.entities()[4].shape);

    CHECK(circle.center == pt(450.0, 150.0));
    CHECK_NEAR(circle.radius, 50.0, kTolerance);
    CHECK_NEAR(xm::core::length(circle), 100.0 * std::numbers::pi, kTolerance);
}

TEST(sample_document, entity_six_is_the_quarter_arc)
{
    const GeometryDocument document = xm::core::makeSampleDocument();
    CHECK(holdsKind(document.entities()[5].shape, "ARC"));
    const Arc& arc = asArc(document.entities()[5].shape);

    CHECK(arc.center == pt(450.0, 350.0));
    CHECK_NEAR(arc.radius, 100.0, kTolerance);
    CHECK_NEAR(arc.startDegrees, 0.0, kTolerance);
    CHECK_NEAR(arc.endDegrees, 90.0, kTolerance);
    CHECK_NEAR(xm::core::arcSweepDegrees(arc), 90.0, kTolerance);
    CHECK_NEAR(xm::core::length(arc), 50.0 * std::numbers::pi, kTolerance);
}

TEST(sample_document, entity_seven_is_the_closed_slot)
{
    const GeometryDocument document = xm::core::makeSampleDocument();
    CHECK(holdsKind(document.entities()[6].shape, "LWPOLYLINE"));
    const Polyline& slot = asPolyline(document.entities()[6].shape);

    CHECK(slot.closed);
    CHECK_EQ(slot.points.size(), std::size_t{4});
    CHECK(slot.points[0] == pt(0.0, -200.0));
    CHECK(slot.points[1] == pt(200.0, -200.0));
    CHECK(slot.points[2] == pt(200.0, -50.0));
    CHECK(slot.points[3] == pt(0.0, -50.0));
    CHECK_NEAR(xm::core::length(slot), 700.0, kTolerance);
}

TEST(sample_document, all_entities_use_the_identity_transform_and_are_warning_free)
{
    const GeometryDocument document = xm::core::makeSampleDocument();

    for (const GeometryEntity& entity : document.entities()) {
        CHECK(entity.transform.isIdentity());
        CHECK(entity.warnings.empty());
    }
}

// Acceptance A3: the sample document's total length is 1900 + 150*pi mm
// (= 2371.238898038469), computed with the closed-form length() and asserted
// with an ABSOLUTE error bound of 1e-9 (internal tier, docs section 7.1).
TEST(sample_document, total_length_is_the_acceptance_figure)
{
    const GeometryDocument document = xm::core::makeSampleDocument();

    const double total = totalLength(document);
    CHECK_NEAR_ABS(total, 1900.0 + 150.0 * std::numbers::pi, 1e-9);
    CHECK_NEAR_ABS(total, 2371.238898038, 1e-9);
}

// The per-entity breakdown is asserted separately so that a future regression
// reports which entity drifted, not just that the total is wrong.
TEST(sample_document, per_entity_lengths_sum_to_the_acceptance_figure)
{
    const GeometryDocument document = xm::core::makeSampleDocument();
    const std::vector<GeometryEntity>& entities = document.entities();

    const double expected[7] = {
        300.0, 300.0, 300.0, 300.0,
        100.0 * std::numbers::pi,
        50.0 * std::numbers::pi,
        700.0,
    };

    double sum = 0.0;
    for (std::size_t i = 0; i < entities.size(); ++i) {
        const double actual = xm::core::length(entities[i].shape);
        CHECK_NEAR_ABS(actual, expected[i], 1e-9);
        sum += actual;
    }
    CHECK_NEAR_ABS(sum, 1900.0 + 150.0 * std::numbers::pi, 1e-9);
}

// makeSampleDocument() must be deterministic and must not hand out the same
// state twice; two independent calls produce independent, equal documents.
TEST(sample_document, builds_fresh_independent_documents)
{
    const GeometryDocument first = xm::core::makeSampleDocument();
    GeometryDocument second = xm::core::makeSampleDocument();

    CHECK_EQ(first.entities().size(), second.entities().size());
    // Deterministic construction: identical geometry, so the totals are equal
    // bit for bit and a zero absolute bound is the right assertion.
    CHECK_NEAR_ABS(totalLength(first), totalLength(second), 0.0);

    // Mutating one document must not affect the other.
    second.add(Line{pt(0.0, 0.0), pt(1.0, 0.0)}, "OUTLINE");
    CHECK_EQ(first.entities().size(), std::size_t{7});
    CHECK_EQ(second.entities().size(), std::size_t{8});
    CHECK_EQ(second.entities().back().id, std::uint64_t{8});
}
