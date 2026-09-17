---
kind: external_dependency
name: Qt 6 桌面框架（Widgets/Charts）
slug: qt-6
category: external_dependency
category_hints:
    - framework_behavior
scope:
    - '**'
source_files:
    - Insulator_Zero_Value_Detection_Robot.vcxproj
---

### Qt 6 桌面框架
- 角色：本项目的 UI 与图表渲染框架，Release 配置启用 `core;gui;widgets;charts` 模块，Debug 仅 `core;gui;widgets`。
- 稳定约束：Debug/Release 使用不同 Qt 版本，切换构建配置时需确保对应 Qt 安装存在；图表功能仅在 Release 开启。
- 验证方式：确认 `QtInstall` 指向的 Qt 版本可用，且 `QtModules` 包含 `charts`。