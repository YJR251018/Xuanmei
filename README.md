# CDR2DXF Geometry Workbench

一个基于 C++ 的 CDR 工程图转换与几何分析上位机。

项目目标是读取 CorelDRAW 的 `.cdr` 文件，提取其中的二维几何轮廓，将其转换为 CAD 常用的 `.dxf` 格式，并完成图形预览、尺寸统计、轮廓检查和转换质量评估。

> 当前状态：C++20 核心切片 1–5 已完成并通过审查。当前可用能力包括几何值类型、闭式长度、文档模型、分析器，以及本项目 ASCII DXF R12 子集的写出与回读；尚不能读取 CDR，也没有正式 GUI。

## 一、项目背景

激光切割、数控加工、雕刻和机器人轨迹规划等场景经常需要使用 DXF 格式描述二维加工轮廓，而设计图可能以 CDR 格式保存。

直接转换存在以下问题：

- CDR 是复杂的专有矢量格式，不同版本的数据结构存在差异；
- CDR 中的 Bézier 曲线、椭圆、变换和组合对象不能简单对应到 DXF 实体；
- 转换过程中需要保持实际尺寸，而不仅是视觉效果相似；
- 曲线离散化会引入长度误差和轮廓偏差；
- 加工轮廓还需要进行端点连接、闭合检测、重复线清理和微小边过滤。

因此，本项目不仅完成文件格式转换，还将研究曲线映射、几何容差和转换质量评价方法。

## 二、预期功能

### 文件转换

- 导入 CDR 文件；
- 识别 CDR 文件版本和页面信息；
- 提取直线、圆、圆弧、折线和 Bézier 曲线；
- 保留图层、基本颜色和几何变换；
- 导出 ASCII DXF 文件。

### 几何分析

- 统计各类几何实体数量；
- 计算单条曲线及全部轮廓长度；
- 计算图形包围盒和实际尺寸；
- 检测开放轮廓、重复线和微小间隙；
- 对相邻端点进行容差合并；
- 生成转换前后的误差报告。

### 上位机界面

- CDR/DXF 文件导入；
- 二维图形预览；
- 平移、缩放、适应窗口和实体选择；
- 图层显示与隐藏；
- 几何属性和分析结果展示；
- 转换进度、警告和运行日志显示。

## 三、技术路线

```text
CDR 文件
   │
   ▼
libcdr + librevenge
   │  解析路径、对象、变换和页面信息
   ▼
GeometryDocument
   │  项目内部统一二维几何模型
   ├───────────────┐
   ▼               ▼
DXF Writer      Geometry Analyzer
   │               │
   ▼               ▼
DXF 文件        长度、包围盒和警告
   │               ▲
   ▼               │
DXF Reader ────────┘
           ▼
     Qt 6 上位机界面
```

### 主要技术选型

| 模块 | 技术 | 用途 |
|---|---|---|
| 编程语言 | C++20 | 核心开发语言 |
| 用户界面 | Qt 6 Widgets | 桌面上位机和二维交互 |
| 构建系统 | CMake | 跨平台工程构建 |
| CDR 解析 | libcdr + librevenge | 读取 CDR 并接收绘图回调 |
| DXF 输出 | 自研最小 DXF Writer | 输出所需二维实体，控制许可证和格式行为 |
| 几何计算 | Open CASCADE Technology | 后续进行精确曲线、拓扑和容差分析 |
| 自动测试 | 项目内建最小测试框架（阶段 A） | 零外部依赖验证几何与文档模型；后续可迁移到 GoogleTest |

OpenCASCADE 主要承担几何计算，而不作为本项目的默认 DXF 读取器。开源 OCCT 本体并未提供可直接使用的免费 DXF 数据交换模块。

### 构建与测试

本机的 CMake 位于 Visual Studio 安装目录，不在默认 `PATH` 中：

```powershell
$cmakeBin = 'D:\Visual Studio\Microsoft Visual Studio18Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$env:PATH = "$cmakeBin;$env:PATH"

cmake --preset core-msvc
cmake --build --preset core-msvc
ctest --preset core-msvc --output-on-failure
.\build\core-msvc\bin\xm_core_tests.exe
```

预期：编译零警告，CTest 100% 通过，当前测试运行器报告 119 个用例全部通过。

## 四、核心数据模型

项目将建立独立的 `GeometryDocument` 中间层，避免第三方库类型直接进入界面或业务逻辑。

当前内部模型支持的实体包括：

```cpp
Line
Circle
Arc
Polyline
```

`CubicBezier` 留到 CDR 适配阶段设计，目前不是已实现类型。

每个实体至少保存：

- 几何参数；
- 所属图层；
- 原始对象编号；
- 当前变换矩阵；
- 闭合状态；
- 转换警告信息。

统一使用 `double` 保存坐标，内部长度单位统一为毫米。

## 五、目录结构

