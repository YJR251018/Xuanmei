# MVP 设计：C++ 最小闭环

对应 README「第一阶段：C++ 最小闭环」与技术路线「阶段 A」。
本文件是本次任务的契约：范围、接口、精度要求与验收命令。实现前需由项目负责人确认。

## 1. 本 MVP 要证明什么

用最小代价证明整条链路的**骨架是正确的**：

```text
程序生成测试几何
      │
      ▼
GeometryDocument（项目自有中间模型，单位毫米）
      │
      ├──────────────► MinimalDxfWriter ──► ASCII DXF 文件
      │                                          │
      ▼                                          ▼
Native2dAnalyzer（闭式长度/包围盒）        DxfReader（回读）
      │                                          │
      └────────────────┬─────────────────────────┘
                       ▼
              Qt 6 界面：预览 + 指标 + 实体表 + 日志
```

核心命题是**往返一致**：几何写入 DXF 再读回来，实体数、总长度、包围盒必须一致。
只要这条测试通过，后续接入 libcdr（阶段 B）和 OCC（阶段 C）就是替换两端的实现，而不用重做骨架。

## 2. 不在本次范围内（明确的非目标）

- CDR 解析、libcdr/librevenge 接入 —— 阶段 B；
- OpenCASCADE 接入、精确曲线偏差、拓扑修复（端点吸附、重复线清理、微小边过滤）—— 阶段 C；
- Bézier、椭圆、SPLINE、尺寸标注、块/INSERT、文字、填充、DXF 2000+ 的 OBJECTS/句柄；
- 图层的显示/隐藏、实体选择、图层颜色覆盖 —— 后续；
- 非恒等变换的实体求值（第一版遇到即记录警告，见 §6.3）；
- 性能优化与大规模文件（本 MVP 只保证 10⁴ 实体量级可用）。

## 3. 验收标准

| 编号 | 验收项 | 判定方法 |
|---|---|---|
| A1 | 零外部依赖配置并编译通过 | `cmake --preset core-msvc` + `cmake --build --preset core-msvc`，退出码 0，无警告（`/W4 /permissive- /WX`）。含 Qt 的 `mvp-msvc` 预设自切片 6 起适用 |
| A2 | 核心层不依赖 Qt | `src/core/**` 不含任何 Qt 头文件；core 测试可独立运行 |
| A3 | 几何长度精确（**内部闭式计算**，不经过 DXF） | 对内建样图，总长度 = **2371.238898038 mm**（见 §8 期望值），**绝对**误差上限 **1e-9** |
| A4 | 往返一致（**经 DXF 六位小数写出与回读**） | 写出 → 读回后，实体数与逐实体长度一致，**绝对**误差上限 **1e-6**。该阈值只对内建样图成立（样图坐标/半径/角度均为整数，六位小数零舍入），**不是**对任意几何的通用上界，推导与适用边界见 §7.1 |
| A5 | DXF 语法正确 | 组码/值严格成对；R12 结构完整；无多余空行破坏配对 |
| A6 | 界面可运行 | 启动即显示样图预览、指标与实体表，无异常弹窗 |
| A7 | 错误路径可控 | 空文件、损坏配对、不支持的实体均给出明确行为与日志，不崩溃 |

## 4. 目录结构（本次新增部分）

