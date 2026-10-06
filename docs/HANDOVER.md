# 项目交接

- 更新日期：2026-10-06
- 技术主线：C++20 + CMake；Qt 6 UI 尚未开始
- 本轮审查前基线：`92ab6bc`

## 1. 当前真实能力

已完成并通过审查：

- CMake 工程骨架，MSVC `/W4 /permissive- /WX`；
- `Point2d`、角度工具、`Transform2d`；
- `Line`、`Circle`、`Arc`、`Polyline` 与闭式长度；
- `GeometryEntity`、`Layer`、`GeometryDocument`；
- 7 实体/3 图层内建样图；
- 66 个 C++ 测试全部通过。

当前不能做：读取 CDR、读写 DXF、计算完整分析报告、显示 Qt 界面、调用 OCC。README 中的相应条目是目标，不是已交付功能。

## 2. 本轮完成内容

- 审查并接收切片 2；
- 修复极小正扫角被误判为零长度；
- 修复 `GeometryDocument::add()` 异常时提前消耗 ID；
- 补齐公共头文件直接依赖，优化 `unit()` getter；
- 删除早期 Python 转换/分析/UI 原型与临时验证 UI；
- 更新 README、MVP 状态、ADR、代码审查记录和后续开发流程。

详细结论见 `docs/CODE_REVIEW_2026-10-06.md`，执行流程见 `docs/DEVELOPMENT_WORKFLOW.md`。

## 3. 构建与验收

```powershell
$cmakeBin = 'D:\Visual Studio\Microsoft Visual Studio18Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$env:PATH = "$cmakeBin;$env:PATH"

cmake --preset core-msvc
cmake --build --preset core-msvc
ctest --preset core-msvc --output-on-failure
.\build\core-msvc\bin\xm_core_tests.exe
```

预期：配置/编译退出码 0、零警告；CTest 100% 通过；运行器输出 66 个用例全部通过。

## 4. 下一项任务

下一切片是 `Native2dAnalyzer`，只实现：

- 逐实体长度与类型计数；
- 总长度；
- 紧凑包围盒；
- 闭合折线数量；
- 文档警告汇总。

圆弧包围盒必须判断 0°/90°/180°/270° 是否实际落在逆时针扫角内，不能用整圆外接矩形。内建样图验收值为：实体 7、图层 3、总长 `1900 + 150π`、包围盒 `(0,-200)–(550,450)`、闭合数 1、警告 0。

不要在该切片同时加入 DXF、Qt、libcdr、OCC 或端点吸附。

## 5. 后续顺序

1. 切片 3：`Native2dAnalyzer`；
2. 切片 4：最小 ASCII DXF Writer；
3. 切片 5：Reader + 写出/回读一致性；
4. 切片 6：Qt 6 UI + `--selftest`；
5. 阶段 B：libcdr/librevenge；
6. 阶段 C：OCC 与几何修复。

每片完成后先审查再进入下一片。具体门槛与分支约定见 `docs/DEVELOPMENT_WORKFLOW.md`。

## 6. 不可破坏的约束

- 单位统一毫米，坐标与长度使用 `double`；
- 角度使用度、逆时针为正、0° 指向 +X；
- 整圆必须用 `Circle`，`Arc(start == end)` 是零扫角；
- `src/core/**` 不得出现 Qt/libcdr/DXF/OCC 第三方类型；
- 第一阶段不链接 GPL 的 libdxfrw；
- 内部闭式计算与 DXF 六位小数往返使用不同精度门槛；
- 不重新引入平行的 Python 业务实现；一次性探针放 `local/`，结论沉淀为 C++ 测试。

## 7. 待负责人最终决定

- 项目发布许可证；
- Qt 6 的最终动态链接与发布合规方案；
- 阶段 B/C 依赖的固定版本和获取方式。
