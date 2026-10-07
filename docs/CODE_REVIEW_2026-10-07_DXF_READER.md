# 代码审查：ASCII DXF R12 Reader

- 日期：2026-10-07
- 分支：`codex/dxf-reader`
- 范围：`dxf_reader`、Reader 测试、构建接入与相关文档
- 结论：通过；本切片不存在未关闭的阻断问题

## 1. 审查目标

本次只实现项目 Writer 所产生的 ASCII DXF R12 子集，不宣称是通用 DXF 解析器。审查重点是：

- 致命结构错误不会返回一个伪成功文档；
- 可恢复内容错误有行号或清楚的汇总警告；
- 未知实体或损坏 `POLYLINE` 不会吞掉后续实体；
- Writer → Reader 后，实体、图层和分析指标保持一致；
- `src/core/**` 仍然只依赖 C++20 标准库。

## 2. 已关闭问题

### R1：往返测试只比较逐实体长度，不能证明位置正确

已补充图层名称/颜色/可见性，以及分析器的总长度和四个包围盒边界。这样即使实体长度相同但位置被读错，验收也会失败。

### R2：对最小整数色号直接 `abs` 存在溢出风险

先扩展到 `long long` 再取幅值，超出 ACI `1..255` 时回退到 7 并告警；`-2147483648` 已有回归用例。

### R3：mesh/polyface 和 bulge 会被误读为普通直线折线

Reader 现在拒绝带非基础标志位的 `POLYLINE`，并在任一 `VERTEX` 含损坏或非零 bulge 时跳过整个折线。后续实体仍继续解析。

### R4：Reader 的失败边界需要与文件入口区分

文件无法打开是环境错误，`readDxfFile` 抛含路径的 `std::runtime_error`；文件内容错误统一进入 `DxfReadResult`。该边界已由测试覆盖。

## 3. 验证覆盖

Reader 专项 16 个用例，覆盖：

- Writer 内建样图完整往返和 CRLF；
- 空文件、二进制、坏组码、悬空组码、坏输入流；
- 缺 ENDSEC/EOF、未知实体汇总、缺失/损坏数值；
- 传统 POLYLINE、缺 SEQEND、mesh/polyface、bulge；
- 图层颜色、隐藏状态、极端整数和文件入口。

最终质量门槛要求 Release 与 Debug 均构建/CTest 通过，Release 为 `/W4 /permissive- /WX`，测试运行器为 119/119，且 `git diff --check` 无错误。

## 4. 已知边界

- 未做第三方 CAD 软件或独立 DXF 库的互操作验证；
- 不支持 `LWPOLYLINE`、SPLINE、INSERT、TEXT、块、任意 OCS/3D 或完整 DXF 版本；
- 对非 R12 版本只告警并尝试读取相同子集；
- Reader 会把支持字段的损坏值回退为零，调用方必须展示 warnings，不能把“可恢复”解释成“无损”。

上述边界与 ADR-008、`MVP_DESIGN.md` 和交接文档一致。
