# Native2dAnalyzer 逻辑复核（2026-10-07 复审）

对提交 `8da17fd`（切片 3）与 `2e9a949`（文档模型整理）的**独立逻辑复核**。
方法：读代码 + 用一次性探针从外部驱动 `Native2dAnalyzer`，覆盖已提交测试未覆盖的输入。

复核基线：`8da17fd`（HEAD），工作区干净。
复核时实测：MSVC `/W4 /permissive- /WX` 零警告；`ctest` 100% 通过；全套 **86/86**；`native_analyzer.` 筛选 **19/19**。

---

## 0. 先说结论

| 项 | 结论 |
|---|---|
| 已提交的测试与文档声称的验收指标 | **全部可复现**，无夸大 |
| 极端坐标下的静默错误（我最担心的一类） | **未发现**：`length()` 溢出为 `inf`，分析器正确告警并排除 |
| 真实缺陷 | **1 个**（§1：退化折线抬高 `closedCount`），边界情形，不影响内建样图 |
| 我提出后又被实验推翻的误报 | **1 条**（§2），已保留记录以免重复判断 |
| 设计/文档问题 | 3 个（见 §3） |
| 测试覆盖缺口 | 2 处（见 §4） |

总体判断：**切片 3 的实现质量高于其文档自述的水平**——圆弧紧凑包围盒、
非有限输入隔离、极小正扫角保留这几处都经受住了额外探针的检验。
唯一确认的缺陷集中在"退化几何是否应计为闭合轮廓"这一口径问题上。

### 后续处置结果

本轮已按 ADR-006 完成处置：公共字段改名为 `declaredClosed` 和
`declaredClosedPolylineCount`，明确它们只反映源数据闭合标志；零长度声明闭合折线会产生警告，
但仍保留在声明计数中。警告改为 `AnalysisWarning`，文档级警告无实体 ID，逐实体与生成警告携带
实体 ID。新增测试覆盖退化闭合折线和相同警告文案的实体归属。

---

## 1. 【已处置】退化折线把 `closedCount` 抬成假阳性

> **性质澄清（重要）**：这不是实现偏离规范的错误，而是**规范本身的口径问题**。
> `docs/MVP_DESIGN.md` L241 明确写着：
>
> > `closedCount` 按闭合 `Polyline` 标志统计；
>
> 代码与这条规范**完全一致**。所以我的意见针对的是**这条规范**：
> 纯按标志位统计，会把零长度退化折线计为闭合轮廓。
> 修它必须**同时改规范与代码**，并配 ADR（影响公共指标语义）。

**位置**：`src/core/native_analyzer.cpp` 的 `isClosed()`

```cpp
if constexpr (std::is_same_v<T, Polyline>) {
    return concrete.closed;      // 只看标志位，不看能否构成轮廓（= 规范 L241）
}
```

**问题**：`isClosed()` 只读 `closed` 标志，不校验折线是否真有轮廓。
空折线、单点折线、所有顶点重合的折线，只要标志位为真，就被计入 `closedCount`。

**复现**（6 条折线，其中只有 2 条是真正的闭合轮廓）：

```
closedCount = 6      ← 期望 2
totalLength = 27.071067811865476     warnings = 0
   id=1 len=0        closed=1 warn=0   ← 空折线
   id=2 len=0        closed=1 warn=0   ← 1 个重合点
   id=3 len=0        closed=1 warn=0   ← 2 个重合点
   id=4 len=0        closed=1 warn=0   ← 3 个重合点
   id=5 len=10       closed=1 warn=0   ← 真实轮廓
   id=6 len=17.07    closed=1 warn=0   ← 真实轮廓
```

**影响**：`closedCount` 对应 `docs/MVP_DESIGN.md` §7 的精度指标 **「闭合轮廓保持率」**。
退化折线会把它抬成假阳性——"6 个闭合轮廓"里 4 个长度为零。下游若是"按闭合轮廓数决定加工路径"，
会凭空多出 4 条空路径。

**与同模块其他图元的口径不一致**：退化圆弧是「零扫角 → 零指标 → **不计入**」，
退化圆是「半径 ≤ 0 → 判为 invalid」。只有退化折线被算作有效闭合轮廓。