```text
Xuanmei/
├─ CMakeLists.txt                    新增：顶层工程（XM_BUILD_APP 开关，默认 OFF）
├─ CMakePresets.json                 新增：core-msvc（纯核心）与 mvp-msvc（含 Qt）两套预设
├─ docs/MVP_DESIGN.md                本文件
├─ src/
│  ├─ core/                          纯 C++20，不含 Qt
│  │  ├─ CMakeLists.txt
│  │  ├─ geometry.hpp                Point2d、向量/角度工具、Transform2d
│  │  ├─ entity.hpp/.cpp             Line/Circle/Arc/Polyline、Shape、闭式长度、kindName
│  │  ├─ document.hpp/.cpp           GeometryDocument、Layer、警告           ← 切片 2
│  │  ├─ native_analyzer.hpp/.cpp    指标、逐实体长度与紧凑包围盒            ← 切片 3
│  │  ├─ dxf_writer.hpp/.cpp         ASCII DXF R12 写出                      ← 切片 4
│  │  ├─ dxf_reader.hpp/.cpp         ASCII DXF R12 读入                      ← 切片 5
│  │  └─ sample_document.hpp/.cpp    内建样图（用于验收与演示）              ← 切片 2
│  └─ app/                           依赖 Qt 6 Widgets                       ← 切片 6
│     ├─ CMakeLists.txt
│     ├─ main.cpp
│     ├─ main_window.hpp/.cpp
│     └─ preview_view.hpp/.cpp
└─ tests/
   ├─ core/                          纯 C++ 单元测试
   │  ├─ CMakeLists.txt
   │  ├─ dsh_test.hpp                极简断言框架（见 §11 依赖说明）
   │  ├─ main.cpp                    仅提供 main()
   │  └─ test_geometry.cpp
   └─ fixtures/                      预留：脱敏图样
```

旧的 Python 原型与临时验证 UI 已于 2026-10-06 清理；仓库从此只保留 C++ 正式实现，避免两套行为逐渐分叉。

### 4.1 分片交付与评审

按 CONTRIBUTING 的「一次只处理一个边界明确的任务」，本 MVP 拆成若干可独立评审的切片。每片完成后由 Codex 审查，确认无误再进入下一片。

| 切片 | 内容 | 状态 |
|---|---|---|
| 1 | CMake 工程骨架 + `geometry` + `entity` + 单元测试 | **已完成**（A1、A3 的闭合公式部分已验证） |
| 2 | `document` + `sample_document` | **已完成并评审**（`Layer`/`GeometryEntity`/`GeometryDocument`/`makeSampleDocument` + 单元测试） |
| 3 | `native_analyzer`（含紧凑圆弧包围盒） | **已完成并复审**（逐实体/总长、类型与图层计数、声明闭合数、带实体 ID 的警告、坏数据隔离） |
| 4 | `dxf_writer`（ASCII R12） | **已完成并评审**（四类内部图元、图层、单位、文件入口和严格失败路径） |
| 5 | `dxf_reader` + 往返一致性测试 | 待开始 |
| 6 | Qt 6 界面 + `--selftest` | 待开始 |

切片 1 验收结果：

```text
cmake --preset core-msvc   → 配置成功
cmake --build --preset core-msvc → 无警告（/W4 /permissive- /WX）
ctest --preset core-msvc   → 100% tests passed, 0 tests failed out of 1
xm_core_tests.exe          → ran 33 test(s), 33 passed, 0 failed
```


## 5. 核心接口

### 5.1 几何值类型（`geometry.hpp`）

```cpp
namespace xm::core {

struct Point2d { double x = 0.0; double y = 0.0; };

// 行主序 2x3 仿射矩阵，内部为长度 6 的数组 {a, b, c, d, e, f}
//   x' = a*x + c*y + e
//   y' = b*x + d*y + f
class Transform2d {
public:
    static Transform2d identity();
    static Transform2d translation(Point2d);
    static Transform2d scaling(double sx, double sy);
    static Transform2d rotation(double radians);
    static Transform2d fromCoefficients(double a, double b, double c,
                                        double d, double e, double f);
    bool isIdentity(double tol = 1e-12) const;
    Point2d apply(Point2d) const;
    Transform2d then(const Transform2d& next) const;   // this 先作用，再 next
};

// 闭式长度，单位毫米
double length(const Line&);      double length(const Circle&);
double length(const Arc&);       double length(const Polyline&);

constexpr double kPi = 3.14159265358979323846;
double toRadians(double degrees);
double normalizeDegrees(double degrees);   // 归一化到 [0, 360)
}
```

### 5.2 实体（`entity.hpp`）

