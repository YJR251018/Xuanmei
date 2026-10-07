# 代码审查：Qt 6 Widgets MVP

- 日期：2026-10-07
- 分支：`codex/qt-mvp`
- 范围：应用服务、Qt UI、应用测试、UI 烟雾测试、部署配置与文档
- 结论：通过；不存在未关闭的阻断问题

## 1. 边界检查

- `src/core/**` 未引入 Qt；
- `WorkbenchSession` 不依赖 Qt，负责 Writer → Reader → Analyzer 工作流；
- `MainWindow` 只做动作、布局和展示，`PreviewView` 只做绘制/视图交互；
- UI 启动展示的是回读文档，不是直接拿内建样图绕过文件链路；
- 本切片没有加入 CDR、OCC、拓扑修复或几何编辑。

## 2. 已关闭问题

### R1：MSVC 字符集参数冲突

Qt 目标自动增加 `-utf-8`，与工具链已有 `/source-charset:utf-8` 触发 D8016。目标设置
`QT_NO_UTF8_SOURCE`，保留项目已有 UTF-8 源码设置；原生 Windows 截图确认中文显示正确。

### R2：首帧预览缩成中央小点

构造期间 `fitInView` 早于最终 viewport 尺寸。`PreviewView::showEvent` 在控件可见后重新适应窗口；截图确认
550 × 650 mm 全包围盒按比例铺满，圆弧方向为 0° → 90° 的世界坐标逆时针方向。

### R3：只启动进程不能证明 UI 结构正确

增加 `--ui-smoketest`：offscreen 构造真实窗口，检查实体表 7 行、警告 0、窗口可见并成功渲染 PNG；
该命令已进入 CTest。另用原生 `qwindows` 渲染复核中文、布局和几何方向。

### R4：窗口类可能吞并业务流程

加载、导出、样图往返、指标和警告聚合进入 `WorkbenchSession`。App 6 个普通 C++ 用例覆盖成功、致命输入、
Reader/Analyzer 两类警告、再导出、自检成功和输出失败；Qt 窗口不解析组码或计算长度。

### R5：开发部署带入不必要插件

干净构建由 CMake 精确复制 Qt Core/Gui/Widgets 动态库及 `qwindows`、`qoffscreen`，避免通用部署工具
把未链接的 Network/Svg 和软依赖插件带入开发目录；
正式发行仍受 `QT_DEPLOYMENT.md` 清单约束，开发构建不冒充合规发行包。

### R6：扁平输出目录让 Debug 覆盖 Release

PE 依赖复核发现，Debug 和 Release 原先使用同一个可执行文件名；Debug 覆盖后，Release 增量构建可能误判
目标已是最新，留下依赖 `Qt6Cored`/调试 CRT 的假 Release。现在 Debug、RelWithDebInfo、MinSizeRel 分别使用
`d`、`rd`、`s` 后缀，Release 保持文档中的无后缀稳定路径；双配置可共存，不再互相污染。

## 3. 验收

- Core 测试：119/119；
- App 测试：6/6；
- `mvp-msvc` CTest：4/4（Core、App、`--selftest`、UI smoke）；
- `--selftest`：7 实体、3 图层、1 声明闭合、0 警告、总长与包围盒符合冻结基准；
- Release 与 Debug 均已以 `/W4 /permissive- /WX` 构建通过；
- `git diff --check` 无错误。

## 4. 已知边界

- Reader 仍只支持项目 R12 子集，UI 的“打开 DXF”不等于通用 CAD 兼容；
- 没有图层开关、实体编辑/选择属性面板、CDR 导入或 OCC 拓扑功能；
- UI 自动测试验证结构与渲染成功，不做像素金图比较；视觉方向另有本轮原生截图人工复核；
- 发布包、安装器及 LGPL 合规材料尚未制作。