```text
Xuanmei/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ README.md
├─ docs/
│  ├─ CPP_TECHNICAL_ROUTE.md
│  ├─ DECISIONS.md
│  ├─ MVP_DESIGN.md
│  ├─ DEVELOPMENT_WORKFLOW.md
│  └─ HANDOVER.md
├─ src/
│  └─ core/                 当前实现：纯 C++20 几何、分析器与 R12 Writer/Reader
├─ tests/
│  └─ core/                 零外部依赖 C++ 测试
└─ build/                   本地生成目录（Git 忽略）
```

后续模块按切片逐步加入 `src/core/`；Qt 界面只在核心分析与 DXF 往返链路稳定后加入 `src/app/`。

## 六、开发阶段

### 第一阶段：C++ 最小闭环

- 建立 Qt 6 + CMake 工程；
- 完成内部二维几何模型；
- 实现基础图元预览和长度计算；
- 实现最小 ASCII DXF Writer；
- 使用程序生成的测试几何验证导出结果。

### 第二阶段：CDR 解析

- 构建并接入 libcdr、librevenge；
- 实现 CDR 文件识别与错误处理；
- 将绘图回调转换为内部几何实体；
- 处理页面单位、仿射变换和图层；
- 完成 CDR 到 DXF 的基础转换。

### 第三阶段：几何质量分析

- 接入 OpenCASCADE；
- 实现精确曲线长度计算；
- 检查轮廓连接与闭合状态；
- 修复容差范围内的端点间隙；
- 识别重复线、零长度边和异常小实体。

### 第四阶段：实验与评价

- 建立覆盖不同 CDR 版本和图元类型的测试集；
- 比较不同曲线离散公差下的长度误差、轮廓误差和运行时间；
- 对比直接几何转换与 `CDR → SVG → DXF` 路线；
- 输出可复现的实验数据和转换质量报告。

## 七、精度评价方法

| 指标 | 说明 |
|---|---|
| 包围盒误差 | 转换前后图形整体尺寸差异 |
| 总长度相对误差 | 全部有效轮廓长度的相对变化 |
| 最大轮廓偏差 | 离散曲线与原曲线之间的最大距离 |
| 实体保留率 | 成功转换实体数占可转换实体数的比例 |
| 闭合轮廓保持率 | 转换后仍保持闭合的轮廓比例 |
| 转换耗时 | 不同文件规模下的处理时间 |

## 八、当前仓库状态

当前已经完成：

- 技术可行性调研；
- libcdr、DXF 和 OCC 的职责划分；
- CMake + C++20 工程骨架；
- `Point2d`、`Transform2d` 与四类基础图元；
- 直线、圆、圆弧、折线的闭式长度；
- `GeometryDocument`、图层、警告、连续实体 ID 与内建样图；
- `Native2dAnalyzer`：逐实体指标、计数、总长度、声明闭合数、带实体 ID 的警告和紧凑包围盒；
- `MinimalDxfWriter`：严格 ASCII DXF R12，支持 LINE、CIRCLE、ARC 和传统 POLYLINE 序列，固定六位小数；
- `readDxf` / `readDxfFile`：宽容读取 Writer 子集，区分致命结构错误与可恢复内容问题；
- Writer → Reader 往返验收覆盖实体、图层、逐实体长度、总长度和包围盒；
- 119 个 C++ 单元/验收用例，MSVC `/W4 /permissive- /WX` 零警告通过。

尚未完成：

- Qt 6 正式界面；
- libcdr 原生接入；
- OCC 几何分析；
- 真实 CDR 样本的精度测试。

当前开发环境已有 Visual Studio 2026、MSVC、CMake 4.2.3 与 Qt 6.8.0；vcpkg 尚未安装，阶段 C 接入 OCC 前再配置。

## 九、参考项目

- [LibreOffice/libcdr](https://github.com/LibreOffice/libcdr)：CDR 文件解析库；
- [LibreCAD/libdxfrw](https://github.com/LibreCAD/libdxfrw)：DXF/DWG 读写实现参考；
- [LibreCAD](https://github.com/LibreCAD/LibreCAD)：Qt/C++ 二维 CAD 应用参考；
- [OpenCASCADE/OCCT](https://github.com/Open-Cascade-SAS/OCCT)：CAD 几何与拓扑内核；
- [Qt Graphics View Framework](https://doc.qt.io/qt-6/graphicsview.html)：二维场景和交互框架。

更详细的技术调研见 [`docs/CPP_TECHNICAL_ROUTE.md`](docs/CPP_TECHNICAL_ROUTE.md)。

## 十、许可证说明

项目许可证尚未最终确定。

libcdr 使用 MPL-2.0；libdxfrw 使用 GPL-2.0-or-later。正式发布前需要根据最终依赖方式确认许可证兼容性。本项目第一阶段倾向于自行实现有限范围的 DXF Writer，减少不必要的许可证约束。
