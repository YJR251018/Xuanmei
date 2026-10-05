# 协作约定

本项目采用“开发实现”和“技术评审”分离的协作方式。

## 角色分工

- 项目负责人：确定需求、优先级和最终取舍。
- DeepSeek Harness / WorkBuddy：根据任务说明进行主要代码实现。
- Codex：维护技术路线，审查架构与代码，检查测试覆盖、几何精度和许可证风险。

角色不是权限边界。任何参与者都可以提出方案，但影响格式兼容性、几何精度或公共接口的修改，应先记录设计决定。

## 开发流程

1. 每次只处理一个边界明确的任务。
2. 从主分支创建短期功能分支，例如 `feature/dxf-writer`。
3. 提交说明应描述修改目的，避免只写“update”或“fix”。
4. 合并前至少完成编译检查、相关自动测试和一组可复现样本验证。
5. 由 Codex 进行代码审查；高风险问题解决后再合并。

## 提交任务时应包含

- 目标和不在本次范围内的内容；
- 输入、输出和错误行为；
- 允许使用的依赖；
- 精度或性能要求；
- 验收命令与预期结果。

## 代码审查重点

- CDR、内部模型和 DXF 之间的单位及坐标变换是否一致；
- 曲线转换是否明确误差上限；
- 是否正确处理空文件、损坏文件和不支持的对象；
- 第三方类型是否泄漏到 `GeometryDocument` 之外的模块；
- 测试是否覆盖直线、圆弧、闭合路径、变换和边界条件；
- 新依赖的许可证是否与项目发布方式兼容；
- README 中“已完成”状态是否与代码一致。

## 分支与提交建议

```text
main
├─ feature/cpp-skeleton
├─ feature/geometry-model
├─ feature/dxf-writer
├─ feature/libcdr-importer
└─ feature/occt-analyzer
```

推荐使用 Conventional Commits 风格：

```text
feat(dxf): add ASCII R12 line writer
fix(geometry): preserve arc sweep across zero degrees
test(import): add transformed bezier fixture
docs(route): record unit conversion decision
```

## 样本数据

- 只提交允许公开的脱敏图样；
- 私有 CDR 文件放在 `samples/private/`，该目录已被 Git 忽略；
- 每个公开样本应说明来源、单位、预期实体数和预期包围盒；
- 大文件在确定 Git LFS 策略前不要直接提交。

