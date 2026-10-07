# 后续开发流程

本流程适用于当前 C++ 主线。目标是让每个切片都能独立构建、测试、审查和回退，避免把格式读写、几何算法与 UI 混在同一次提交中。

## 1. 单个切片的标准流程

1. 在 `main` 最新状态上建立短期分支，例如 `feature/native-analyzer`。
2. 在动代码前写清任务契约：输入、输出、错误行为、精度、依赖、非目标和验收命令。
3. 先补失败用例或明确验收样例，再实现最小代码。
4. 保持 `src/core/**` 只依赖 C++ 标准库；第三方类型只能出现在适配层。
5. 本地通过编译、CTest 和测试运行器后再进行代码审查。
6. 修完审查问题后提交；提交信息说明目的，推送分支，再合并到 `main`。
7. 同步更新 `README.md`、`docs/MVP_DESIGN.md` 和本文件中的状态；不要让“计划功能”看起来像“已实现功能”。

## 2. 每次提交的质量门槛

```powershell
$cmakeBin = 'D:\Visual Studio\Microsoft Visual Studio18Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$env:PATH = "$cmakeBin;$env:PATH"

cmake --preset core-msvc
cmake --build --preset core-msvc
ctest --preset core-msvc --output-on-failure
.\build\core-msvc\bin\xm_core_tests.exe
git diff --check
git status --short
```

必须同时满足：

- MSVC `/W4 /permissive- /WX` 零警告；
- 相关自动测试全部通过；
- 精度断言选择正确：内部闭式计算使用绝对误差，DXF 文本往返按舍入传播单独设阈值；
- 新文件已纳入 diff，生成物、私有样本和凭据未进入 Git；
- 公共接口、许可证或格式行为发生变化时，已追加 ADR。

## 3. 推荐的后续切片

### 切片 3：`Native2dAnalyzer`（已完成）

范围：逐实体长度、类型计数、总长度、紧凑包围盒、闭合折线数；不做 DXF、Qt、端点吸附或拓扑修复。

关键测试：

- 0°、90°、180°、270° 基数方向分别落在/不落在圆弧扫角内；
- 跨 0° 圆弧；极小正扫角；退化圆弧；
- 空文档、单点/双点折线、负半径和非有限输入的明确行为；
- 内建样图结果：7 个实体、总长 `1900 + 150π`、包围盒 `(0,-200)–(550,450)`、闭合数 1。

实测：20 个分析器相关新增用例（含 1 个文档警告入口用例），全套 86 个 C++ 用例通过。

### 切片 4：最小 ASCII DXF Writer

范围：R12/R2000 兼容的 HEADER/TABLES/ENTITIES/EOF 骨架与四类实体；固定小数格式和错误返回。不得接入 GPL 的 libdxfrw。

### 切片 5：最小 ASCII DXF Reader

范围：只解析项目 Writer 产生的子集，明确处理空文件、奇数组码、未知实体与损坏数值；加入写出→回读一致性测试。

### 切片 6：Qt 6 界面

只有切片 3–5 稳定后开始。UI 只消费应用服务和 `GeometryDocument`，不得自行解析 DXF 或重复实现长度/包围盒算法。实现前先确认 Qt 动态链接与发布许可证方案。

### 阶段 B/C

- 阶段 B：libcdr/librevenge 适配器，把第三方回调转换为 `GeometryDocument`；
- 阶段 C：OpenCASCADE 分析器，处理精确曲线、拓扑和 Shape Healing；
- vcpkg 在阶段 C 前再引入，避免提前扩大构建面。

## 4. 目录约束

```text
src/core/       纯 C++20 领域模型、算法、最小 DXF 子集
src/app/        Qt 应用层（切片 6 才创建）
tests/core/     与 core 一一对应的测试
tests/fixtures/ 可公开、脱敏且有期望值说明的样本
docs/           契约、ADR、审查与交接
build/          可删除、可重建，不提交
local/          临时诊断产物，不提交
```

不要重新引入平行的 Python 业务实现。一次性探针可放 `local/`，验证结论必须沉淀为 C++ 测试或文档。

## 5. 完成定义

一个切片只有在以下条件全部满足时才算完成：接口与非目标清楚、实现通过质量门槛、关键失败路径有测试、审查问题已关闭、文档状态已同步、分支已推送且提交可独立回退。