**建议**（需同时改 L241 的规范文字与 `docs/MVP_DESIGN.md` §7 的指标定义）：

1. 闭合折线要求**至少 2 个相异顶点**才计入 `closedCount`（否则不计入且不告警——与零扫角圆弧一致）；
2. 或计入但追加生成警告「闭合折线顶点重合，未构成轮廓」，与坏几何口径一致；
3. 或维持现状，但在文档中明确"闭合数含零长度退化轮廓，使用者需自行按长度过滤"。

注意：现状 `len=0` 的闭合折线**不会**告警，所以调用方无从察觉，这是 §4 缺口的来源。

---

## 2. 【已撤回】原"缺陷二：包围盒无效会连带丢弃长度"不成立

**这一节记录一条我自己提出、又用实验推翻的误报**，保留下来以免后人重复同样的判断。

**我原先的判断**：`analyze()` 用同一个 `if` 门控长度入账与包围盒状态——

```cpp
if (boundsStatus == BoundsStatus::invalid || !std::isfinite(entityLength)
    || entityLength < 0.0) {
    addGeneratedWarning(result, metric, kInvalidGeometryWarning);
} else {
    metric.length = entityLength;
    result.totalLength += entityLength;      // 我认为它被 boundsStatus 错误门控
    ...
}
```

我据此推断：可能存在"长度有效但包围盒不可表示"的实体，其长度被静默丢弃。

**实验否证**。穷举 9 个极端坐标用例，统计"`length()` 有限却被分析器丢成 0"的个数：

```
Line(MAX,0 -> -MAX,0)        len()=inf          analyzer=0            hasBounds=0 warn=1
Line(0,0 -> MAX,0)           len()=1.79769e+308 analyzer=1.79769e+308 hasBounds=1 warn=0
Circle(c=MAX, r=1)           len()=6.28319      analyzer=6.28319      hasBounds=1 warn=0
Circle(c=0, r=MAX)           len()=inf          analyzer=0            hasBounds=0 warn=1
Circle(c=MAX, r=MAX)         len()=inf          analyzer=0            hasBounds=0 warn=1
Arc(c=MAX, r=1, 0->90)       len()=1.5708       analyzer=1.5708       hasBounds=1 warn=0
Arc(c=0, r=MAX, 0->90)       len()=inf          analyzer=0            hasBounds=0 warn=1
Polyline(-1e308,0,1e308)     len()=inf          analyzer=0            hasBounds=0 warn=1
Polyline zigzag MAX          len()=inf          analyzer=0            hasBounds=0 warn=1

cases showing finite length dropped: 0
```

**为什么它不会发生**：两个判定在数值上**同源**。

1. `BoundsStatus::invalid` 只在坐标为非有限值时出现。但此时的**长度也必然是非有限的**，
   `!std::isfinite(entityLength)` 单独就会告警——两个条件不会一个真一个假。
2. `BoundsAccumulator::include()` 只做 `min`/`max`，**有限坐标的包围盒永远有限**，
   所以 `isFinite(lower)`/`isFinite(upper)` 那道防线对有限输入不会触发。
3. 有限长度的**求和溢出**是罕见的：它需要中间和超过 `MAX_DOUBLE`，而那通常伴随坐标量级
   本身已使距离溢出。

**结论**：`boundsStatus == BoundsStatus::invalid` 这一支对可构造输入**实际不可达**；
`BoundsStatus` 的 `invalid` 与 `contributed` 判定在当前实现下高度冗余。
这不是缺陷（属防御性编程），而是一处**可以简化、也可以保留**的冗余。

**我此前引用的一段"复现"是错的**：我写过

```
analyzer: Line(0,0 -> 1e308,0) → len=0 warn=0 hasBounds=1 width=0
```

单实体隔离实测为：

```
Line(0,0 -> 1e308,0)   length()=1e+308   analyzer: len=1e+308   hasBounds=1 width=1e+308 warn=0
```

即**长度与包围盒都被正确保留**。我之前把 `P(0,0)` 与 `P(1e308,0)` 构成的普通线段
与 `Polyline{-1e308, …, 1e308}` 的 `inf` 结果混淆了。

**教训**：`inf` 与"有限但大"必须分别验证，不能凭"量级很大"推断溢出。凡涉及边界数值，
都应按量级枚举实测，而不是从读代码推演。