```cpp
namespace xm::core {

struct Line     { Point2d from; Point2d to; };
struct Circle   { Point2d center; double radius = 0.0; };
struct Arc      { Point2d center; double radius = 0.0;
                  double startDegrees = 0.0; double endDegrees = 0.0; };
struct Polyline { std::vector<Point2d> points; bool closed = false; };

// 圆周角约定：角度为度、逆时针为正、0° 指向 +X，与 DXF ARC 的组码 50/51 完全一致。
// 圆弧扫角 = normalizeDegrees(endDegrees - startDegrees)，落在 [0, 360)；
// 整圆必须使用 Circle，不允许用 start==end 的 Arc 表示（避免 0° 与 360° 歧义）。

using Shape = std::variant<Line, Circle, Arc, Polyline>;

struct GeometryEntity {
    std::uint64_t   id = 0;          // 文档内原始对象编号
    std::string     layer = "0";
    Shape           shape;
    Transform2d     transform;       // 默认恒等；导入时记录原始变换
    std::vector<std::string> warnings;  // 该实体的转换警告
};

const char* kindName(const Shape&);   // "LINE" / "CIRCLE" / "ARC" / "POLYLINE"
}
```

### 5.3 文档（`document.hpp`）

```cpp
namespace xm::core {

struct Layer { std::string name; int colorIndex = 7; bool visible = true; };

class GeometryDocument {
public:
    static constexpr const char* kUnitMillimeter = "mm";

    std::uint64_t add(Shape shape, std::string layer = "0",
                      Transform2d transform = Transform2d::identity(),
                      std::vector<std::string> warnings = {});
    void addLayer(Layer layer);
    void addWarning(std::string message);      // 文档级警告（不绑定单一实体）

    const std::vector<GeometryEntity>& entities() const;
    const std::vector<Layer>& layers() const;
    const std::vector<std::string>& warnings() const;
    std::string unit() const;                  // 本 MVP 恒为 "mm"
    void setUnit(std::string unit);
};
}
```

`id` 由 `add()` 单调分配（1 起）。第三方类型（Qt、libcdr、DXF）一律不得进入 `core`。

### 5.4 分析器（`native_analyzer.hpp`）

```cpp
namespace xm::core {

struct EntityMetric {
    std::uint64_t id;  std::string kind;  std::string layer;
    double length = 0.0;  bool declaredClosed = false;
    bool hasWarning = false;
};

struct AnalysisWarning {
    std::optional<std::uint64_t> entityId;  // 文档级警告为空
    std::string message;
};

struct DocumentMetrics {
    std::map<std::string, std::size_t> countsByKind;
    std::map<std::string, std::size_t> countsByLayer;
    double totalLength = 0.0;
    std::size_t declaredClosedPolylineCount = 0;
    bool   hasBounds = false;
    double minX = 0, minY = 0, maxX = 0, maxY = 0;
    std::vector<EntityMetric> perEntity;
    std::vector<AnalysisWarning> warnings;
};

class IGeometryAnalyzer {                 // 阶段 C 由 OcctAnalyzer 实现
public:
    virtual ~IGeometryAnalyzer() = default;
    virtual DocumentMetrics analyze(const GeometryDocument&) const = 0;
};

class Native2dAnalyzer final : public IGeometryAnalyzer {
public:
    DocumentMetrics analyze(const GeometryDocument&) const override;
    static double width(const DocumentMetrics&);
    static double height(const DocumentMetrics&);
};
}
```

分析器行为：

- 计数覆盖全部实体；逐实体警告按实体顺序追加在文档警告之后；
- 非恒等变换暂不求值，仍按原始几何统计，并追加约定警告；
- 非有限坐标、非正半径或计算结果不可表示时，该实体仍计数，但不污染总长度与包围盒，并标记警告；
- 空折线与零扫角圆弧不产生包围盒；单点折线产生零宽高包围盒；
- `declaredClosedPolylineCount` 只按 `Polyline::closed` 源标志统计，不声称完成拓扑有效性验证；
- 声明闭合但长度为零的折线仍保留在声明计数中，同时产生带实体 ID 的退化警告；
- 圆弧包围盒包含起终点及实际扫角经过的 0°/90°/180°/270° 基数方向。

