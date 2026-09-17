---
kind: external_dependency
name: 海康威视 XCloud SDK
slug: xcloud-sdk
category: external_dependency
category_hints:
    - vendor_identity
    - sdk_real_api
scope:
    - '**'
source_files:
    - Insulator_Zero_Value_Detection_Robot.vcxproj
    - Camera/XCloud.h
    - Camera/XCloud.cpp
---

### 海康威视 XCloud SDK
- 角色：摄像头设备接入与视频流播放/截图/录像的底层 SDK，封装了海康摄像头的网络协议。
- 集成点：`Camera/XCloud.cpp/.h` 封装 SDK 调用；Release 配置链接 `XCloudSDK.lib`，头文件路径 `D:\3rd\XCloud\include`，库路径 `D:\3rd\XCloud\lib`。
- 稳定约束：项目区分「新摄像头模式」（支持录像）与「旧摄像头模式」（SDK 直显，不支持录像），录像能力取决于摄像头模式。
- 方向：新增摄像头接入时优先复用 `XCloud` 封装层，避免直接调用 SDK 原始接口。