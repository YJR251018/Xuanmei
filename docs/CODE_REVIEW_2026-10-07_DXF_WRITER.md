# MinimalDxfWriter 代码审查记录（2026-10-07）

## 范围

切片 4 `dxf_writer.hpp/.cpp`、Writer 测试、CMake 接入，以及切片 3 二次复审后的
声明闭合指标和结构化警告修正。不包含 DXF Reader、Qt、CDR、OCC 或拓扑修复。

## 契约核对

- 输出声明为 ASCII DXF R12（`AC1009`）；
- 内部 `Polyline` 写为 R12 `POLYLINE/VERTEX/SEQEND`，不混用后续版本的 `LWPOLYLINE`；
- 段顺序为 HEADER、TABLES、ENTITIES、EOF，组码和值严格成对；
- 数值使用 classic locale、定点六位小数，文件入口以 binary 模式保持 LF；
- 图层 `0` 必定存在，显式图层保留颜色/可见性，实体引用但未声明的图层按默认值补入；
- 文档在生成文本前完成验证，验证失败不会截断已有目标文件；
- 非毫米单位、非恒等变换、非有限数值、非正或低于文本分辨率的半径、零扫角或舍入后退化的圆弧、
  空折线、重复图层及不兼容的 R12 图层名均明确失败。

## 审查中修复

1. 原设计把 `AC1009` 与 `LWPOLYLINE` 组合，版本语义冲突。曾评估改用 R2000，但完整 R2000
   还需要句柄、所有者关系和更多对象记录；最终按 ADR-007 选择真正的 R12 传统多段线序列。
2. `kindName(Polyline)` 原本返回格式特定的 `LWPOLYLINE`，会与 R12 Writer 报告矛盾；改为稳定的
   项目类型名 `POLYLINE`。
3. 初版只检查圆的极小半径，漏掉圆弧半径；已补齐并增加回归测试。
4. 接近整圈的圆弧可能在六位小数舍入后起终角相同；现在按实际写出精度量化角度并在输出前拒绝。
5. 文件入口测试最初使用系统临时目录且未及时关闭读取流，不适合受限 Windows 环境；改为测试当前
   工作目录并通过局部读取函数及时释放句柄，测试结束后确认删除成功。
6. 分析器二次复审确认的闭合语义和警告归属问题按 ADR-006 处理：字段明确为“声明闭合”，退化闭合
   折线产生警告，警告携带可选实体 ID。

## 验证

- MSVC Release：`/W4 /permissive- /WX` 零警告；
- MSVC Debug：编译和 CTest 通过；
- CTest：100% 通过；
- 测试运行器：103/103 通过，其中 Writer 专项 15/15；
- `git diff --check` 通过；
- 源码与测试逐行复核完成，未发现阻止提交的问题。

本机未安装 AutoCAD、LibreCAD 或独立 DXF 解析库，因此本轮不声称完成第三方软件互操作验证。
当前结论基于 Autodesk DXF 官方规则、精确组码契约测试和文件级测试。切片 5 Reader 完成后必须补
写出→回读一致性；进入 UI 前还应使用至少一种独立 CAD/解析器打开内建样图导出文件。

## 参考规范

- Autodesk：Writing a DXF File
  https://help.autodesk.com/cloudhelp/2018/ENU/AutoCAD-DXF/files/GUID-5D1DFE5C-94FC-43B7-B535-43001D1662C1.htm
- Autodesk：HEADER Section Group Codes
  https://help.autodesk.com/cloudhelp/2021/ENU/AutoCAD-DXF/files/GUID-A85E8E67-27CD-4C59-BE61-4DC9FADBE74A.htm
- Autodesk：LAYER Group Codes
  https://help.autodesk.com/cloudhelp/2018/ENU/AutoCAD-DXF/files/GUID-D94802B0-8BE8-4AC9-8054-17197688AFDB.htm