### 5.5 DXF 写出（`dxf_writer.hpp`）

```cpp
namespace xm::core {

class IDxfWriter {                        // 阶段 B/C 可替换实现
public:
    virtual ~IDxfWriter() = default;
    virtual void write(const GeometryDocument&, std::ostream&) const = 0;
};

class MinimalDxfWriter final : public IDxfWriter {
public:
    void write(const GeometryDocument&, std::ostream&) const override;
};

// 便利入口：路径写入，打开/写入失败抛 std::runtime_error
void writeDxfFile(const GeometryDocument&, const std::filesystem::path&);
}
```

### 5.6 DXF 读入（`dxf_reader.hpp`）

```cpp
namespace xm::core {

struct DxfReadResult {
    std::optional<GeometryDocument> document;  // 致命错误时为空
    std::vector<std::string> warnings;         // 可恢复问题
    bool ok() const { return document.has_value(); }
};

DxfReadResult readDxf(std::istream&);
DxfReadResult readDxfFile(const std::filesystem::path&);
}
```

**错误行为（MUST）**

| 情形 | 行为 |
|---|---|
| 文件不存在 / 无法打开 | 抛 `std::runtime_error`（含路径） |
| 二进制 DXF（`AutoCAD Binary DXF`） | 返回 `ok()==false`，警告「仅支持 ASCII DXF」 |
| 组码行不是整数 / 组码与值数量为奇数 | 返回 `ok()==false`，警告含行号 |
| 未闭合的 SECTION / 缺 EOF | 尽力解析已有内容，`ok()==true` 并追加警告 |
| 不支持的实体（SPLINE/TEXT/INSERT…） | 跳过，按类型计数为一条汇总警告，不中断 |
| 未知组码、缺省必填组码 | 取默认值并记警告 |
| 空文件（0 字节） | `ok()==false`，警告「文件为空」 |

读入端为**宽容模式**：可恢复问题绝不中断，全部通过 `warnings` 上报到界面日志。

### 5.7 演示样图（`sample_document.hpp`）

```cpp
namespace xm::core {
GeometryDocument makeSampleDocument();          // 确定性，无随机
}
```

## 6. DXF 约定（本 MVP 冻结的部分）

### 6.1 版本与结构

- 版本：**ASCII DXF R12**，`$ACADVER = AC1009`，无句柄、无 BLOCKS/OBJECTS 段；
- 段顺序：`HEADER` → `TABLES`（只含 `LAYER`）→ `ENTITIES` → `EOF`；
- 换行统一 `\n`；组码与值严格成对，每行一项；
- 数值格式：定点小数 `%.6f`（**不用** `%g`，避免科学计数法与精度丢失）。
  六位小数意味着单个坐标的舍入误差上限为 **5e-7 mm**。该误差经长度公式传播后会随实体类型与顶点数增长（推导见 §7.1），**不能**沿用内部档的 1e-9 验收。

### 6.2 写出的实体与组码

| 实体 | 组码 |
|---|---|
| `LINE` | 8 图层；10/20 起点；11/21 终点 |
| `CIRCLE` | 8；10/20 圆心；40 半径 |
| `ARC` | 8；10/20 圆心；40 半径；50 起始角(°)；51 终止角(°) |
| 内部 `Polyline` | R12 `POLYLINE`（8 图层；66=1；70 标志，1=闭合）+ 每点一个 `VERTEX`（8；10/20/30）+ `SEQEND` |

### 6.3 单位与变换

