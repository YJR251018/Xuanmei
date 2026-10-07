// Contract tests for the dependency-free ASCII DXF writer.

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "core/dxf_writer.hpp"
#include "dsh_test.hpp"

using xm::core::Arc;
using xm::core::Circle;
using xm::core::GeometryDocument;
using xm::core::Layer;
using xm::core::Line;
using xm::core::MinimalDxfWriter;
using xm::core::Point2d;
using xm::core::Polyline;
using xm::core::Transform2d;
using xm::core::writeDxfFile;

namespace {

std::string writeDxf(const GeometryDocument& document)
{
    std::ostringstream output;
    MinimalDxfWriter{}.write(document, output);
    return output.str();
}

std::vector<std::string> linesOf(const std::string& text)
{
    std::istringstream input(text);
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);) {
        lines.push_back(std::move(line));
    }
    return lines;
}

template <typename Exception, typename Function>
bool throwsContaining(Function&& function, std::string_view fragment)
{
    try {
        std::forward<Function>(function)();
    } catch (const Exception& error) {
        return std::string_view(error.what()).find(fragment) != std::string_view::npos;
    } catch (...) {
        return false;
    }
    return false;
}

bool contains(const std::string& text, std::string_view fragment)
{
    return text.find(fragment) != std::string::npos;
}

std::string readBinaryFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>{input},
                       std::istreambuf_iterator<char>{}};
}

}  // namespace

TEST(dxf_writer, empty_document_has_r12_sections_units_and_strict_pairs)
{
    const std::string text = writeDxf(GeometryDocument{});
    const std::vector<std::string> lines = linesOf(text);

    CHECK(text.starts_with("0\nSECTION\n2\nHEADER\n"));
    CHECK(text.ends_with("0\nENDSEC\n0\nEOF\n"));
    CHECK(!contains(text, "\r"));
    CHECK_EQ(lines.size() % std::size_t{2}, std::size_t{0});
    CHECK(contains(text, "9\n$ACADVER\n1\nAC1009\n"));
    CHECK(contains(text, "9\n$INSUNITS\n70\n4\n"));
    CHECK(contains(text, "9\n$MEASUREMENT\n70\n1\n"));
    CHECK(contains(text, "9\n$LUNITS\n70\n2\n"));
    CHECK(contains(text, "9\n$LUPREC\n70\n6\n"));
    CHECK(contains(text, "2\nTABLES\n"));
    CHECK(contains(text, "2\nENTITIES\n"));
}

TEST(dxf_writer, line_circle_arc_and_polyline_use_fixed_six_decimal_fields)
{
    GeometryDocument document;
    document.add(Line{Point2d{1.25, -2.0}, Point2d{3.0, 4.5}}, "CUT");
    document.add(Circle{Point2d{5.0, 6.0}, 2.5}, "HOLES");
    document.add(Arc{Point2d{7.0, 8.0}, 3.0, 10.0, 95.0}, "HOLES");
    document.add(Polyline{{Point2d{0.0, 0.0}, Point2d{2.0, 0.0}, Point2d{2.0, 1.0}},
                          true},
                 "OUTLINE");

    const std::string text = writeDxf(document);

    CHECK(contains(text, "0\nLINE\n8\nCUT\n"
                         "10\n1.250000\n20\n-2.000000\n30\n0.000000\n"
                         "11\n3.000000\n21\n4.500000\n31\n0.000000\n"));
    CHECK(contains(text, "0\nCIRCLE\n8\nHOLES\n"
                         "10\n5.000000\n20\n6.000000\n30\n0.000000\n40\n2.500000\n"));
    CHECK(contains(text, "0\nARC\n8\nHOLES\n"
                         "10\n7.000000\n20\n8.000000\n30\n0.000000\n40\n3.000000\n"
                         "50\n10.000000\n51\n95.000000\n"));
    CHECK(contains(text, "0\nPOLYLINE\n8\nOUTLINE\n66\n1\n70\n1\n"
                         "10\n0.000000\n20\n0.000000\n30\n0.000000\n"
                         "0\nVERTEX\n8\nOUTLINE\n10\n0.000000\n20\n0.000000\n30\n0.000000\n"
                         "0\nVERTEX\n8\nOUTLINE\n10\n2.000000\n20\n0.000000\n30\n0.000000\n"
                         "0\nVERTEX\n8\nOUTLINE\n10\n2.000000\n20\n1.000000\n30\n0.000000\n"
                         "0\nSEQEND\n8\nOUTLINE\n"));
}

TEST(dxf_writer, layer_table_contains_zero_declared_and_entity_only_layers_once)
{
    GeometryDocument document;
    document.addLayer(Layer{"CUT", 1, true});
    document.addLayer(Layer{"HIDDEN", 3, false});
    document.add(Line{Point2d{0.0, 0.0}, Point2d{1.0, 1.0}}, "ADHOC");

    const std::string text = writeDxf(document);

    CHECK(contains(text, "2\nLAYER\n70\n4\n"));
    CHECK(contains(text, "2\n0\n70\n0\n62\n7\n6\nCONTINUOUS\n"));
    CHECK(contains(text, "2\nCUT\n70\n0\n62\n1\n6\nCONTINUOUS\n"));
    CHECK(contains(text, "2\nHIDDEN\n70\n0\n62\n-3\n6\nCONTINUOUS\n"));
    CHECK(contains(text, "2\nADHOC\n70\n0\n62\n7\n6\nCONTINUOUS\n"));
}

