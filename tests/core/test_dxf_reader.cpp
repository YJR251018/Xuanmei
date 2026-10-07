// Contract and round-trip tests for the tolerant ASCII DXF R12 reader.

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/dxf_reader.hpp"
#include "core/dxf_writer.hpp"
#include "core/entity.hpp"
#include "core/native_analyzer.hpp"
#include "core/sample_document.hpp"
#include "dsh_test.hpp"

using xm::core::DxfReadResult;
using xm::core::GeometryDocument;
using xm::core::Line;
using xm::core::MinimalDxfWriter;
using xm::core::Native2dAnalyzer;
using xm::core::Polyline;
using xm::core::readDxf;
using xm::core::readDxfFile;

namespace {

DxfReadResult readText(const std::string& text)
{
    std::istringstream input(text);
    return readDxf(input);
}

std::string writeText(const GeometryDocument& document)
{
    std::ostringstream output;
    MinimalDxfWriter{}.write(document, output);
    return output.str();
}

bool warningsContain(const DxfReadResult& result, std::string_view fragment)
{
    return std::any_of(result.warnings.begin(), result.warnings.end(),
                       [fragment](const std::string& warning) {
                           return std::string_view(warning).find(fragment)
                               != std::string_view::npos;
                       });
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

std::string entitiesDocument(std::string_view records, bool closeSection = true,
                             bool includeEof = true)
{
    std::string text = "0\nSECTION\n2\nENTITIES\n";
    text.append(records);
    if (closeSection) {
        text += "0\nENDSEC\n";
    }
    if (includeEof) {
        text += "0\nEOF\n";
    }
    return text;
}

}  // namespace

TEST(dxf_reader, writer_sample_round_trips_entities_layers_and_analysis_metrics)
{
    const GeometryDocument original = xm::core::makeSampleDocument();
    const DxfReadResult result = readText(writeText(original));

    CHECK(result.ok());
    if (!result.ok()) {
        return;
    }
    const GeometryDocument& restored = *result.document;
    CHECK(result.warnings.empty());
    CHECK_EQ(restored.entities().size(), original.entities().size());
    CHECK_EQ(restored.layers().size(), original.layers().size());
    for (std::size_t index = 0; index < original.layers().size(); ++index) {
        CHECK_EQ(restored.layers()[index].name, original.layers()[index].name);
        CHECK_EQ(restored.layers()[index].colorIndex,
                 original.layers()[index].colorIndex);
        CHECK_EQ(restored.layers()[index].visible,
                 original.layers()[index].visible);
    }
    for (std::size_t index = 0; index < original.entities().size(); ++index) {
        const auto& expected = original.entities()[index];
        const auto& actual = restored.entities()[index];
        CHECK_EQ(actual.id, expected.id);
        CHECK_EQ(actual.layer, expected.layer);
        CHECK_EQ(std::string(xm::core::kindName(actual.shape)),
                 std::string(xm::core::kindName(expected.shape)));
        CHECK_NEAR_ABS(xm::core::length(actual.shape),
                       xm::core::length(expected.shape), 1e-6);
    }

    const auto originalMetrics = Native2dAnalyzer{}.analyze(original);
    const auto restoredMetrics = Native2dAnalyzer{}.analyze(restored);
    CHECK_EQ(restoredMetrics.hasBounds, originalMetrics.hasBounds);
    CHECK_EQ(restoredMetrics.perEntity.size(), originalMetrics.perEntity.size());
    CHECK_NEAR_ABS(restoredMetrics.totalLength, originalMetrics.totalLength, 1e-6);
    CHECK_NEAR_ABS(restoredMetrics.minX, originalMetrics.minX, 1e-6);
    CHECK_NEAR_ABS(restoredMetrics.minY, originalMetrics.minY, 1e-6);
    CHECK_NEAR_ABS(restoredMetrics.maxX, originalMetrics.maxX, 1e-6);
    CHECK_NEAR_ABS(restoredMetrics.maxY, originalMetrics.maxY, 1e-6);
}

TEST(dxf_reader, accepts_crlf_from_external_ascii_dxf)
{
    std::string text = writeText(xm::core::makeSampleDocument());
    std::string crlf;
    crlf.reserve(text.size() * 2U);
    for (const char character : text) {
        crlf += (character == '\n') ? "\r\n" : std::string(1, character);
    }

    const DxfReadResult result = readText(crlf);

    CHECK(result.ok());
    CHECK(result.warnings.empty());
}

TEST(dxf_reader, empty_and_binary_inputs_are_fatal)
{
    const DxfReadResult empty = readText("");
    CHECK(!empty.ok());
    CHECK(warningsContain(empty, "文件为空"));

    const DxfReadResult binary = readText("AutoCAD Binary DXF\r\n\x1a\0payload");
    CHECK(!binary.ok());
    CHECK(warningsContain(binary, "仅支持 ASCII DXF"));
}

TEST(dxf_reader, malformed_group_code_and_dangling_code_are_fatal_with_line_numbers)
{
    const DxfReadResult malformed = readText("not-a-code\nSECTION\n");
    CHECK(!malformed.ok());
    CHECK(warningsContain(malformed, "第 1 行"));

    const DxfReadResult dangling = readText("0\nSECTION\n2\n");
    CHECK(!dangling.ok());
    CHECK(warningsContain(dangling, "第 3 行"));
}

TEST(dxf_reader, missing_endsec_and_eof_are_recoverable)
{
    const std::string line =
        "0\nLINE\n8\nCUT\n10\n0\n20\n0\n11\n3\n21\n4\n";
    const DxfReadResult result = readText(entitiesDocument(line, false, false));

    CHECK(result.ok());
    CHECK(warningsContain(result, "ENDSEC"));
    CHECK(warningsContain(result, "EOF"));
    if (result.ok()) {
        CHECK_EQ(result.document->entities().size(), std::size_t{1});
    }
}

TEST(dxf_reader, unknown_entities_are_skipped_and_summarized_by_type)
{
    const std::string records =
        "0\nTEXT\n8\n0\n1\nfirst\n"
        "0\nTEXT\n8\n0\n1\nsecond\n"
        "0\nSPLINE\n8\n0\n";
    const DxfReadResult result = readText(entitiesDocument(records));

    CHECK(result.ok());
    CHECK(warningsContain(result, "TEXT"));
    CHECK(warningsContain(result, "2"));
    CHECK(warningsContain(result, "SPLINE"));
    if (result.ok()) {
        CHECK(result.document->entities().empty());
    }
}

TEST(dxf_reader, missing_and_damaged_line_fields_default_to_zero_with_warnings)
{
    const std::string records =
        "0\nLINE\n8\nCUT\n10\n1.5\n20\nnot-a-number\n21\n4\n";
    const DxfReadResult result = readText(entitiesDocument(records));

    CHECK(result.ok());
    CHECK(warningsContain(result, "组码 20"));
    CHECK(warningsContain(result, "组码 11"));
    if (!result.ok() || result.document->entities().empty()) {
        return;
    }
    const Line& line = std::get<Line>(result.document->entities()[0].shape);
    CHECK_NEAR_ABS(line.from.x, 1.5, 0.0);
    CHECK_NEAR_ABS(line.from.y, 0.0, 0.0);
    CHECK_NEAR_ABS(line.to.x, 0.0, 0.0);
    CHECK_NEAR_ABS(line.to.y, 4.0, 0.0);
}

TEST(dxf_reader, legacy_polyline_sequence_restores_vertices_and_closed_flag)
{
    const std::string records =
        "0\nPOLYLINE\n8\nSLOT\n66\n1\n70\n1\n10\n0\n20\n0\n30\n0\n"
        "0\nVERTEX\n8\nSLOT\n10\n0\n20\n0\n30\n0\n"
        "0\nVERTEX\n8\nSLOT\n10\n5\n20\n0\n30\n0\n"
        "0\nVERTEX\n8\nSLOT\n10\n5\n20\n2\n30\n0\n"
        "0\nSEQEND\n8\nSLOT\n";
    const DxfReadResult result = readText(entitiesDocument(records));

    CHECK(result.ok());
    CHECK(result.warnings.empty());
    if (!result.ok() || result.document->entities().empty()) {
        return;
    }
    const Polyline& polyline = std::get<Polyline>(result.document->entities()[0].shape);
    CHECK(polyline.closed);
    CHECK_EQ(polyline.points.size(), std::size_t{3});
    CHECK_NEAR_ABS(polyline.points[2].x, 5.0, 0.0);
    CHECK_NEAR_ABS(polyline.points[2].y, 2.0, 0.0);
}

TEST(dxf_reader, missing_seqend_warns_without_swallowing_the_next_entity)
{
    const std::string records =
        "0\nPOLYLINE\n8\nSLOT\n66\n1\n70\n0\n"
        "0\nVERTEX\n8\nSLOT\n10\n1\n20\n2\n"
        "0\nLINE\n8\nCUT\n10\n0\n20\n0\n11\n1\n21\n0\n";
    const DxfReadResult result = readText(entitiesDocument(records));

    CHECK(result.ok());
    CHECK(warningsContain(result, "SEQEND"));
    if (result.ok()) {
        CHECK_EQ(result.document->entities().size(), std::size_t{2});
        CHECK_EQ(std::string(xm::core::kindName(result.document->entities()[1].shape)),
                 std::string("LINE"));
    }
}

TEST(dxf_reader, unsupported_polyline_flags_and_bulges_are_not_silently_flattened)
{
    const std::string records =
        "0\nPOLYLINE\n8\nMESH\n70\n64\n"
        "0\nVERTEX\n8\nMESH\n10\n1\n20\n2\n"
        "0\nSEQEND\n8\nMESH\n"
        "0\nPOLYLINE\n8\nCURVE\n70\n0\n"
        "0\nVERTEX\n8\nCURVE\n10\n0\n20\n0\n42\n1\n"
        "0\nVERTEX\n8\nCURVE\n10\n2\n20\n0\n"
        "0\nSEQEND\n8\nCURVE\n"
        "0\nLINE\n8\nCUT\n10\n0\n20\n0\n11\n1\n21\n0\n";
    const DxfReadResult result = readText(entitiesDocument(records));

    CHECK(result.ok());
    CHECK(warningsContain(result, "标志位 64"));
    CHECK(warningsContain(result, "bulge"));
    if (result.ok()) {
        CHECK_EQ(result.document->entities().size(), std::size_t{1});
        CHECK_EQ(std::string(xm::core::kindName(result.document->entities()[0].shape)),
                 std::string("LINE"));
    }
}

TEST(dxf_reader, layer_table_restores_color_visibility_and_skips_implicit_zero)
{
    const std::string text =
        "0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n70\n2\n"
        "0\nLAYER\n2\n0\n70\n0\n62\n7\n6\nCONTINUOUS\n"
        "0\nLAYER\n2\nCUT\n70\n0\n62\n-3\n6\nCONTINUOUS\n"
        "0\nENDTAB\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n"
        "0\nENDSEC\n0\nEOF\n";
    const DxfReadResult result = readText(text);

    CHECK(result.ok());
    if (!result.ok()) {
        return;
    }
    CHECK_EQ(result.document->layers().size(), std::size_t{1});
    CHECK_EQ(result.document->layers()[0].name, std::string("CUT"));
    CHECK_EQ(result.document->layers()[0].colorIndex, 3);
    CHECK(!result.document->layers()[0].visible);
}

TEST(dxf_reader, minimum_integer_layer_color_is_safely_defaulted)
{
    const std::string text =
        "0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n70\n1\n"
        "0\nLAYER\n2\nBAD\n70\n0\n62\n-2147483648\n6\nCONTINUOUS\n"
        "0\nENDTAB\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n"
        "0\nENDSEC\n0\nEOF\n";
    const DxfReadResult result = readText(text);

    CHECK(result.ok());
    CHECK(warningsContain(result, "ACI 颜色无效"));
    if (result.ok()) {
        CHECK_EQ(result.document->layers().size(), std::size_t{1});
        CHECK_EQ(result.document->layers()[0].colorIndex, 7);
        CHECK(!result.document->layers()[0].visible);
    }
}

TEST(dxf_reader, non_r12_version_and_non_millimetre_units_are_recoverable_warnings)
{
    const std::string text =
        "0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n"
        "9\n$INSUNITS\n70\n1\n0\nENDSEC\n"
        "0\nSECTION\n2\nENTITIES\n0\nENDSEC\n0\nEOF\n";
    const DxfReadResult result = readText(text);

    CHECK(result.ok());
    CHECK(warningsContain(result, "AC1015"));
    CHECK(warningsContain(result, "$INSUNITS"));
    if (result.ok()) {
        CHECK_EQ(result.document->unit(), std::string("mm"));
    }
}

TEST(dxf_reader, entities_only_minimal_dxf_is_accepted)
{
    const std::string records =
        "0\nCIRCLE\n8\n0\n10\n1\n20\n2\n40\n3\n";
    const DxfReadResult result = readText(entitiesDocument(records));

    CHECK(result.ok());
    if (result.ok()) {
        CHECK_EQ(result.document->entities().size(), std::size_t{1});
    }
}

TEST(dxf_reader, bad_input_stream_is_fatal)
{
    std::istringstream input;
    input.setstate(std::ios::badbit);
    const DxfReadResult result = readDxf(input);

    CHECK(!result.ok());
    CHECK(warningsContain(result, "输入流"));
}

TEST(dxf_reader, file_entry_point_reads_binary_mode_and_reports_missing_path)
{
    const std::filesystem::path path =
        std::filesystem::current_path() / "xuanmei_dxf_reader_contract_test.dxf";
    std::error_code cleanupError;
    std::filesystem::remove(path, cleanupError);
    {
        std::ofstream output(path, std::ios::binary);
        output << writeText(xm::core::makeSampleDocument());
    }

    const DxfReadResult result = readDxfFile(path);
    CHECK(result.ok());
    if (result.ok()) {
        CHECK_EQ(result.document->entities().size(), std::size_t{7});
    }
    std::filesystem::remove(path, cleanupError);
    CHECK(!cleanupError);

    const std::filesystem::path missing =
        std::filesystem::current_path() / "xuanmei_missing_reader_input.dxf";
    std::filesystem::remove(missing, cleanupError);
    CHECK(throwsContaining<std::runtime_error>(
        [&] { static_cast<void>(readDxfFile(missing)); }, missing.string()));
}