- 内部长度单位统一 **毫米**；`HEADER` 写 `$INSUNITS = 4`（毫米）、`$MEASUREMENT = 1`（公制）、`$LUNITS = 2`（十进制）、`$LUPREC = 6`。
- 注意：`$INSUNITS` 属 R13+ 变量。R12 客户端会忽略未知变量，因此写它既保持 R12 兼容，又让现代 CAD 正确识别为毫米。
- `GeometryEntity::transform` 若为非恒等：分析器按原始几何统计并追加带实体 ID 的警告；Writer 拒绝写出，避免把未应用变换的坐标静默导出。
- Writer 只接受毫米文档；R12 图层名最多 31 个 ASCII 字符，只允许字母、数字、`$`、`_`、`-`。
- 半径或圆弧扫角低于六位小数分辨率时拒绝写出，避免非零几何在文本中退化为零。

## 7. 精度与数值约定

- 全部坐标、半径、长度用 `double`；
- 长度公式为闭式，无离散化误差：
  - `Line`：`|to - from|`
  - `Circle`：`2πr`
  - `Arc`：`r · radians(normalizeDegrees(end - start))`
  - `Polyline`：相邻顶点距离之和，闭合时补末点→首点
- **包围盒取紧凑值**（圆弧按其实际角度范围求极值，而非整圆外接矩形）；
- 采样点约定：若将来需要离散，圆与弧的采样点数按弦高误差 `<= 0.05 mm` 计算并向上取整（本 MVP 不写 SPLINE，不涉及）。

### 7.1 两档精度约定（不可混用）

误差上限取决于**是否经过 DXF 文本**，因此分为两档，必须分别验收：

| 档位 | 适用范围 | 绝对误差上限 | 依据 |
|---|---|---|---|
| **内部档** | §8 内建样图的闭式几何计算：`length()`、包围盒、实体计数，不经过任何文本往返。该 1e-9 是**这条验收判据**的上限，不是"任意量级下 `double` 都能达到 1e-9"的断言 | **1e-9** | 样图数值在 2371 量级，1 ulp 约为 4.55e-13，1e-9 相当于约 2199 ulp，余量充足。绝对精度随量级退化：约 1e16 处 `double` 的相邻可表示值间距已达 2，任何绝对 1e-9 判据在那里都不可能成立 |
| **DXF 档** | 经 DXF 六位小数写出后回读得到的数值 | **无与几何无关的通用常量**，须按实体类型与顶点数逐个推导；本 MVP 对内建样图的验收阈值取 **1e-6** | 单值舍入上限 5e-7；传播后随实体类型与多段线顶点数增长，推导见下 |

**为什么 DXF 档不能沿用 1e-9。** 写出格式为定点六位小数（§6.1），单个坐标的舍入误差上限为

```text
0.5 × 10^-6 = 5e-7 mm
```

`5e-7` 已是 `1e-9` 的 500 倍，因此对任意输入都**不能保证** 1e-9。

两个方向都要说清楚，否则仍然会误导：

- **不能保证**不等于**必然超过**。`5e-7` 是单值舍入的**上限**，不是每次都会达到的实际误差；误差还可能在长度计算中相互抵消。例如端点横坐标 `0.0000001` 与 `1.0000001` 都无法用六位小数精确表示，但两者共同舍入后长度仍精确为 `1`，误差为 `0`。
- 但确实存在实际误差远大于 1e-9 的输入：半径 `1.00000049` 写出为 `1.000000`，回读圆周长误差约 `3.08e-6`。

因此结论只能表述为：**经过 DXF 文本时，1e-9 不能作为通用判据**。§8 内建样图的实际舍入误差为 0，是"恰好能被六位小数精确表示"的一个特例，而不是唯一可能满足 1e-9 的情况。

**为什么不存在与几何无关的常量上限。** 长度由坐标**计算**得出，误差沿长度公式传播，并随实体类型与顶点数变化：

```text
单个点（两个坐标各自舍入）的位移误差 <= sqrt(5e-7^2 + 5e-7^2) = sqrt(2) x 5e-7 = 7.071068e-7 mm
```