---

## 3. 【低】设计/文档层面的问题

### 3.1 【已处置】警告无法归属到实体

`DocumentMetrics::warnings` 是 `std::vector<std::string>`，文档级警告与逐实体警告混在同一个扁平列表里，
**没有任何字段记录来源实体**。两条不同的实体产生同一条文案时，列表里就是两条相同字符串：

```
warnings=2 (both are the SAME string, entity identity lost)
   "实体包含无效或不可表示的几何值，已从相应统计中忽略"
   "实体包含无效或不可表示的几何值，已从相应统计中忽略"
perEntity: id=1 warn=1 id=2 warn=1
```

面板要显示"第 3 号实体在第 2 图层上半径非法"时，无法从 `warnings` 反查。
`EntityMetric::hasWarning` 只给布尔，不给文案。

**建议**：要么把 `warnings` 改为携带可选实体 id 的结构，要么在文档里明确
"warnings 仅为汇总计数，逐实体详情需查 `perEntity[i].hasWarning`"。

### 3.2 【保留】同一实体可产生两条警告，噪声与顺序

一个几何非法**且**带非恒等变换的实体会连出两条（顺序为变换警告在前、非法几何在后）：

```
negative-radius circle with translation → warnings=2
   "实体带非恒等变换，尚未参与求值"          ← 因为几何已非法，这条其实没有信息量
   "实体包含无效或不可表示的几何值，已从相应统计中忽略"
```

处置决定：保留两条警告。非法几何与未应用变换是两个独立事实；结构化警告已经携带实体 ID，
调用方可以合并展示，但分析器不应静默丢掉其中一种诊断。

### 3.3 `boundsOf(const Arc&)` 里 `end` 是未使用变量

```cpp
const double end = normalizeDegrees(arc.endDegrees);   // 计算并参与 isfinite 校验
...
candidate.include(pointOnCircle(arc.center, arc.radius, end));   // 已使用
```

复核更正：`end` **确实被使用**（作为起点/终点之一），此项**不是问题**，此前判断有误。
保留此条仅作为"复核过程中曾误判、已排除"的记录。

### 3.4 次正规数（subnormal）半径被静默接受

`r = 1e-320`、圆心 `1e-320` 这类次正规输入不触发任何警告，产生次正规长度
（`1.5706346881293228e-320`）。按 `docs/MVP_DESIGN.md` §7.1 内部档，次正规数会丢失有效位，
精度承诺在那里不成立。是否要在分析器层面告警，需项目负责人决定（当前无测试覆盖）。

---

## 4. 测试覆盖缺口

已提交的 19 个分析器用例质量高，0°/90°/180°/270° 四基数方向、跨零、极小正扫角、
退化圆弧、空文档、单点/双点折线、负半径、非有限输入**都有**。以下缺口对应 §1 与 §3：

| 缺口 | 对应 |
|---|---|
| 未断言退化闭合折线（空/单点/重合点）的声明闭合行为 | 已补测试并按 ADR-006 处置 |
| 未断言 `warnings` 与实体的对应关系或去重语义 | 已补相同文案、不同实体 ID 的测试 |

补充建议（非缺口）：极端坐标（`1e308` 量级）目前无用例。本次探针确认其**行为正确**，
但值得补 1~2 个用例把"溢出即告警并排除"锁定为契约，防止将来改包围盒算法时把它改坏。

建议为每条缺口补一个失败用例（先红后绿），这也符合 `docs/DEVELOPMENT_WORKFLOW.md` §1.3
"先补失败用例或明确验收样例，再实现最小代码"。

---

## 5. 复核确认**没有**问题的部分（含一次自我更正）

我在复核过程中一度认为"巨大坐标折线会静默给出零长度且不告警"，**该判断错误，已排除**。
实测：

```
length() Line(-1e308,0 -> 1e308,0)   = inf     finite=0    ← distance() 的差值先溢出
length() Line(-1e308,0 -> 0,0)       = 1e+308  finite=1
analyzer: 该实体 len=0 warn=1                              ← 正确：告警并排除
```

即 `entity.cpp` 的 `std::hypot` 差值溢出后 `length()` 返回 `inf`，
分析器的 `!std::isfinite(entityLength)` 分支正确接住并告警。
**没有静默错误。** 这一条是撤回的误报。

