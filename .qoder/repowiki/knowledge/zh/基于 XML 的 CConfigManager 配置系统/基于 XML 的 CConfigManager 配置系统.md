---
kind: configuration_system
name: 基于 XML 的 CConfigManager 配置系统
category: configuration_system
scope:
    - '**'
source_files:
    - Insulator_Zero_Value_Detection_Robot/Config/ConfigManager.h
    - Insulator_Zero_Value_Detection_Robot/Config/ConfigManager.cpp
    - Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp
    - Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.h
    - Insulator_Zero_Value_Detection_Robot/Tools/tinyxml2.h
---

## 1. 使用的系统与方案

本项目采用**自实现的 XML 配置文件 + tinyxml2 解析器**作为运行时配置系统，核心由 `Insulator_Zero_Value_Detection_Robot/Config/ConfigManager.{h,cpp}` 提供。应用启动时在 UI 主窗口 `InitParam()` 中通过 `WHSD_Tools::GetAbsolutePath("Config.xml")` 定位并读取配置文件，失败时记录日志提示检查文件。

- **持久化格式**：XML（根节点为 `<Config>`），使用内嵌的 `Tools/tinyxml2.h` 进行读写。
- **内存模型**：每个配置段对应一个 POD 风格结构体（`CControlBoardConfig`、`CCameraConfig`、`CNewTicketConfig`、`CNewReportConfig`），统一由 `CConfigManager` 持有实例。
- **序列化方向**：Read/Write 双向支持；写回时将枚举值通过 `m_vecLoopType`、`m_vecBunchType`、`m_vecCurrentType` 等静态转换函数序列化为中文文本（如“同塔单回”、“单联”、“交流”）。
- **复杂字段**：工单的测量数据 `m_mapTicketMearData` 以 JSON 字符串形式嵌入 XML 的 `<TicketMearData>` 节点，读入时用 `QJsonDocument::fromJson` 解析，写出时用 `QJsonDocument::toJson(QJsonDocument::Compact)` 压缩后写入。

## 2. 关键文件与包

| 文件 | 作用 |
|---|---|
| `Insulator_Zero_Value_Detection_Robot/Config/ConfigManager.h` | 定义所有配置结构体及 `CConfigManager` 接口（`Read` / `Write`） |
| `Insulator_Zero_Value_Detection_Robot/Config/ConfigManager.cpp` | XML 解析/生成实现，包含全部字段映射逻辑 |
| `Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp` | 应用入口调用点：构造 `CConfigManager`、读取 `Config.xml`、将设备 IP/端口/心跳等注入协议层和相机模块 |
| `Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.h` | 声明成员 `CConfigManager* m_pConfig` |
| `Insulator_Zero_Value_Detection_Robot/Tools/tinyxml2.h/.cpp` | 第三方 XML 库（随项目分发） |
| `Insulator_Zero_Value_Detection_Robot/RunDir/` | 运行期目录（应用在此目录下查找/写入 `Config.xml`） |

## 3. 架构与约定

### 3.1 配置段与 XML 结构
`CConfigManager` 将配置分为四个独立段，分别对应 XML 中的同级子节点：

- `<DeviceControlBoard>`：控制板连接参数（`Ip`、`Port`、`DeviceHeartBeat`、`FactoryMode`）、探针角度（`UpAngle`、`DownAngle`、`UpAngle2`）、电机速度（`WalkMotorSpeed`、`ServoSpeed`）、绝缘阈值（`InsuThreshold`）。
- `<Camera>`：三目相机 IP（`Left`、`Mid`、`Right`）、是否新款相机（`NewCamera`）、是否使用主码流（`UseMainSp`）、RTSP 地址（`MainRtsp`、`SubRtsp`）。
- `<NewTicketList>`：多条目列表，每个 `<Size>` 代表一条工单配置，包含线路名、杆塔号、串数类型、回路数、交直流类型、起止时间、检测人员/单位、备注以及 JSON 格式的测量数据。
- `<NewReportList>`：报告模板列表，每个 `<Size>` 包含报告编号、作业地点、检测人员、检测单位。

### 3.2 加载流程
1. `Insulator_Zero_Value_Detection_Robot::InitParam()` 创建 `CConfigManager`。
2. 调用 `Read(WHSD_Tools::GetAbsolutePath("Config.xml"))` 解析文件。
3. 解析成功后，UI 层直接访问 `m_memControlBoardConfig`、`m_memCCameraConfig`、`m_vecNewTicketConfig`、`m_vecNewReportConfig` 这些公共成员，将其值传递给 `IDeviceCom`、`CWHSDControlBoardProtocol`、相机模块等子系统。
4. 若解析失败（返回 `false`），仅记录日志，不中断初始化。

### 3.3 保存流程
通过 `Write(filePath)` 重建整个 XML 文档树，顺序写入上述四个段，最后调用 `doc.SaveFile()` 落盘。保存失败时返回 `false`（注释指出可按规范添加日志或异常处理）。

### 3.4 默认值与容错
- `CControlBoardConfig` 构造函数对 `DeviceHeartBeat` 赋默认值 `200`，`FactoryMode` 默认 `false`。
- Read 过程中对缺失节点采取“跳过”策略：`FirstChildElement` 为空则直接忽略该字段，不会报错。
- 布尔字段通过 `QueryIntText` 后判断 `> 0` 来解析。
- 枚举字段通过 `stoi` 或字符串匹配解析，未知值回退到默认枚举（如 `LoopType::eOne`、`CurrentType::eAC`、`BunchType::eSingle`）。

## 4. 约定与约束

- **配置文件位置**：通过 `WHSD_Tools::GetAbsolutePath("Config.xml")` 解析相对路径，实际位于应用运行目录（`RunDir` 同级）。调用方负责传入完整路径。
- **单一来源**：所有设备与业务配置集中由 `CConfigManager` 管理，其他模块通过其公共成员直接读取，不存在分散的环境变量或注册表读取逻辑。
- **无热重载**：代码中未发现监听文件变更或重新加载配置的机制；修改 `Config.xml` 需重启应用生效。
- **无环境变量覆盖**：未观察到从进程环境、`.env` 文件或命令行参数覆盖配置项的逻辑。
- **无加密/鉴权**：配置文件以明文 XML 存储，不包含密码或密钥字段。
- **向后兼容**：新增字段在 Read 中以可选节点方式处理，旧版 XML 缺少新字段时仍可正常加载（字段保持默认值）。
- **JSON 嵌套**：`TicketMearData` 字段约定必须为合法 JSON 对象，否则解析错误会被忽略，该字段保持空对象。
- **枚举可逆**：`CNewTicketConfig` 为每个枚举提供成对的字符串↔枚举转换静态方法，保证 XML 中可读的中文文本与内存枚举一致。