| 实体 | 长度误差上限（推导） | 对内建样图（§8）代入 |
|---|---|---|
| `LINE` | 两端点位移误差相加 `<= 2 x sqrt(2) x 5e-7` = **1.414214e-6** | **0**（端点坐标 `0/300` 均为整数） |
| `CIRCLE` | 半径舍入经 `2πr` 放大 `<= 2π x 5e-7` = **3.141593e-6** | **0**（`r = 50` 精确） |
| `ARC` | 半径项 `Δθ x 5e-7` 加角度项 `r x 1e-6 x π/180`；`r = 100`、`Δθ = π/2` 时为 `7.853982e-7 + 1.745329e-6` = **2.530727e-6** | **0**（`r = 100`、`0°/90°` 精确） |
| `POLYLINE`（n 段） | 各段两端点误差累计 `<= n x 1.414214e-6`，**随 n 线性增长** | **0**（顶点均为整数） |

`ARC` 一行的角度项是 `r x 1e-6 x π/180` 而不是 `r x 5e-7 x π/180`，因为**起始角与终止角是两个独立写出、独立舍入的字段**（组码 50 与 51），各自误差上限 `5e-7` 度，角差的误差上限因此是 `1e-6` 度。此外该项只在舍入**未跨越角度规范化分支**时成立：`Arc` 的长度按 `normalizeDegrees(end - start)` 计算，若某个角的舍入使差值跨过 `0°/360°` 边界，必须按具体数值重新推导。内建样图的 `0°/90°` 不涉及该分支。

表中的常数都按**向上取整**写成上界（`1.414214e-6` 而非 `1.4142e-6`、`3.141593e-6` 而非 `3.1416e-6`），因为"上限"一旦向下取整就不再是最坏情况的界。

`POLYLINE` 一行已说明问题：它没有与顶点数无关的上界，只有"随段数增长的上界"，因此**不可能**存在一个与几何无关的常量。文档的上一版写"严格最坏上界 1.414e-6，落在 2e-6 之内，本档取 1e-6"，是自相矛盾的：1e-6 **低于**它自己承认的最坏上界；而且当时只推导了 `LINE`，漏掉了 `CIRCLE` 的 2π 放大、`ARC` 的角度项与多段线的累计。

**本档的验收阈值必须绑定到具体样图。** A4 只验收 §8 的内建样图。该样图的坐标、半径、角度全部是整数，六位小数可精确表示，**逐项舍入误差为 0**；因此它的往返误差由写出/解析实现决定，而不是由舍入决定。取 **1e-6** 作为 A4 的阈值，是为实现缺陷（而非舍入）留出的余量：

- 对样图**充分**：实际舍入误差为 0，远小于 1e-6；
- **不是**对任意几何成立的通用上界，不得据此声称"经过 DXF 一定在 1e-6 以内"；
- 若将来加入坐标不能整格表示的往返样本，必须按上表对该样本单独推导阈值，并**同步**修改本节、§6.1、验收表 A4 与往返测试，不得只改一处。

**该阈值必须与验收表 A4 及后续往返测试保持一致**，文档与测试不得各写一个数。

### 7.2 断言方式

两档判据都必须用 `CHECK_NEAR_ABS`（严格绝对误差）验证：

- `CHECK_NEAR` 会把上限按数值量级放大（2371 量级下传 1e-6 实为约 2.37e-3 的绝对界），**不能**用于验证绝对误差；
- 该要求对内部档与 DXF 档同时有效，不因档位放宽而取消；
- 内部档的 1e-9 绑定 §8 内建样图（A3）。若将来要验收坐标量级显著更大的图，必须按那个量级的 ulp 重新推导绝对上限，不能沿用 1e-9。

切片 1 只覆盖内部档（A3 的闭式部分，判据 1e-9）；DXF 档（A4）自写出端与读入端落地后启用，判据 1e-6（该阈值绑定 §8 样图，适用边界见 §7.1）。

## 8. 内建样图与期望值（验收基准）

