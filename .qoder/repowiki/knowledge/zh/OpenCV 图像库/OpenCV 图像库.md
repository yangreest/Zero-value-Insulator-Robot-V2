---
kind: external_dependency
name: OpenCV 图像库
slug: opencv
category: external_dependency
category_hints:
    - sdk_real_api
scope:
    - '**'
source_files:
    - Insulator_Zero_Value_Detection_Robot.vcxproj
---

### OpenCV 图像库
- 角色：摄像头画面采集、截图、录像等图像处理能力。
- 集成点：Release 配置链接 `opencv_world4120.lib`，头文件路径为 `D:\3rd\opencv_4_12_0\include`，库路径为 `D:\3rd\opencv_4_12_0\lib`。
- 稳定约束：依赖静态打包的 `opencv_world4120`，需保持该 DLL 随 `RunDir\` 分发。
- 验证方式：确认 `opencv_world4120.dll` 与 `opencv_world4120.lib` 版本一致。