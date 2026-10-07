# Qt 6 动态部署与发布前清单

本文件是工程清单，不是法律意见。当前构建只用于开发和验收；在下列事项完成并由项目负责人确认前，
`build/` 中的可执行文件和 DLL 不应作为正式发行包对外分发。

## 当前工程状态

- Qt 版本：6.8.0，MSVC 共享库构建；
- 直接链接模块：Qt Core、Gui、Widgets；
- 平台插件：`qwindows`（正常窗口）与 `qoffscreen`（自动渲染测试）；
- CMake 在构建后部署动态库，未静态链接 Qt；
- 没有修改 Qt 源码。

## 正式分发前必须完成

1. 固定实际发行使用的 Qt 精确版本、模块、插件和第三方组件清单；优先使用 Qt 6.8 起提供的 SBOM 核对。
2. 随包提供适用的 LGPLv3/GPLv3 文本、Qt 使用声明及实际随包第三方组件的许可证/致谢。
3. 向接收者提供所用 Qt 库的完整对应源码（含修改）；或者提供由发行方控制、内容和期限经法律复核的书面获取要约。不能只放一个 Qt 官网链接替代。
4. 提供替换 Qt DLL/插件和运行替换版本的说明；不得通过签名、DRM、安装器或设备限制阻止接口兼容库替换及 LGPL 允许的逆向工程。
5. 核查最终项目许可证、用户协议、安装渠道和设备策略不限制 LGPL 赋予的权利。应用商店或封闭设备需要单独审查。
6. 对发行包做一次“替换 DLL 后可启动”的演练，并保存依赖清单、源码归档/要约和许可证包作为发布证据。
7. 若无法完整满足上述要求，停止 LGPL 发行路径并评估 Qt 商业许可。

依据：Qt 官方的 [GPL/LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations)、
[Qt open-source licensing FAQ](https://www.qt.io/faq/qt-open-source-licensing) 与
[Qt licensing overview](https://doc.qt.io/qt-6/licensing.html)。最终方案应由熟悉目标发行地区和渠道的专业人员复核。
