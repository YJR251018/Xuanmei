#include "core/dxf_writer.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/entity.hpp"

namespace xm::core {
namespace {

constexpr int kDefaultLayerColor = 7;
constexpr std::size_t kMaxLayerCount = 32'767;
constexpr double kDxfResolution = 1e-6;

class DxfTextBuilder {
public:
    DxfTextBuilder()
    {
        output_.imbue(std::locale::classic());
        output_ << std::fixed << std::setprecision(6);
    }

    void pair(int code, std::string_view value)
    {
        output_ << code << '\n' << value << '\n';
    }

    void pair(int code, std::int64_t value)
    {
        output_ << code << '\n' << value << '\n';
    }

    void pair(int code, double value)
    {
        // Avoid spelling exact negative zero as "-0.000000".
        if (value == 0.0) {
            value = 0.0;
        }
        output_ << code << '\n' << value << '\n';
    }

    std::string finish()
    {
        if (!output_) {
            throw std::runtime_error("生成 DXF 文本失败");
        }
        return std::move(output_).str();
    }

private:
    std::ostringstream output_;
};

bool isFinite(const Point2d& point) noexcept
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

double dxfAngle(double degrees) noexcept
{
    const double normalized = normalizeDegrees(degrees);
    const double rounded = std::round(normalized / kDxfResolution) * kDxfResolution;
    return normalizeDegrees(rounded);
}

void requireLayerName(std::string_view value)
{
    if (value.empty()) {
        throw std::invalid_argument("图层名不能为空");
    }
    if (value.size() > 31U) {
        throw std::invalid_argument("R12 图层名不能超过 31 个 ASCII 字符: " + std::string(value));
    }
    for (const char character : value) {
        const unsigned char byte = static_cast<unsigned char>(character);
        const bool asciiLetter = (byte >= static_cast<unsigned char>('A')
                                  && byte <= static_cast<unsigned char>('Z'))
            || (byte >= static_cast<unsigned char>('a')
                && byte <= static_cast<unsigned char>('z'));
        const bool asciiDigit = byte >= static_cast<unsigned char>('0')
            && byte <= static_cast<unsigned char>('9');
        const bool allowed = asciiLetter || asciiDigit || character == '$'
            || character == '_' || character == '-';
        if (!allowed) {
            throw std::invalid_argument(
                "R12 图层名只能使用 ASCII 字母、数字、$、_ 或 -: " + std::string(value));
        }
    }
}

[[noreturn]] void rejectEntity(std::uint64_t id, std::string_view reason)
{
    throw std::invalid_argument("实体 " + std::to_string(id) + " " + std::string(reason));
}

void validateShape(const GeometryEntity& entity)
{
    std::visit(
        [&entity](const auto& concrete) {
            using T = std::decay_t<decltype(concrete)>;
            if constexpr (std::is_same_v<T, Line>) {
                if (!isFinite(concrete.from) || !isFinite(concrete.to)) {
                    rejectEntity(entity.id, "包含非有限坐标");
                }
            } else if constexpr (std::is_same_v<T, Circle>) {
                if (!isFinite(concrete.center) || !std::isfinite(concrete.radius)) {
                    rejectEntity(entity.id, "包含非有限圆参数");
                }
                if (concrete.radius <= 0.0) {
                    rejectEntity(entity.id, "的半径必须大于零");
                }
                if (concrete.radius < kDxfResolution) {
                    rejectEntity(entity.id, "的半径小于 DXF 六位小数分辨率");
                }
            } else if constexpr (std::is_same_v<T, Arc>) {
                if (!isFinite(concrete.center) || !std::isfinite(concrete.radius)
                    || !std::isfinite(concrete.startDegrees)
                    || !std::isfinite(concrete.endDegrees)) {
                    rejectEntity(entity.id, "包含非有限圆弧参数");
                }
                if (concrete.radius <= 0.0) {
                    rejectEntity(entity.id, "的半径必须大于零");
                }
                if (concrete.radius < kDxfResolution) {
                    rejectEntity(entity.id, "的半径小于 DXF 六位小数分辨率");
                }
                const double sweep = arcSweepDegrees(concrete);
                if (sweep == 0.0) {
                    rejectEntity(entity.id, "是零扫角圆弧；整圆应使用 Circle");
                }
                if (sweep < kDxfResolution) {
                    rejectEntity(entity.id, "的扫角小于 DXF 六位小数分辨率");
                }
                if (dxfAngle(concrete.startDegrees) == dxfAngle(concrete.endDegrees)) {
                    rejectEntity(entity.id, "的角度在 DXF 六位小数写出后会退化为零扫角");
                }
            } else if constexpr (std::is_same_v<T, Polyline>) {
                if (concrete.points.empty()) {
                    rejectEntity(entity.id, "的折线至少需要一个顶点");
                }
                for (const Point2d& point : concrete.points) {
                    if (!isFinite(point)) {
                        rejectEntity(entity.id, "的折线包含非有限坐标");
                    }
                }
            }
        },
        entity.shape);
}

void validateDocument(const GeometryDocument& document)
{
    if (document.unit() != GeometryDocument::kUnitMillimeter) {
        throw std::invalid_argument("DXF Writer 只接受单位为 mm 的 GeometryDocument");
    }

    std::set<std::string> declaredLayers;
    for (const Layer& layer : document.layers()) {
        requireLayerName(layer.name);
        if (!declaredLayers.insert(layer.name).second) {
            throw std::invalid_argument("GeometryDocument 包含重复图层: " + layer.name);
        }
        if (layer.colorIndex < 1 || layer.colorIndex > 255) {
            throw std::invalid_argument("图层 " + layer.name + " 的 ACI 颜色必须在 1..255");
        }
    }

    for (const GeometryEntity& entity : document.entities()) {
        requireLayerName(entity.layer);
        if (!entity.transform.isIdentity(0.0)) {
            rejectEntity(entity.id, "带有尚未应用的非恒等变换");
        }
        validateShape(entity);
    }
}

std::vector<Layer> collectLayers(const GeometryDocument& document)
{
    std::vector<Layer> layers{Layer{"0", kDefaultLayerColor, true}};
    std::set<std::string> known{"0"};

    for (const Layer& layer : document.layers()) {
        if (layer.name == "0") {
            layers.front() = layer;
        } else {
            layers.push_back(layer);
            known.insert(layer.name);
        }
    }
    for (const GeometryEntity& entity : document.entities()) {
        if (known.insert(entity.layer).second) {
            layers.push_back(Layer{entity.layer, kDefaultLayerColor, true});
        }
    }
    if (layers.size() > kMaxLayerCount) {
        throw std::invalid_argument("DXF 图层数量超过组码 70 可表示范围");
    }
    return layers;
}

void writeHeader(DxfTextBuilder& dxf)
{
    dxf.pair(0, "SECTION");
    dxf.pair(2, "HEADER");
    dxf.pair(9, "$ACADVER");
    dxf.pair(1, "AC1009");
    dxf.pair(9, "$INSUNITS");
    dxf.pair(70, std::int64_t{4});
    dxf.pair(9, "$MEASUREMENT");
    dxf.pair(70, std::int64_t{1});
    dxf.pair(9, "$LUNITS");
    dxf.pair(70, std::int64_t{2});
    dxf.pair(9, "$LUPREC");
    dxf.pair(70, std::int64_t{6});
    dxf.pair(0, "ENDSEC");
}

void writeLayerTable(DxfTextBuilder& dxf, const std::vector<Layer>& layers)
{
    dxf.pair(0, "SECTION");
    dxf.pair(2, "TABLES");
    dxf.pair(0, "TABLE");
    dxf.pair(2, "LAYER");
    dxf.pair(70, static_cast<std::int64_t>(layers.size()));
    for (const Layer& layer : layers) {
        dxf.pair(0, "LAYER");
        dxf.pair(2, layer.name);
        dxf.pair(70, std::int64_t{0});
        const int signedColor = layer.visible ? layer.colorIndex : -layer.colorIndex;
        dxf.pair(62, static_cast<std::int64_t>(signedColor));
        dxf.pair(6, "CONTINUOUS");
    }
    dxf.pair(0, "ENDTAB");
    dxf.pair(0, "ENDSEC");
}

void writeEntity(DxfTextBuilder& dxf, const GeometryEntity& entity)
{
    std::visit(
        [&dxf, &entity](const auto& concrete) {
            using T = std::decay_t<decltype(concrete)>;
            if constexpr (std::is_same_v<T, Line>) {
                dxf.pair(0, "LINE");
                dxf.pair(8, entity.layer);
                dxf.pair(10, concrete.from.x);
                dxf.pair(20, concrete.from.y);
                dxf.pair(30, 0.0);
                dxf.pair(11, concrete.to.x);
                dxf.pair(21, concrete.to.y);
                dxf.pair(31, 0.0);
            } else if constexpr (std::is_same_v<T, Circle>) {
                dxf.pair(0, "CIRCLE");
                dxf.pair(8, entity.layer);
                dxf.pair(10, concrete.center.x);
                dxf.pair(20, concrete.center.y);
                dxf.pair(30, 0.0);
                dxf.pair(40, concrete.radius);
            } else if constexpr (std::is_same_v<T, Arc>) {
                dxf.pair(0, "ARC");
                dxf.pair(8, entity.layer);
                dxf.pair(10, concrete.center.x);
                dxf.pair(20, concrete.center.y);
                dxf.pair(30, 0.0);
                dxf.pair(40, concrete.radius);
                dxf.pair(50, dxfAngle(concrete.startDegrees));
                dxf.pair(51, dxfAngle(concrete.endDegrees));
            } else if constexpr (std::is_same_v<T, Polyline>) {
                dxf.pair(0, "POLYLINE");
                dxf.pair(8, entity.layer);
                dxf.pair(66, std::int64_t{1});
                dxf.pair(70, concrete.closed ? std::int64_t{1} : std::int64_t{0});
                dxf.pair(10, 0.0);
                dxf.pair(20, 0.0);
                dxf.pair(30, 0.0);
                for (const Point2d& point : concrete.points) {
                    dxf.pair(0, "VERTEX");
                    dxf.pair(8, entity.layer);
                    dxf.pair(10, point.x);
                    dxf.pair(20, point.y);
                    dxf.pair(30, 0.0);
                }
                dxf.pair(0, "SEQEND");
                dxf.pair(8, entity.layer);
            }
        },
        entity.shape);
}

std::string serialize(const GeometryDocument& document)
{
    validateDocument(document);
    const std::vector<Layer> layers = collectLayers(document);

    DxfTextBuilder dxf;
    writeHeader(dxf);
    writeLayerTable(dxf, layers);
    dxf.pair(0, "SECTION");
    dxf.pair(2, "ENTITIES");
    for (const GeometryEntity& entity : document.entities()) {
        writeEntity(dxf, entity);
    }
    dxf.pair(0, "ENDSEC");
    dxf.pair(0, "EOF");
    return dxf.finish();
}

void writeText(std::ostream& output, const std::string& text, std::string_view destination)
{
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        throw std::runtime_error("写入" + std::string(destination) + "失败");
    }
}

}  // namespace

void MinimalDxfWriter::write(const GeometryDocument& document, std::ostream& output) const
{
    writeText(output, serialize(document), " DXF 输出流");
}

void writeDxfFile(const GeometryDocument& document, const std::filesystem::path& path)
{
    const std::string text = serialize(document);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("无法打开 DXF 输出文件: " + path.string());
    }
    writeText(output, text, " DXF 文件 " + path.string());
    output.close();
    if (!output) {
        throw std::runtime_error("关闭 DXF 输出文件失败: " + path.string());
    }
}

}  // namespace xm::core
