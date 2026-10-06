# C++ 技术路线调研与架构决定

调研日期：2026-10-05

## 1. 调研结论

目前没有发现一个成熟开源项目能直接完成以下完整闭环：

```text
CDR -> 精确二维几何 -> DXF -> 几何检查/长度统计 -> 桌面上位机
```

但存在可组合的成熟组件：

| 模块 | 候选项目 | 结论 |
|---|---|---|
| CDR 解析 | LibreOffice/libcdr | 推荐；C++、MPL-2.0，通过 librevenge 绘图回调输出路径 |
| DXF 读写 | LibreCAD/libdxfrw | 功能成熟，但 GPL-2.0-or-later，需要认真评估项目发布方式 |
| 二维界面 | Qt 6 Widgets / Graphics View | 推荐；适合缩放、选择、图层和测量交互 |
| 几何计算 | Open CASCADE Technology | 推荐作为后续几何内核；不建议把开源 OCCT 当作免费 DXF 读取器 |
| 参考应用 | LibreCAD | 可参考 Qt + DXF 的工程组织和交互，不直接复制业务代码 |

## 2. 一个关键纠正

开源 OCCT 的核心仓库主要提供 STEP、IGES、BREP、STL、OBJ、glTF 等数据交换能力。官方资料将 DXF 放在额外的 Advanced Data Exchange Components 中，该部分是商业组件。因此本项目应采用：

```text
libcdr 负责 CDR
自研或合规的 DXF 层负责 DXF
OCCT 负责曲线、拓扑、容差修复和长度计算
Qt 负责界面与二维预览
```

而不是：

```text
CDR -> DXF -> 直接调用开源 OCC 读取 DXF
```

## 3. 推荐架构

```text
Qt MainWindow
  |
  +-- ApplicationService
        |
        +-- ICdrImporter
        |     +-- LibcdrImporter
        |
        +-- GeometryDocument（项目自己的中间模型）
        |     +-- Line / Circle / Arc
        |     +-- Polyline / CubicBezier
        |     +-- Layer / Transform / Unit
        |
        +-- IDxfWriter
        |     +-- MinimalDxfWriter（第一阶段）
        |     +-- LibdxfrwAdapter（可选、受许可证约束）
        |
        +-- IGeometryAnalyzer
              +-- Native2dAnalyzer（第一阶段）
              +-- OcctAnalyzer（第二阶段）
```

必须先建立自己的 `GeometryDocument`，不能让 Qt、libcdr、DXF 或 OCC 的类型直接贯穿整个程序。这样可以独立测试转换误差，也能替换任意第三方库。

## 4. 分阶段实现

### 阶段 A：可演示的 C++ 最小闭环

- C++20 + CMake + Qt 6 Widgets。
- 手写最小 ASCII DXF R12/R2000 writer，先支持 LINE、ARC、CIRCLE、LWPOLYLINE。
- 先用测试几何验证 DXF 写出、重新读取、长度统计和界面预览。
- 单位统一为毫米，所有坐标使用 `double`。

该阶段暂不依赖 OCC，降低 Windows 构建难度。

### 阶段 B：接入 libcdr

- 使用 `libcdr::CDRDocument::isSupported()` 检查文件。
- 调用 `libcdr::CDRDocument::parse(input, painter)`。
- 实现或封装 `librevenge::RVNGDrawingInterface`，把回调中的路径、变换和图层写入 `GeometryDocument`。
- 三次 Bé塞尔曲线优先写成 DXF SPLINE；若目标设备只接受折线，则使用带公差的自适应离散化。
- 记录 CDR 页面单位到毫米的换算，不能靠图片像素猜测尺寸。

不建议把 CDR 先转 SVG 再读 SVG 作为最终方案，因为中间多一次格式语义和单位转换；它只适合作为早期验证或回退后端。

### 阶段 C：接入 OCC

- 将中间模型转换成 `Geom2d_Curve`，需要拓扑时再构造 `TopoDS_Edge/Wire`。
- 使用 OCC 计算曲线长度、连接关系和包围盒。
- 增加端点吸附、重复线删除、微小边过滤、闭合轮廓检测和 Shape Healing。
- 输出转换质量报告：实体数量、总长度、包围盒、闭合轮廓数及最大偏差。

### 阶段 D：科研项目表达

- 建立不同 CDR 版本、曲线类型和单位的测试集。
- 设计容差实验：不同离散误差下的文件大小、转换耗时和长度误差。
- 对比“直接几何回调”和“CDR->SVG->DXF”两条链路。
- 给出可复现指标，而不只展示软件截图。

## 5. 许可证与工程风险

- libcdr 使用 MPL-2.0，通常比 GPL 更适合模块化集成，但发布时仍需遵守其文件级开源要求。
- LibreCAD/libdxfrw 使用 GPL-2.0-or-later。如果程序分发并直接链接它，整个程序的许可证安排需要谨慎处理。
- 因为第一阶段只需写出有限类型的 DXF，建议先实现独立的最小 DXF writer；读取端也可只实现项目所需子集。
- CDR 是复杂的专有格式。文本、字体、渐变和位图不能自然映射成机械 DXF，应在需求中明确“机械轮廓优先”。

## 6. 参考项目与官方资料

- libcdr：https://github.com/LibreOffice/libcdr
- libcdr 公共解析入口：https://github.com/LibreOffice/libcdr/blob/master/inc/libcdr/CDRDocument.h
- libdxfrw：https://github.com/LibreCAD/libdxfrw
- LibreCAD：https://github.com/LibreCAD/LibreCAD
- OCCT：https://github.com/Open-Cascade-SAS/OCCT
- OCCT 官方组件说明：https://github.com/Open-Cascade-SAS/OCCT/wiki
- Qt Graphics View：https://doc.qt.io/qt-6/graphicsview.html

## 7. Windows 开发环境建议

建议安装：

1. Visual Studio 2022（Desktop development with C++）。
2. CMake 与 Ninja。
3. Qt 6 的 MSVC 2022 64-bit 套件。
4. vcpkg，用于 OCC 和常规依赖；libcdr 在 Windows 上依赖较多，建议单独构建并固定版本。

当前机器已安装 Visual Studio 2026、MSVC、CMake 4.2.3 与 Qt 6.8.0；CMake 不在默认 `PATH`，可使用 VS 自带路径。vcpkg 尚未安装，阶段 C 接入 OCC 前再配置。
