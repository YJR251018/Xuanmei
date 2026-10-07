# 项目交接

- 更新日期：2026-10-07
- 技术主线：C++20 + CMake + Qt 6 Widgets；MVP 闭环已完成
- 当前工作分支：`codex/qt-mvp`

## 1. 当前真实能力

已完成并通过审查：

- CMake 工程骨架，MSVC `/W4 /permissive- /WX`；
- `Point2d`、角度工具、`Transform2d`；
- `Line`、`Circle`、`Arc`、`Polyline` 与闭式长度；
- `GeometryEntity`、`Layer`、`GeometryDocument`；
- 7 实体/3 图层内建样图；
- `Native2dAnalyzer` 的逐实体指标、计数、总长、声明闭合数、可归属警告与紧凑包围盒；
- `MinimalDxfWriter` 的 ASCII DXF R12 写出：LINE、CIRCLE、ARC、POLYLINE、图层和毫米单位；
- `readDxf` / `readDxfFile` 的宽容 ASCII DXF R12 子集读取，以及写出→回读一致性；
- `WorkbenchSession` 应用服务：打开、导出、样图往返和 `--selftest` 共用同一流程；
- Qt 主窗口：Y 轴向上预览、滚轮缩放/拖动、指标、实体表、日志、打开/导出和视图操作；
- Core 119 个、App 6 个 C++ 测试全部通过；`mvp-msvc` 4 个 CTest 目标通过。

当前不能做：读取 CDR、调用 OCC、编辑几何，或完整读取任意第三方 DXF。Reader 只承诺本项目 Writer 子集；README 中其余相应条目是目标，不是已交付功能。

## 2. 最近完成内容

- 审查并接收切片 2；
- 修复极小正扫角被误判为零长度；
- 修复 `GeometryDocument::add()` 异常时提前消耗 ID；
- 补齐公共头文件直接依赖；
- 删除早期 Python 转换/分析/UI 原型与临时验证 UI；
- 更新 README、MVP 状态、ADR、代码审查记录和后续开发流程。
- 完成切片 3 `Native2dAnalyzer`；
- 增加紧凑圆弧包围盒、坏数据隔离、变换告警与警告汇总；
- 为 `GeometryDocument::add()` 增加可选逐实体警告入口；
- 新增 20 个相关用例并完成本切片审查。
- 二次复审后把闭合指标明确为源数据“声明闭合”，退化闭合折线产生警告；
- 警告增加可选实体 ID，解决同文案无法定位来源的问题；
- 完成切片 4 `MinimalDxfWriter`，并修正原设计中 `R12 + LWPOLYLINE` 的版本矛盾；
- Writer 在验证完成后才输出，拒绝非毫米、未应用变换和六位小数下会退化的几何。
- 完成切片 5 `DxfReader`，空文件、二进制、坏组码为致命错误，缺段尾、缺 EOF、损坏字段和未知实体为可恢复警告；
- Reader 不会把 mesh/polyface 或带非零 bulge 的 `POLYLINE` 静默压平成直线折线；
- 往返测试对比实体/图层、逐实体长度、总长度与包围盒，覆盖 CRLF、损坏输入和文件入口。
- 完成切片 6：Qt 6 主窗口和无 Qt 的 `WorkbenchSession` 应用服务；
- 启动路径固定为样图生成 → R12 Writer → Reader → Analyzer → UI，不用原始样图直接冒充回读结果；
- 增加 `--selftest`、offscreen UI 烟雾测试与原生 Windows 截图复核；修复首帧预览缩成小点的问题；
- Qt 使用动态链接，发布前合规清单见 `docs/QT_DEPLOYMENT.md`。

切片 2 审查见 `docs/CODE_REVIEW_2026-10-06.md`，切片 3 审查见
`docs/CODE_REVIEW_2026-10-07_NATIVE_ANALYZER.md`，二次逻辑复核见
`docs/CODE_REVIEW_2026-10-07_ANALYZER_LOGIC_RECHECK.md`，切片 4 审查见
`docs/CODE_REVIEW_2026-10-07_DXF_WRITER.md`，切片 5 审查见
`docs/CODE_REVIEW_2026-10-07_DXF_READER.md`，切片 6 审查见
`docs/CODE_REVIEW_2026-10-07_QT_MVP.md`，执行流程见 `docs/DEVELOPMENT_WORKFLOW.md`。

## 3. 构建与验收

```powershell
$cmakeBin = 'D:\Visual Studio\Microsoft Visual Studio18Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$env:PATH = "$cmakeBin;$env:PATH"

cmake --preset core-msvc
cmake --build --preset core-msvc
ctest --preset core-msvc --output-on-failure
.\build\core-msvc\bin\xm_core_tests.exe

cmake --preset mvp-msvc
cmake --build --preset mvp-msvc
ctest --preset mvp-msvc --output-on-failure
.\build\mvp-msvc\bin\cdr2dxf_workbench.exe --selftest
.\build\mvp-msvc\bin\cdr2dxf_workbench.exe
```

预期：配置/编译退出码 0、零警告；Core 119/119、App 6/6；`mvp-msvc` CTest 4/4；`--selftest` 输出 7 个实体、3 个图层、0 警告和冻结的长度/包围盒。

## 4. 下一项任务

下一阶段是 libcdr/librevenge 适配器。先固定依赖版本、构建方式、页面/单位/变换回调契约和可公开的最小 CDR fixture，
再把第三方回调映射到 `GeometryDocument`；不得让 libcdr 类型进入 core 或 UI，也不在同一切片加入 OCC 拓扑修复。

## 5. 后续顺序

1. 切片 4：最小 ASCII DXF Writer（已完成并评审）；
2. 切片 5：Reader + 写出/回读一致性（已完成并评审）；
3. 切片 6：Qt 6 UI + `--selftest`（已完成并评审）；
4. 阶段 B：libcdr/librevenge（下一项）；
5. 阶段 C：OCC 与几何修复。

每片完成后先审查再进入下一片。具体门槛与分支约定见 `docs/DEVELOPMENT_WORKFLOW.md`。

## 6. 不可破坏的约束

- 单位统一毫米，坐标与长度使用 `double`；
- 角度使用度、逆时针为正、0° 指向 +X；
- 整圆必须用 `Circle`，`Arc(start == end)` 是零扫角；
- `src/core/**` 不得出现 Qt/libcdr/DXF/OCC 第三方类型；
- 第一阶段不链接 GPL 的 libdxfrw；
- R12 Polyline 必须写为 `POLYLINE/VERTEX/SEQEND`，不得写 `LWPOLYLINE`；
- Reader 遇到 mesh/polyface 标志或非零 bulge 时必须告警并跳过，不得静默改变曲线几何；
- UI 只能消费 `WorkbenchSession`/core 公共接口，不得复制 DXF 解析、长度或包围盒算法；
- Qt 保持动态链接；开发部署产物在完成 `QT_DEPLOYMENT.md` 清单前不得作为正式发行包；
- 内部闭式计算与 DXF 六位小数往返使用不同精度门槛；
- 不重新引入平行的 Python 业务实现；一次性探针放 `local/`，结论沉淀为 C++ 测试。

## 7. 待负责人最终决定

- 项目发布许可证；
- Qt 6 正式发行时的对应源码归档/书面要约与合规复核；
- 阶段 B/C 依赖的固定版本和获取方式。