同时确认无误的关键逻辑：

- **圆弧紧凑包围盒**：只纳入起终点 + 实际扫角经过的基数方向，四基数方向、跨零、
  半圆（90°→270°）、以及 `-180°→180°`（归零扫角）均正确；
- **极端坐标**：圆心/半径 `1e308` 溢出 → 告警并排除，不污染总长与包围盒；
- **NaN/无穷角度**：三种组合均告警并排除；
- **混合有效/无效**：有效实体不受影响（`5`、`10` 正确入账）；
- **无效实体仍计入 `countsByKind`**：与"计数实体存在性、不计数其几何"的语义一致；
- **`width`/`height` 在 `hasBounds == false` 时返回 `0.0`**，与文档一致；
- **`2e9a949` 把圆弧长度判定由 `sweep <= 1e-12` 改为 `sweep == 0.0` 是重要修正**：
  旧写法会把扫角 `1e-13` 度的合法短弧静默判为零长度，违反闭式长度契约。
  配套的 `native_analyzer.sub_trillionth_degree_arc_remains_measurable` 用例锁定了该行为
  （实测 `len = 8.9289433549020973e-14`，`warnings = 0`）。

---

## 6. 对两份文档的核对

### 6.1 `CODE_REVIEW_2026-10-07_NATIVE_ANALYZER.md`

| 声称 | 复核结果 |
|---|---|
| MSVC Release 零警告 | ✅ 复现 |
| CTest 100% 通过 | ✅ 复现 |
| 分析器筛选 19/19 | ✅ 复现（`native_analyzer.` 前缀 19 个） |
| 全套 86/86 | ✅ 复现 |
| 样图 7 实体、3 图层、`1900 + 150π`、包围盒 `(0,-200)–(550,450)`、闭合数 1、警告 0 | ✅ 复现（`sample_document_matches_all_acceptance_metrics`） |
| "未发现阻止切片 3 提交的高风险问题" | ✅ 同意；§1 已按 ADR-006 处置，§2 是已撤回误报 |
| 第 1 条"`if constexpr` 后公共 `return` 报不可达代码，改为显式 `else`" | ✅ 与代码一致（`isClosed()` 现为 `if constexpr / else`） |

### 6.2 `docs/DEVELOPMENT_WORKFLOW.md` §3 切片 3 声称

> "实测：20 个分析器相关新增用例（含 1 个文档警告入口用例），全套 86 个 C++ 用例通过。"

核对：`test_native_analyzer.cpp` **19** 个 + `test_document.cpp` 新增「文档警告入口」**1** 个 = **20**，
且 20 + 66（切片 1/2 既有）= 86。**该声称准确**，我最初按 `native_analyzer.` 前缀筛出 19
而一度怀疑此处写错，属统计口径不同，已更正。

### 6.3 `docs/HANDOVER.md`：核对通过（含一次自我更正）

我在复核中一度怀疑 `HANDOVER.md` 会引用已按 ADR-005 删除的 `tools/verify_ui.py`，
**该怀疑错误，已撤回**。实测全文检索：

```
verify_ui   出现 0 次
verify-ui   出现 0 次
tools/      出现 0 次
Python 原型  出现 0 次
```

该文件已随 `8da17fd` 正确同步：基线写明 `2e9a949`，切片 3 记为已完成，
测试数写为 86，并指向 `CODE_REVIEW_2026-10-06.md` / `CODE_REVIEW_2026-10-07_NATIVE_ANALYZER.md`
与本文件所依据的执行流程。**未发现过时引用。**

唯一可补充之处（非错误）：本次复核新发现的 §1、§2 两个缺陷尚未反映在 `HANDOVER.md` 的
"未完成/待处理"清单中；处置后应同步，见 §7。

---

## 7. 处置记录

1. 已先补失败用例，再修改接口与实现；
2. 已用 ADR-006 明确“声明闭合”语义，不把阶段 A 指标冒充拓扑有效轮廓；
3. 已让警告携带可选实体 ID；
4. 重复警告保留为两个独立诊断事实；
5. `BoundsStatus::invalid` 保留为防御性分支，不为它单独增加改动。