| # | 图层 | 实体 | 参数 | 长度 (mm) |
|---|---|---|---|---|
| 1 | `OUTLINE` | LINE | (0,0) → (300,0) | 300 |
| 2 | `OUTLINE` | LINE | (300,0) → (300,300) | 300 |
| 3 | `OUTLINE` | LINE | (300,300) → (0,300) | 300 |
| 4 | `OUTLINE` | LINE | (0,300) → (0,0) | 300 |
| 5 | `HOLES` | CIRCLE | 圆心 (450,150)，r = 50 | 100π ≈ 314.159265359 |
| 6 | `HOLES` | ARC | 圆心 (450,350)，r = 100，0° → 90° | 50π ≈ 157.079632679 |
| 7 | `SLOT` | POLYLINE（闭合） | (0,−200) (200,−200) (200,−50) (0,−50) | 700 |

**期望指标**

- 实体总数：**7**（LINE ×4、CIRCLE ×1、ARC ×1、POLYLINE ×1）
- 图层数：**3**（`OUTLINE`、`HOLES`、`SLOT`）
- 总长度：**2371.238898038 mm**（精确值 `1900 + 150π`：直线与折线 1900，圆 100π，弧 50π；已用独立探针程序逐项核对）
- 包围盒：`minX=0, minY=-200, maxX=550, maxY=450` → **宽 550.000000 × 高 650.000000 mm**
- 声明闭合的折线数：**1**（有效闭合轮廓需后续拓扑阶段另行判断）
- 文档警告：**0**

包围盒极值来源：

| 极值 | 来源 | 计算 |
|---|---|---|
| `minX = 0` | 正方形左边、折线左边 | `min(0, 0, 400, 450) = 0` |
| `minY = -200` | 折线下边 | 折线最下顶点 `y = -200` |
| `maxX = 550` | **圆弧起点** | 弧起点在 0°：`450 + 100·cos 0° = 550` |
| `maxY = 450` | **圆弧终点** | 弧终点在 90°：`350 + 100·sin 90° = 450` |

因此宽 `550 - 0 = 550`、高 `450 - (-200) = 650`。

> 修正记录：本节原先写作 `maxX=500` → 宽 500，理由是"圆最右 450+50=500"，**漏掉了圆弧自身的最右点**。弧心 `x=450`、半径 `100`、起始角 `0°`，起点即 `(550, 350)`，故 `maxX` 必须取 550。原注释中"弧覆盖 x∈[450,550]"与"故最上取 350+100=450"只推导了 `maxY`，未推导 `maxX`，属自相矛盾。该错误由独立重算与人工核图发现。

## 9. 界面设计（`src/app`）

单窗口，四区：

```text
┌──────────────────────────────────────────────────────────────┐
│ 菜单：文件(打开 DXF / 导出 DXF / 退出)  视图(适应窗口 / 重置) │
├──────────────────────────────────────────────────────────────┤
│ 工具栏：打开 DXF | 导出 DXF | 适应窗口 | 重置视图            │
├───────────────────────────────────┬──────────────────────────┤
│                                   │ 指标                     │
│  预览（QGraphicsView，Y 轴向上）  │  实体总数 / 总长度        │
│  深色背景、青色线、cosmetic 画笔  │  包围盒 宽 × 高           │
│  自动 fitInView（20 mm 边距）     │  图层数 / 闭合数          │
│                                   │  警告数                  │
│                                   ├──────────────────────────┤
│                                   │ 实体表                   │
│                                   │  ID/类型/图层/长度/闭合   │
├───────────────────────────────────┴──────────────────────────┤
│ 运行日志（可滚动，含全部解析警告）                            │
└──────────────────────────────────────────────────────────────┘
```

- 坐标映射：世界 Y 向上；视图用 `QGraphicsView::scale(1, -1)`，圆弧角度与 DXF 完全一致，无需翻转角度符号；
- 画笔 `setCosmetic(true)`，缩放时线宽不随比例放大；
- 实体按图层着色（图层色为 DXF ACI 索引，映射一张最小调色表）；
- 启动流程：构造样图 → 导出到 `<build>/out/mvp_sample.dxf` → 回读 → 用**回读结果**做全部展示（即界面本身就是往返验证的可视证据）；
- 无参数启动进 GUI；带 `--selftest` 时无界面运行同一流程，把指标打印到 stdout 并按 A3/A4 断言，供无 GUI 环境验收。