TEST(dxf_writer, output_is_independent_of_callers_numeric_formatting)
{
    GeometryDocument document;
    document.add(Circle{Point2d{1.0, 2.0}, 3.0});
    std::ostringstream output;
    output.setf(std::ios::scientific, std::ios::floatfield);
    output.precision(2);

    MinimalDxfWriter{}.write(document, output);

    CHECK(contains(output.str(), "40\n3.000000\n"));
    CHECK(!contains(output.str(), "3.00e"));
}

TEST(dxf_writer, rejects_non_millimetre_documents)
{
    GeometryDocument document;
    document.setUnit("inch");
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(document)); }, "mm"));
}

TEST(dxf_writer, rejects_non_identity_transform_instead_of_silently_ignoring_it)
{
    GeometryDocument document;
    document.add(Line{Point2d{0.0, 0.0}, Point2d{1.0, 0.0}}, "0",
                 Transform2d::translation(2.0, 3.0));
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(document)); }, "实体 1"));
}

TEST(dxf_writer, rejects_non_finite_coordinates)
{
    GeometryDocument document;
    document.add(Line{Point2d{0.0, 0.0},
                      Point2d{std::numeric_limits<double>::infinity(), 1.0}});
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(document)); }, "实体 1"));
}

TEST(dxf_writer, rejects_non_positive_radius_and_zero_sweep_arc)
{
    GeometryDocument badCircle;
    badCircle.add(Circle{Point2d{0.0, 0.0}, 0.0});
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(badCircle)); }, "半径"));

    GeometryDocument badArc;
    badArc.add(Arc{Point2d{0.0, 0.0}, 1.0, 45.0, 45.0});
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(badArc)); }, "零扫角"));
}

TEST(dxf_writer, rejects_geometry_that_six_decimal_output_would_collapse)
{
    GeometryDocument tinyCircle;
    tinyCircle.add(Circle{Point2d{0.0, 0.0}, 1e-7});
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(tinyCircle)); }, "六位小数"));

    GeometryDocument tinyArc;
    tinyArc.add(Arc{Point2d{0.0, 0.0}, 1.0, 0.0, 1e-7});
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(tinyArc)); }, "六位小数"));

    GeometryDocument tinyArcRadius;
    tinyArcRadius.add(Arc{Point2d{0.0, 0.0}, 1e-7, 0.0, 90.0});
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(tinyArcRadius)); }, "六位小数"));

    GeometryDocument almostFullArc;
    almostFullArc.add(Arc{Point2d{0.0, 0.0}, 1.0, 0.0000004, 359.9999996});
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(almostFullArc)); }, "零扫角"));
}

TEST(dxf_writer, normalizes_arc_angles_to_the_dxf_zero_to_360_range)
{
    GeometryDocument document;
    document.add(Arc{Point2d{0.0, 0.0}, 2.0, -10.0, 10.0});

    const std::string text = writeDxf(document);

    CHECK(contains(text, "50\n350.000000\n51\n10.000000\n"));
}

TEST(dxf_writer, rejects_empty_polyline_and_layer_names_that_break_line_pairs)
{
    GeometryDocument emptyPolyline;
    emptyPolyline.add(Polyline{{}, false});
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(emptyPolyline)); }, "顶点"));

    GeometryDocument badLayer;
    badLayer.add(Line{Point2d{0.0, 0.0}, Point2d{1.0, 0.0}}, "CUT\nINJECTED");
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(badLayer)); }, "图层"));
}

TEST(dxf_writer, rejects_layer_names_outside_the_r12_portable_subset)
{
    GeometryDocument spacedLayer;
    spacedLayer.add(Line{Point2d{0.0, 0.0}, Point2d{1.0, 0.0}}, "CUT PATH");
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(spacedLayer)); }, "R12 图层名"));

    GeometryDocument longLayer;
    longLayer.add(Line{Point2d{0.0, 0.0}, Point2d{1.0, 0.0}},
                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ_12345");
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(longLayer)); }, "31"));
}

TEST(dxf_writer, rejects_duplicate_declared_layers)
{
    GeometryDocument document;
    document.addLayer(Layer{"CUT", 1, true});
    document.addLayer(Layer{"CUT", 2, true});
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { static_cast<void>(writeDxf(document)); }, "重复图层"));
}

TEST(dxf_writer, reports_destination_stream_failure)
{
    std::ostringstream output;
    output.setstate(std::ios::badbit);
    CHECK(throwsContaining<std::runtime_error>(
        [&] { MinimalDxfWriter{}.write(GeometryDocument{}, output); }, "输出流"));
}

TEST(dxf_writer, file_entry_point_writes_binary_lf_without_truncating_on_validation_error)
{
    const std::filesystem::path path =
        std::filesystem::current_path() / "xuanmei_dxf_writer_contract_test.dxf";
    std::error_code cleanupError;
    std::filesystem::remove(path, cleanupError);

    GeometryDocument valid;
    valid.add(Line{Point2d{0.0, 0.0}, Point2d{1.0, 1.0}});
    writeDxfFile(valid, path);
    const std::string text = readBinaryFile(path);
    CHECK(!text.empty());
    CHECK(!contains(text, "\r"));
    CHECK(text.ends_with("0\nEOF\n"));

    GeometryDocument invalid;
    invalid.setUnit("inch");
    CHECK(throwsContaining<std::invalid_argument>(
        [&] { writeDxfFile(invalid, path); }, "mm"));
    const std::string unchanged = readBinaryFile(path);
    CHECK_EQ(unchanged, text);

    std::filesystem::remove(path, cleanupError);
    CHECK(!cleanupError);
}
