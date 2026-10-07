#include "core/dxf_reader.hpp"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <istream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/entity.hpp"

namespace xm::core {
namespace {

struct GroupPair {
    int code = 0;
    std::string value;
    std::size_t codeLine = 0;
    std::size_t valueLine = 0;
};

std::string_view trim(std::string_view value) noexcept
{
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1U);
}

template <typename Number>
std::optional<Number> parseNumber(std::string_view text) noexcept
{
    text = trim(text);
    if (text.empty()) {
        return std::nullopt;
    }
    Number value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    if constexpr (std::is_floating_point_v<Number>) {
        if (!std::isfinite(value)) {
            return std::nullopt;
        }
    }
    return value;
}

DxfReadResult fatal(std::string warning)
{
    DxfReadResult result;
    result.warnings.push_back(std::move(warning));
    return result;
}

std::string atLine(std::size_t line, std::string message)
{
    return "第 " + std::to_string(line) + " 行: " + std::move(message);
}

std::optional<std::vector<GroupPair>> readPairs(std::istream& input,
                                                DxfReadResult& failure)
{
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    if (input.bad()) {
        failure = fatal("读取 DXF 输入流失败");
        return std::nullopt;
    }
    if (lines.empty()) {
        failure = fatal("文件为空");
        return std::nullopt;
    }
    if (std::string_view(lines.front()).starts_with("AutoCAD Binary DXF")) {
        failure = fatal("仅支持 ASCII DXF");
        return std::nullopt;
    }
    if ((lines.size() % 2U) != 0U) {
        failure = fatal(atLine(lines.size(), "组码没有对应的值行"));
        return std::nullopt;
    }

    std::vector<GroupPair> pairs;
    pairs.reserve(lines.size() / 2U);
    for (std::size_t index = 0; index < lines.size(); index += 2U) {
        const std::optional<int> code = parseNumber<int>(lines[index]);
        if (!code.has_value()) {
            failure = fatal(atLine(index + 1U, "组码行不是整数"));
            return std::nullopt;
        }
        pairs.push_back(GroupPair{*code, std::move(lines[index + 1U]),
                                  index + 1U, index + 2U});
    }
    return pairs;
}

std::size_t nextRecord(const std::vector<GroupPair>& pairs,
                       std::size_t begin, std::size_t end) noexcept
{
    for (std::size_t index = begin; index < end; ++index) {
        if (pairs[index].code == 0) {
            return index;
        }
    }
    return end;
}

const GroupPair* findCode(const std::vector<GroupPair>& pairs,
                          std::size_t begin, std::size_t end, int code) noexcept
{
    for (std::size_t index = begin; index < end; ++index) {
        if (pairs[index].code == code) {
            return &pairs[index];
        }
    }
    return nullptr;
}

std::string fieldString(const std::vector<GroupPair>& pairs,
                        std::size_t begin, std::size_t end, int code,
                        std::string_view record, std::size_t recordLine,
                        std::string fallback, std::vector<std::string>& warnings)
{
    const GroupPair* pair = findCode(pairs, begin, end, code);
    if (pair == nullptr || trim(pair->value).empty()) {
        warnings.push_back(atLine(recordLine,
            std::string(record) + " 缺少组码 " + std::to_string(code)
                + "，按 " + fallback + " 处理"));
        return fallback;
    }
    return std::string(trim(pair->value));
}

double fieldDouble(const std::vector<GroupPair>& pairs,
                   std::size_t begin, std::size_t end, int code,
                   std::string_view record, std::size_t recordLine,
                   std::vector<std::string>& warnings)
{
    const GroupPair* pair = findCode(pairs, begin, end, code);
    if (pair == nullptr) {
        warnings.push_back(atLine(recordLine,
            std::string(record) + " 缺少组码 " + std::to_string(code)
                + "，按 0 处理"));
        return 0.0;
    }
    const std::optional<double> value = parseNumber<double>(pair->value);
    if (!value.has_value()) {
        warnings.push_back(atLine(pair->valueLine,
            std::string(record) + " 的组码 " + std::to_string(code)
                + " 数值损坏，按 0 处理"));
        return 0.0;
    }
    return *value;
}

int fieldInt(const std::vector<GroupPair>& pairs,
             std::size_t begin, std::size_t end, int code,
             std::string_view record, std::size_t recordLine,
             int fallback, std::vector<std::string>& warnings)
{
    const GroupPair* pair = findCode(pairs, begin, end, code);
    if (pair == nullptr) {
        warnings.push_back(atLine(recordLine,
            std::string(record) + " 缺少组码 " + std::to_string(code)
                + "，按 " + std::to_string(fallback) + " 处理"));
        return fallback;
    }
    const std::optional<int> value = parseNumber<int>(pair->value);
    if (!value.has_value()) {
        warnings.push_back(atLine(pair->valueLine,
            std::string(record) + " 的组码 " + std::to_string(code)
                + " 数值损坏，按 " + std::to_string(fallback) + " 处理"));
        return fallback;
    }
    return *value;
}

void parseHeader(const std::vector<GroupPair>& pairs,
                 std::size_t begin, std::size_t end,
                 std::vector<std::string>& warnings)
{
    for (std::size_t index = begin; index < end; ++index) {
        if (pairs[index].code != 9 || index + 1U >= end) {
            continue;
        }
        const std::string_view variable = trim(pairs[index].value);
        const GroupPair& value = pairs[index + 1U];
        if (variable == "$ACADVER") {
            const std::string_view version = trim(value.value);
            if (value.code != 1 || version != "AC1009") {
                warnings.push_back(atLine(value.valueLine,
                    "DXF 版本 " + std::string(version)
                        + " 不是 AC1009，将按 R12 子集尝试读取"));
            }
        } else if (variable == "$INSUNITS") {
            const std::optional<int> unit = parseNumber<int>(value.value);
            if (value.code != 70 || !unit.has_value() || *unit != 4) {
                warnings.push_back(atLine(value.valueLine,
                    "$INSUNITS 不是毫米(4)，内部仍按 mm 读取"));
            }
        }
    }
}

void parseLayerTable(const std::vector<GroupPair>& pairs,
                     std::size_t begin, std::size_t end,
                     GeometryDocument& document,
                     std::vector<std::string>& warnings)
{
    std::set<std::string> names;
    for (std::size_t index = begin; index < end;) {
        if (pairs[index].code != 0 || trim(pairs[index].value) != "LAYER") {
            ++index;
            continue;
        }
        const std::size_t recordEnd = nextRecord(pairs, index + 1U, end);
        const std::string name = fieldString(pairs, index + 1U, recordEnd, 2,
                                             "LAYER", pairs[index].codeLine,
                                             "0", warnings);
        const int signedColor = fieldInt(pairs, index + 1U, recordEnd, 62,
                                         "LAYER", pairs[index].codeLine,
                                         7, warnings);
        long long colorMagnitude = static_cast<long long>(signedColor);
        if (colorMagnitude < 0) {
            colorMagnitude = -colorMagnitude;
        }
        int color = 7;
        if (colorMagnitude < 1 || colorMagnitude > 255) {
            warnings.push_back(atLine(pairs[index].codeLine,
                "图层 " + name + " 的 ACI 颜色无效，按 7 处理"));
        } else {
            color = static_cast<int>(colorMagnitude);
        }
        if (!names.insert(name).second) {
            warnings.push_back(atLine(pairs[index].codeLine,
                "重复图层 " + name + " 已忽略"));
        } else if (name != "0") {
            document.addLayer(Layer{name, color, signedColor >= 0});
        }
        index = recordEnd;
    }
}

std::size_t parsePolyline(const std::vector<GroupPair>& pairs,
                          std::size_t header, std::size_t end,
                          GeometryDocument& document,
                          std::vector<std::string>& warnings)
{
    const std::size_t headerEnd = nextRecord(pairs, header + 1U, end);
    const std::string layer = fieldString(pairs, header + 1U, headerEnd, 8,
                                          "POLYLINE", pairs[header].codeLine,
                                          "0", warnings);
    const int flags = fieldInt(pairs, header + 1U, headerEnd, 70,
                               "POLYLINE", pairs[header].codeLine,
                               0, warnings);
    bool supported = (flags & ~1) == 0;
    if (!supported) {
        warnings.push_back(atLine(pairs[header].codeLine,
            "POLYLINE 含不支持的标志位 " + std::to_string(flags)
                + "，已跳过该实体"));
    }
    std::vector<Point2d> points;
    std::size_t cursor = headerEnd;
    bool foundEnd = false;
    while (cursor < end) {
        if (pairs[cursor].code != 0) {
            ++cursor;
            continue;
        }
        const std::string_view type = trim(pairs[cursor].value);
        if (type == "VERTEX") {
            const std::size_t recordEnd = nextRecord(pairs, cursor + 1U, end);
            const double x = fieldDouble(pairs, cursor + 1U, recordEnd, 10,
                                         "VERTEX", pairs[cursor].codeLine, warnings);
            const double y = fieldDouble(pairs, cursor + 1U, recordEnd, 20,
                                         "VERTEX", pairs[cursor].codeLine, warnings);
            if (const GroupPair* bulge = findCode(pairs, cursor + 1U, recordEnd, 42);
                bulge != nullptr) {
                const std::optional<double> value = parseNumber<double>(bulge->value);
                if (!value.has_value() || *value != 0.0) {
                    supported = false;
                    warnings.push_back(atLine(bulge->valueLine,
                        "VERTEX 含损坏或非零 bulge，不能按直线折线读取；"
                        "已跳过所属 POLYLINE"));
                }
            }
            points.push_back(Point2d{x, y});
            cursor = recordEnd;
            continue;
        }
        if (type == "SEQEND") {
            foundEnd = true;
            cursor = nextRecord(pairs, cursor + 1U, end);
        }
        break;
    }
    if (!foundEnd) {
        warnings.push_back(atLine(pairs[header].codeLine,
            "POLYLINE 缺少 SEQEND，已保留已读取顶点"));
    }
    if (points.empty()) {
        warnings.push_back(atLine(pairs[header].codeLine,
            "POLYLINE 没有 VERTEX，已创建空折线"));
    }
    if (supported) {
        document.add(Polyline{std::move(points), (flags & 1) != 0}, layer);
    }
    return cursor;
}

void parseEntities(const std::vector<GroupPair>& pairs,
                   std::size_t begin, std::size_t end,
                   GeometryDocument& document,
                   std::vector<std::string>& warnings,
                   std::map<std::string, std::size_t>& unknown)
{
    for (std::size_t index = begin; index < end;) {
        if (pairs[index].code != 0) {
            warnings.push_back(atLine(pairs[index].codeLine,
                "ENTITIES 中出现实体记录外的组码，已忽略"));
            ++index;
            continue;
        }
        const std::string type(trim(pairs[index].value));
        if (type == "POLYLINE") {
            index = parsePolyline(pairs, index, end, document, warnings);
            continue;
        }

        const std::size_t recordEnd = nextRecord(pairs, index + 1U, end);
        const std::string layer = type == "SEQEND"
            ? std::string("0")
            : fieldString(pairs, index + 1U, recordEnd, 8,
                          type, pairs[index].codeLine, "0", warnings);
        if (type == "LINE") {
            document.add(Line{
                Point2d{fieldDouble(pairs, index + 1U, recordEnd, 10,
                                    type, pairs[index].codeLine, warnings),
                        fieldDouble(pairs, index + 1U, recordEnd, 20,
                                    type, pairs[index].codeLine, warnings)},
                Point2d{fieldDouble(pairs, index + 1U, recordEnd, 11,
                                    type, pairs[index].codeLine, warnings),
                        fieldDouble(pairs, index + 1U, recordEnd, 21,
                                    type, pairs[index].codeLine, warnings)}},
                layer);
        } else if (type == "CIRCLE") {
            document.add(Circle{
                Point2d{fieldDouble(pairs, index + 1U, recordEnd, 10,
                                    type, pairs[index].codeLine, warnings),
                        fieldDouble(pairs, index + 1U, recordEnd, 20,
                                    type, pairs[index].codeLine, warnings)},
                fieldDouble(pairs, index + 1U, recordEnd, 40,
                            type, pairs[index].codeLine, warnings)},
                layer);
        } else if (type == "ARC") {
            document.add(Arc{
                Point2d{fieldDouble(pairs, index + 1U, recordEnd, 10,
                                    type, pairs[index].codeLine, warnings),
                        fieldDouble(pairs, index + 1U, recordEnd, 20,
                                    type, pairs[index].codeLine, warnings)},
                fieldDouble(pairs, index + 1U, recordEnd, 40,
                            type, pairs[index].codeLine, warnings),
                fieldDouble(pairs, index + 1U, recordEnd, 50,
                            type, pairs[index].codeLine, warnings),
                fieldDouble(pairs, index + 1U, recordEnd, 51,
                            type, pairs[index].codeLine, warnings)},
                layer);
        } else if (type != "SEQEND") {
            ++unknown[type.empty() ? "<空实体类型>" : type];
        }
        index = recordEnd;
    }
}

}  // namespace