## 10. 验收命令

切片 1 起即可执行（无需 vcvars64、无需 Qt；预设使用 VS 生成器，能自行定位工具链）：

```powershell
cmake --preset core-msvc
cmake --build --preset core-msvc
ctest --preset core-msvc --output-on-failure
.\build\core-msvc\bin\xm_core_tests.exe          # 直接运行，可加名称子串过滤
```

切片 6（Qt 界面）完成后追加：

```powershell
cmake --preset mvp-msvc
cmake --build --preset mvp-msvc
ctest --preset mvp-msvc --output-on-failure
.\build\mvp-msvc\bin\cdr2dxf_workbench.exe --selftest   # 无界面自检
.\build\mvp-msvc\bin\cdr2dxf_workbench.exe              # 界面
```

**预期结果**

- `core-msvc`：配置与编译退出码 0、无警告；`100% tests passed`；`xm_core_tests.exe` 输出 `ran 86 test(s), 86 passed, 0 failed`（切片 3 评审后实测值，随切片递增）；
- `mvp-msvc --selftest`：输出含 `entities=7  layers=3  closed=1  warnings=0`、`totalLength=2371.238898038`、`bounds=550.000000 x 650.000000`；
- 界面：窗口显示样图（方框 + 圆 + 四分之一弧 + 下方矩形），指标与实体表与上表一致。

## 11. 依赖与许可证

| 依赖 | 版本 | 许可证 | 说明 |
|---|---|---|---|
| MSVC (Visual Studio 2026 Community) | 14.44 / 14.51 | 专有（免费 Community） | 本机已装；VS 生成器默认选用 14.44 |
| CMake | 4.2.3-msvc3（VS 自带） | BSD-3-Clause | 本机已装；不在 PATH 中，需要使用 VS 自带路径或先加入 PATH |
| Ninja | VS 自带 | Apache-2.0 | 本机已装；切片 1 使用 VS 生成器，暂不需要 |
| Qt 6 | **6.8 LTS**（`win64_msvc2022_64`） | LGPL-3.0（动态链接） | 本次安装到 `D:\Qt\6.8.0\msvc2022_64`；CI 或镜像不可用时改用官方源 `--base https://download.qt.io` |

- Qt 采用**动态链接**，符合 LGPL-3.0 使用方式；发布时需附许可证文本并允许用户替换 Qt 库。这一点应在确定项目许可证前记录（后续追加 ADR）。
- 单元测试框架：本 MVP **内置极简断言头** `tests/core/dsh_test.hpp`（仅标准库，无外部依赖），以便在无 vcpkg 的环境下完成验收。ADR-002 已声明第一阶段避免不必要依赖；待 vcpkg 落地后再迁移到 GoogleTest/Catch2，测试用例的 `TEST(...)` 结构保持不变以便平滑迁移。
- 仍未引入 libcdr、libdxfrw、OCC —— 全部留给阶段 B/C。

## 12. 已知限制与下一步

| 限制 | 计划处理阶段 |
|---|---|
| 不支持 CDR（仅程序生成几何与 DXF 回读） | 阶段 B（libcdr） |
| 折线不含 bulge（弧段），Bézier 未支持 | 阶段 B/C |
| 非恒等变换不参与求值，仅告警 | 阶段 B |
| 端点吸附、重复线清理、微小边过滤未实现 | 阶段 C（OCC） |
| 图层可见性、实体选择未实现 | MVP 之后 |
| 无 DXF SPLINE 写出 | 阶段 B（曲线离散化后） |

**建议的实现顺序**：`geometry` → `entity`/`document` → `sample_document` → `native_analyzer` → `dxf_writer` → `dxf_reader` → core 测试 → Qt 界面 → 全链路验收。
