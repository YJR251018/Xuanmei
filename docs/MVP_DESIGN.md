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
| A3 | 几何长度精确 | 对内建样图，总长度 = **2371.238898038 mm**（见 §8 期望值），容差 1e-9 |
| A4 | 往返一致 | 写出 → 读回后，实体数与逐实体长度一致，容差 1e-9 |
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
│  │  ├─ native_analyzer.hpp/.cpp    指标与逐实体长度                        ← 切片 3
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

`prototype/`（现为仓库根的 Python 原型）本次**不移动、不修改**，避免在动骨架的同时动历史代码。

### 4.1 分片交付与评审

按 CONTRIBUTING 的「一次只处理一个边界明确的任务」，本 MVP 拆成若干可独立评审的切片。每片完成后由 Codex 审查，确认无误再进入下一片。

| 切片 | 内容 | 状态 |
|---|---|---|
| 1 | CMake 工程骨架 + `geometry` + `entity` + 单元测试 | **已完成**（A1、A3 的闭合公式部分已验证） |
| 2 | `document` + `sample_document` | 待评审后开始 |
| 3 | `native_analyzer`（含紧凑圆弧包围盒） | 待开始 |
| 4 | `dxf_writer`（ASCII R12） | 待开始 |
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
// 圆弧扫角 = normalizeDegrees(endDegrees - startDegrees)，落在 (0, 360]；
// 整圆必须使用 Circle，不允许用 start==end 的 Arc 表示（避免 0° 与 360° 歧义）。

using Shape = std::variant<Line, Circle, Arc, Polyline>;

struct GeometryEntity {
    std::uint64_t   id = 0;          // 文档内原始对象编号
    std::string     layer = "0";
    Shape           shape;
    Transform2d     transform;       // 默认恒等；导入时记录原始变换
    std::vector<std::string> warnings;  // 该实体的转换警告
};

const char* kindName(const Shape&);   // "LINE" / "CIRCLE" / "ARC" / "LWPOLYLINE"
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
                      Transform2d transform = Transform2d::identity());
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
    double length = 0.0;  bool closed = false;
    bool hasWarning = false;
};

struct DocumentMetrics {
    std::map<std::string, std::size_t> countsByKind;
    std::map<std::string, std::size_t> countsByLayer;
    double totalLength = 0.0;
    bool   hasBounds = false;
    double minX = 0, minY = 0, maxX = 0, maxY = 0;
    std::vector<EntityMetric> perEntity;
    std::vector<std::string> warnings;    // 文档警告 + 逐实体警告
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

// 便利入口：路径写入，失败抛 std::runtime_error
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

- 版本：**ASCII DXF R12**，`$ACADVER = AC1009`，无句柄、无 OBJECTS 段；
- 段顺序：`HEADER` → `TABLES`（只含 `LAYER`）→ `ENTITIES` → `EOF`；
- 换行统一 `\n`；组码与值严格成对，每行一项；
- 数值格式：定点小数 `%.6f`（**不用** `%g`，避免科学计数法与精度丢失）。

### 6.2 写出的实体与组码

| 实体 | 组码 |
|---|---|
| `LINE` | 8 图层；10/20 起点；11/21 终点 |
| `CIRCLE` | 8；10/20 圆心；40 半径 |
| `ARC` | 8；10/20 圆心；40 半径；50 起始角(°)；51 终止角(°) |
| `LWPOLYLINE` | 8；90 顶点数；70 标志（1=闭合）；每顶点 10/20 |

### 6.3 单位与变换

- 内部长度单位统一 **毫米**；`HEADER` 写 `$INSUNITS = 4`（毫米）、`$MEASUREMENT = 1`（公制）、`$LUNITS = 2`（十进制）、`$LUPREC = 6`。
- 注意：`$INSUNITS` 属 R13+ 变量。R12 客户端会忽略未知变量，因此写它既保持 R12 兼容，又让现代 CAD 正确识别为毫米。
- `GeometryEntity::transform` 若为非恒等：**不**参与本 MVP 的几何求值，改为追加警告「实体带非恒等变换，尚未参与求值」。这是显式取舍，避免悄悄算错长度。

## 7. 精度与数值约定

- 全部坐标、半径、长度用 `double`；
- 长度公式为闭式，无离散化误差：
  - `Line`：`|to - from|`
  - `Circle`：`2πr`
  - `Arc`：`r · radians(normalizeDegrees(end - start))`
  - `Polyline`：相邻顶点距离之和，闭合时补末点→首点
- **包围盒取紧凑值**（圆弧按其实际角度范围求极值，而非整圆外接矩形），与 Python 原型偏保守的做法不同，属有意修正；
- 比较容差：几何等值 1e-9；DXF 往返后 1e-9（写 6 位小数，3000 mm 量级下舍入误差 < 1e-6，故实际余量充足）；
- 采样点约定：若将来需要离散，圆与弧的采样点数按弦高误差 `<= 0.05 mm` 计算并向上取整（本 MVP 不写 SPLINE，不涉及）。

## 8. 内建样图与期望值（验收基准）

| # | 图层 | 实体 | 参数 | 长度 (mm) |
|---|---|---|---|---|
| 1 | `OUTLINE` | LINE | (0,0) → (300,0) | 300 |
| 2 | `OUTLINE` | LINE | (300,0) → (300,300) | 300 |
| 3 | `OUTLINE` | LINE | (300,300) → (0,300) | 300 |
| 4 | `OUTLINE` | LINE | (0,300) → (0,0) | 300 |
| 5 | `HOLES` | CIRCLE | 圆心 (450,150)，r = 50 | 100π ≈ 314.159265359 |
| 6 | `HOLES` | ARC | 圆心 (450,350)，r = 100，0° → 90° | 50π ≈ 157.079632679 |
| 7 | `SLOT` | LWPOLYLINE（闭合） | (0,−200) (200,−200) (200,−50) (0,−50) | 700 |

**期望指标**

- 实体总数：**7**（LINE ×4、CIRCLE ×1、ARC ×1、LWPOLYLINE ×1）
- 图层数：**3**（`OUTLINE`、`HOLES`、`SLOT`）
- 总长度：**2371.238898038 mm**（精确值 `1900 + 150π`：直线与折线 1900，圆 100π，弧 50π；已用独立探针程序逐项核对）
- 包围盒：`minX=0, minY=-200, maxX=500, maxY=450` → **宽 500.000000 × 高 650.000000 mm**
- 闭合轮廓数：**1**
- 文档警告：**0**

包围盒极值来源：圆最右 `450+50=500`；圆最上 `150+50=200`；弧 0°→90° 覆盖 x∈[450,550] 但 r=100，故最上取 `350+100=450`；折线最下 `−200`。

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

- `core-msvc`：配置与编译退出码 0、无警告；`100% tests passed`；`xm_core_tests.exe` 输出 `ran 33 test(s), 33 passed, 0 failed`（切片 1 实测值，随切片递增）；
- `mvp-msvc --selftest`：输出含 `entities=7  layers=3  closed=1  warnings=0`、`totalLength=2371.238898038`、`bounds=500.000000 x 650.000000`；
- 界面：窗口显示样图（方框 + 圆 + 四分之一弧 + 下方矩形），指标与实体表与上表一致。

## 11. 依赖与许可证

| 依赖 | 版本 | 许可证 | 说明 |
|---|---|---|---|
| MSVC (Visual Studio 2026 Community) | 14.44 / 14.51 | 专有（免费 Community） | 本机已装；VS 生成器默认选用 14.44 |
| CMake | 3.31（VS 自带） | BSD-3-Clause | 本机已装；不在 PATH 中，预设内可直接调用 |
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