DxfReadResult readDxf(std::istream& input)
{
    DxfReadResult failure;
    const std::optional<std::vector<GroupPair>> parsed = readPairs(input, failure);
    if (!parsed.has_value()) {
        return failure;
    }
    const std::vector<GroupPair>& pairs = *parsed;

    DxfReadResult result;
    result.document.emplace();
    std::map<std::string, std::size_t> unknownEntities;
    bool foundEof = false;
    bool foundEntities = false;

    for (std::size_t index = 0; index < pairs.size();) {
        if (pairs[index].code == 0 && trim(pairs[index].value) == "EOF") {
            foundEof = true;
            ++index;
            continue;
        }
        if (pairs[index].code != 0 || trim(pairs[index].value) != "SECTION") {
            ++index;
            continue;
        }
        if (index + 1U >= pairs.size() || pairs[index + 1U].code != 2) {
            result.warnings.push_back(atLine(pairs[index].codeLine,
                "SECTION 缺少组码 2 的段名，已跳过"));
            ++index;
            continue;
        }

        const std::string section(trim(pairs[index + 1U].value));
        const std::size_t contentBegin = index + 2U;
        std::size_t contentEnd = contentBegin;
        while (contentEnd < pairs.size()) {
            if (pairs[contentEnd].code == 0) {
                const std::string_view marker = trim(pairs[contentEnd].value);
                if (marker == "ENDSEC" || marker == "EOF" || marker == "SECTION") {
                    break;
                }
            }
            ++contentEnd;
        }
        const bool closed = contentEnd < pairs.size()
            && pairs[contentEnd].code == 0
            && trim(pairs[contentEnd].value) == "ENDSEC";

        if (section == "HEADER") {
            parseHeader(pairs, contentBegin, contentEnd, result.warnings);
        } else if (section == "TABLES") {
            parseLayerTable(pairs, contentBegin, contentEnd,
                            *result.document, result.warnings);
        } else if (section == "ENTITIES") {
            foundEntities = true;
            parseEntities(pairs, contentBegin, contentEnd,
                          *result.document, result.warnings, unknownEntities);
        }
        if (!closed) {
            result.warnings.push_back(atLine(pairs[index].codeLine,
                "SECTION " + section + " 缺少 ENDSEC，已尽力解析"));
            index = contentEnd;
        } else {
            index = contentEnd + 1U;
        }
    }

    if (!foundEntities) {
        result.warnings.push_back("缺少 ENTITIES 段，返回空文档");
    }
    if (!foundEof) {
        result.warnings.push_back("缺少 EOF，已尽力解析已有内容");
    }
    for (const auto& [type, count] : unknownEntities) {
        result.warnings.push_back("跳过不支持的实体 " + type + "，共 "
                                  + std::to_string(count) + " 个");
    }
    return result;
}

DxfReadResult readDxfFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("无法打开 DXF 输入文件: " + path.string());
    }
    return readDxf(input);
}

}  // namespace xm::core
