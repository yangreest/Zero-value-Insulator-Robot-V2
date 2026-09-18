# UI模块

<cite>
**本文引用的文件**
- [main.cpp](file://Insulator_Zero_Value_Detection_Robot/main.cpp)
- [Insulator_Zero_Value_Detection_Robot.h](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.h)
- [Insulator_Zero_Value_Detection_Robot.cpp](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp)
- [Insulator_Zero_Value_Detection_Robot.ui](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.ui)
- [contentwidget.h](file://Insulator_Zero_Value_Detection_Robot/UI/contentwidget.h)
- [contentwidget.cpp](file://Insulator_Zero_Value_Detection_Robot/UI/contentwidget.cpp)
- [modeldatawidget.h](file://Insulator_Zero_Value_Detection_Robot/UI/modeldatawidget.h)
- [modeldatawidget.cpp](file://Insulator_Zero_Value_Detection_Robot/UI/modeldatawidget.cpp)
- [modeldatamodel.h](file://Insulator_Zero_Value_Detection_Robot/UI/modeldatamodel.h)
- [modeldatamodel.cpp](file://Insulator_Zero_Value_Detection_Robot/UI/modeldatamodel.cpp)
- [NewTicketDialog.h](file://Insulator_Zero_Value_Detection_Robot/UI/NewTicketDialog.h)
- [NewReportDialog.h](file://Insulator_Zero_Value_Detection_Robot/UI/NewReportDialog.h)
- [WHSDControlBoradProtocol.h](file://Insulator_Zero_Value_Detection_Robot/Protocol/WHSDControlBoradProtocol.h)
</cite>

## 更新摘要
**所做更改**
- 新增舵机到位反馈处理机制章节，详细说明 `CallBack_ServoArrival()` 和 `OnServoArrivalFeedback()` 方法
- 更新多线程通信策略部分，强调 `QMetaObject::invokeMethod()` 的使用
- 完善测量流程状态机，包含舵机到位事件的完整处理流程
- 更新协议回调注册机制说明

## 目录
1. [简介](#简介)
2. [项目结构](#项目结构)
3. [核心组件](#核心组件)
4. [架构总览](#架构总览)
5. [详细组件分析](#详细组件分析)
6. [依赖关系分析](#依赖关系分析)
7. [性能与并发](#性能与并发)
8. [故障排查指南](#故障排查指南)
9. [结论](#结论)
10. [附录：UI扩展开发指南与最佳实践](#附录ui扩展开发指南与最佳实践)

## 简介
本模块为绝缘子零值检测机器人V2的图形用户界面（UI）层，基于Qt框架实现。主窗口类负责整体布局、事件绑定、设备通信回调处理、测量流程控制、定标流程控制、告警展示、工单/报告管理以及图表与表格的动态更新。通过信号槽机制将协议线程与UI线程解耦，保证多线程环境下UI更新的线程安全与流畅性。**最新增强**：新增了舵机到位反馈处理机制，支持精确的探针位置控制和异常处理。

## 项目结构
UI模块位于工程目录的 UI 子目录下，包含主窗口、自定义图表控件、数据模型、对话框等。入口程序创建并显示主窗口，随后进入事件循环。

```mermaid
graph TB
A["main.cpp<br/>创建应用与主窗口"] --> B["Insulator_Zero_Value_Detection_Robot<br/>主窗口"]
B --> C["ContentWidget<br/>图表基类"]
B --> D["ModelDataWidget<br/>表格+曲线视图"]
B --> E["ModelDataModel<br/>表格数据模型"]
B --> F["NewTicketDialog<br/>新建/编辑工单"]
B --> G["NewReportDialog<br/>新建/编辑报告"]
B --> H["设备通信/协议层<br/>IDeviceCom / Protocol"]
B --> I["配置/日志/工具<br/>Config/Log/Tools"]
H --> J["舵机到位回调<br/>RegisterServoArrivalCallBack"]
```

**图示来源**
- [main.cpp:11-23](file://Insulator_Zero_Value_Detection_Robot/main.cpp#L11-L23)
- [Insulator_Zero_Value_Detection_Robot.h:26-384](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.h#L26-L384)
- [WHSDControlBoradProtocol.h:253-257](file://Insulator_Zero_Value_Detection_Robot/Protocol/WHSDControlBoradProtocol.h#L253-L257)

**章节来源**
- [main.cpp:11-23](file://Insulator_Zero_Value_Detection_Robot/main.cpp#L11-L23)
- [Insulator_Zero_Value_Detection_Robot.ui:1-200](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.ui#L1-L200)

## 核心组件
- 主窗口 Insulator_Zero_Value_Detection_Robot
  - 职责：初始化UI与参数、绑定按钮与定时器、处理设备心跳与传感器数据、驱动测量与定标流程、维护工单/报告数据、更新图表与表格、处理键盘与手柄输入、截图与录像。
  - 关键状态：测量步骤 m_nMeasureStep、定标步骤 m_eCalibStep、探针到位标志 m_bProbeArrived、录制标志 m_bRecording、连接状态 m_bControlBroadConnected 等。
  - 关键定时器：m_pTimer（设备状态轮询）、m_pTimerInput（手柄输入轮询）、m_pProbeWaitTimer（探针到位轮询）、m_pMeasureTimeoutTimer（测量结果超时）、m_pCalibTimeoutTimer（定标等待超时）。
- 自定义控件 ContentWidget
  - 职责：提供可复用的图表容器与加载流程，支持错误提示与默认ChartView管理。
- 数据可视化 ModelDataWidget + ModelDataModel
  - 职责：根据表头动态生成表格列，按行填充测量值；同步绘制折线图；支持重测删除最近值；自适应宽度与坐标轴范围。
- 对话框 NewTicketDialog / NewReportDialog
  - 职责：收集工单/报告信息，通过信号通知主窗口进行新增或修改。

**章节来源**
- [Insulator_Zero_Value_Detection_Robot.h:26-384](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.h#L26-L384)
- [contentwidget.h:12-32](file://Insulator_Zero_Value_Detection_Robot/UI/contentwidget.h#L12-L32)
- [modeldatawidget.h:20-49](file://Insulator_Zero_Value_Detection_Robot/UI/modeldatawidget.h#L20-L49)
- [modeldatamodel.h:12-39](file://Insulator_Zero_Value_Detection_Robot/UI/modeldatamodel.h#L12-L39)
- [NewTicketDialog.h:10-35](file://Insulator_Zero_Value_Detection_Robot/UI/NewTicketDialog.h#L10-L35)
- [NewReportDialog.h:10-31](file://Insulator_Zero_Value_Detection_Robot/UI/NewReportDialog.h#L10-L31)

## 架构总览
UI层采用"主窗口协调 + 自定义控件封装 + 信号槽跨线程"的分层设计：
- 主窗口作为控制器，负责业务编排与状态机推进。
- 自定义控件专注于数据呈现与交互细节。
- 协议/设备通信在独立线程中运行，通过信号槽将结果投递到UI线程，避免直接操作UI引发的线程安全问题。

```mermaid
sequenceDiagram
participant Dev as "设备/协议线程"
participant Main as "主窗口(Insulator...)"
participant Chart as "ModelDataWidget"
participant Log as "日志系统"
participant Servo as "舵机到位回调"
Note over Dev,Servo : 舵机到位反馈流程
Dev->>Main : CallBack_ServoArrival() (协议线程)
Main->>Main : QMetaObject : : invokeMethod() 切到UI线程
Main->>Main : OnServoArrivalFeedback() (UI线程)
alt 正常到位(0x01)
Main->>Main : TriggerMeasureAndArm()
else 超时异常(0x00)
Main->>Main : AbortMeasure()
end
Note over Dev,Main : 其他回调流程
Dev->>Main : 回调传感器数据/测量结果(协议线程)
Main->>Main : 判断是否处于定标流程
alt 定标流程
Main->>Main : 发送信号 CalibMeasureValueSignal (QueuedConnection)
Main-->>Main : On_CalibMeasureValue 槽更新定标状态
else 工单测量流程
Main->>Main : 记录JSON测量数据
Main->>Main : invokeMethod 切到UI线程
Main->>Chart : appendValue(表头, 值)
Main->>Log : 写入设备日志
Main->>Main : 触发告警 AddAlarm(如低于阈值)
end
```

**图示来源**
- [Insulator_Zero_Value_Detection_Robot.cpp:836-893](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L836-L893)
- [Insulator_Zero_Value_Detection_Robot.cpp:675-753](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L675-L753)
- [Insulator_Zero_Value_Detection_Robot.h:36-127](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.h#L36-L127)

## 详细组件分析

### 主窗口 Insulator_Zero_Value_Detection_Robot
- 界面布局
  - 使用 Qt Designer 生成的 .ui 文件定义主窗口布局，包含顶部导航按钮、视频区域、标签页、表格与图表区域等。
  - 启动时隐藏标题栏、最大化窗口、设置分割器初始比例（视频区:标签页=2:1），并对控件样式进行统一美化。
- 事件处理机制
  - 定时器：每100ms轮询设备状态（传感器状态、电量、结果），每10ms读取手柄输入并下发控制指令。
  - 按钮/下拉框：通过 BindAction 集中 connect 绑定到对应槽函数，完成开关设备、截图、录屏、保存参数、测量、定标等操作。
  - 键盘与手柄：keyPressEvent/keyReleaseEvent 与 On_timerInput_timeout 配合，实现探针角度、行走、停止、开始测量等动作。
- 状态管理
  - 测量流程状态机：m_nMeasureStep 表示当前步骤（空闲/内测/外侧），配合探针到位轮询与结果超时控制流程推进。
  - 定标流程状态机：ECalibStep 枚举描述步骤顺序，依靠信号槽接收下位机应答/回报推进流程。
  - 设备状态快照：GetHeartBeatSnapshot 加锁返回心跳数据副本，UI线程读取需持锁避免读到写一半的数据。
- 多线程UI更新策略
  - 协议回调在协议线程，通过 emit 信号 + QueuedConnection 切换到UI线程执行槽函数，确保UI更新线程安全。
  - **新增**：舵机到位回调使用 `QMetaObject::invokeMethod()` 实现跨线程通信，避免直接操作UI对象。
  - 对共享资源（如测量数据、录制帧缓存）使用 std::mutex 与 std::atomic 保护。
- 截图与录像
  - 截图：captureCurrentWindow/screenshot相关逻辑，文件名含内测/外侧标记与时间。
  - 录像：m_bRecording 标志位由UI线程写，取流线程读；帧缓存用 clone 避免复用缓冲区，停止后统一编码成视频文件。

```mermaid
flowchart TD
Start(["开始"]) --> ProbeMove["下发探针移动指令"]
ProbeMove --> WaitProbe["启动到位轮询定时器"]
WaitProbe --> Arrived{"收到舵机到位反馈?"}
Arrived -- 是 --> CheckResult{"检查到位结果"}
CheckResult -- 正常(0x01) --> Trigger["触发测量 + 截图"]
CheckResult -- 超时(0x00) --> Abort["异常结束: 复原探针/关闭弹窗/恢复按钮/记录告警"]
Arrived -- 否且超时 --> TimeoutAbort["兜底超时: 中止测量流程"]
Trigger --> ArmTimeout["启动测量结果超时定时器"]
ArmTimeout --> Result{"收到测量结果?"}
Result -- 是 --> Update["更新表格/曲线/告警/工单时间"]
Result -- 否且超时 --> Abort
Update --> Next{"双联需要外侧吗?"}
Next -- 是 --> MoveNext["移动到外侧位置"]
Next -- 否 --> End(["结束"])
MoveNext --> WaitProbe
TimeoutAbort --> End
```

**图示来源**
- [Insulator_Zero_Value_Detection_Robot.h:188-197](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.h#L188-L197)
- [Insulator_Zero_Value_Detection_Robot.cpp:413-559](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L413-L559)
- [Insulator_Zero_Value_Detection_Robot.cpp:836-893](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L836-L893)

**章节来源**
- [Insulator_Zero_Value_Detection_Robot.h:26-384](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.h#L26-L384)
- [Insulator_Zero_Value_Detection_Robot.cpp:88-405](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L88-L405)
- [Insulator_Zero_Value_Detection_Robot.cpp:413-800](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L413-L800)

### 舵机到位反馈处理机制
**新增功能**：实现了完整的舵机到位反馈处理机制，包括回调注册、跨线程通信和异常处理。

- 回调注册
  - 在 `BindAction()` 中通过 `RegisterServoArrivalCallBack()` 注册回调函数。
  - 回调函数 `CallBack_ServoArrival()` 运行在协议线程，负责跨线程通信。
- 跨线程通信
  - 使用 `QMetaObject::invokeMethod()` 将回调从协议线程切换到UI线程执行。
  - 确保UI操作的线程安全性，避免直接操作UI对象导致的崩溃。
- 到位处理逻辑
  - `OnServoArrivalFeedback()` 在UI线程处理到位反馈。
  - 正常到位(0x01)：触发测量流程，调用 `TriggerMeasureAndArm()`。
  - 超时异常(0x00)：记录告警并中止测量，调用 `AbortMeasure()`。
  - 非测量流程：仅记录日志，不做其他处理。

```mermaid
classDiagram
class CServoArrivalFeedback {
+uint8_t m_cResult
+uint16_t m_wTargetPos
+uint16_t m_wActualPos
+bool IsArrived() const
+static double StepsToAngle(uint16_t wSteps)
}
class Insulator_Zero_Value_Detection_Robot {
+void CallBack_ServoArrival(const CServoArrivalFeedback& feedback)
+void OnServoArrivalFeedback(const CServoArrivalFeedback& feedback)
+void TriggerMeasureAndArm()
+void AbortMeasure(const QString& strReason)
}
class WHSDControlBoardProtocol {
+void RegisterServoArrivalCallBack(function)
}
Insulator_Zero_Value_Detection_Robot --> CServoArrivalFeedback : "处理"
Insulator_Zero_Value_Detection_Robot --> WHSDControlBoardProtocol : "注册回调"
```

**图示来源**
- [WHSDControlBoradProtocol.h:21-50](file://Insulator_Zero_Value_Detection_Robot/Protocol/WHSDControlBoradProtocol.h#L21-L50)
- [Insulator_Zero_Value_Detection_Robot.h:163-172](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.h#L163-L172)
- [Insulator_Zero_Value_Detection_Robot.cpp:836-893](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L836-L893)

**章节来源**
- [Insulator_Zero_Value_Detection_Robot.cpp:286](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L286)
- [Insulator_Zero_Value_Detection_Robot.cpp:836-893](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L836-L893)
- [WHSDControlBoradProtocol.h:253-257](file://Insulator_Zero_Value_Detection_Robot/Protocol/WHSDControlBoradProtocol.h#L253-L257)

### 自定义控件 ContentWidget
- 职责：提供统一的图表容器与加载流程，支持错误提示与默认ChartView管理。
- 关键点：load() 防止重复加载；doLoad() 供子类重写以完成具体初始化；resizeEvent 自动调整默认ChartView尺寸。

**章节来源**
- [contentwidget.h:12-32](file://Insulator_Zero_Value_Detection_Robot/UI/contentwidget.h#L12-L32)
- [contentwidget.cpp:12-63](file://Insulator_Zero_Value_Detection_Robot/UI/contentwidget.cpp#L12-L63)

### 数据可视化 ModelDataWidget + ModelDataModel
- ModelDataWidget
  - 构建QTableView与QChartView，使用QSplitter控制表格与曲线图宽度。
  - setTableLayout：根据表头重建表格列与曲线系列，设置坐标轴范围。
  - appendValue：找到该列下一个空单元格填充值，同时追加曲线点，动态扩展Y轴范围。
  - removeLastValue：删除最近一个测量值及其曲线点，用于重测场景。
- ModelDataModel
  - 继承QAbstractTableModel，维护二维数据列表与映射区域。
  - setTableLayout：重置模型，初始化NaN占位。
  - data：DisplayRole/EditRole返回数值，BackgroundRole按映射着色。
  - setData：编辑单元格并触发dataChanged。

```mermaid
classDiagram
class ContentWidget {
+load()
#doLoad() bool
+createDefaultChartView(chart)
+defaultChartView() QChartView*
+setDefaultChartView(view)
}
class ModelDataWidget {
+setTableLayout(headers, rowCount)
+appendValue(header, value)
+removeLastValue(header)
-applyTableWidth()
-resizeEvent(event)
}
class ModelDataModel {
+rowCount(index) int
+columnCount(index) int
+headerData(section, orientation, role) QVariant
+data(index, role) QVariant
+setData(index, value, role) bool
+flags(index) Qt : : ItemFlags
+setTableLayout(headers, rowCount)
+columnIndex(header) int
+addMapping(color, area)
+clearMapping()
}
ContentWidget <|-- ModelDataWidget : "继承"
ModelDataWidget --> ModelDataModel : "使用"
```

**图示来源**
- [contentwidget.h:12-32](file://Insulator_Zero_Value_Detection_Robot/UI/contentwidget.h#L12-L32)
- [modeldatawidget.h:20-49](file://Insulator_Zero_Value_Detection_Robot/UI/modeldatawidget.h#L20-L49)
- [modeldatamodel.h:12-39](file://Insulator_Zero_Value_Detection_Robot/UI/modeldatamodel.h#L12-L39)

**章节来源**
- [modeldatawidget.cpp:24-187](file://Insulator_Zero_Value_Detection_Robot/UI/modeldatawidget.cpp#L24-L187)
- [modeldatamodel.cpp:13-113](file://Insulator_Zero_Value_Detection_Robot/UI/modeldatamodel.cpp#L13-L113)

### 对话框 NewTicketDialog / NewReportDialog
- 职责：收集工单/报告信息，通过信号通知主窗口进行新增或修改。
- 信号：NewTicketSignal/ChangeTicketSignal、NewReportSignal/ChangeReportSignal。
- 主窗口侧：BindAction 中连接这些信号到对应槽函数，完成数据持久化与界面刷新。

**章节来源**
- [NewTicketDialog.h:10-35](file://Insulator_Zero_Value_Detection_Robot/UI/NewTicketDialog.h#L10-L35)
- [NewReportDialog.h:10-31](file://Insulator_Zero_Value_Detection_Robot/UI/NewReportDialog.h#L10-L31)
- [Insulator_Zero_Value_Detection_Robot.cpp:349-352](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L349-L352)

## 依赖关系分析
- 主窗口依赖：
  - 设备通信接口 IDeviceCom 与协议 CWHSDControlBoardProtocol，用于收发命令与解析数据。
  - 配置管理器 CConfigManager，读写Config.xml。
  - 日志系统 CWriteLog，记录设备日志与操作日志。
  - 摄像头抽象 ICameraBase，支持新/旧摄像头接入。
  - 输入辅助 CXInputHelper，处理手柄状态。
- 控件依赖：
  - ModelDataWidget 依赖 ModelDataModel 提供表格数据。
  - ContentWidget 提供通用图表容器能力。
- 外部库：
  - OpenCV 用于图像处理（截图、录像帧）。
  - Qt Charts/QChartView 用于曲线绘制。

```mermaid
graph LR
Main["主窗口"] --> Com["IDeviceCom"]
Main --> Proto["CWHSDControlBoardProtocol"]
Main --> Config["CConfigManager"]
Main --> Log["CWriteLog"]
Main --> Cam["ICameraBase"]
Main --> Input["CXInputHelper"]
Main --> Chart["ModelDataWidget"]
Chart --> Model["ModelDataModel"]
Proto --> Servo["舵机到位回调"]
```

**图示来源**
- [Insulator_Zero_Value_Detection_Robot.h:8-21](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.h#L8-L21)
- [Insulator_Zero_Value_Detection_Robot.cpp:243-290](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L243-L290)
- [WHSDControlBoradProtocol.h:253-257](file://Insulator_Zero_Value_Detection_Robot/Protocol/WHSDControlBoradProtocol.h#L253-L257)

**章节来源**
- [Insulator_Zero_Value_Detection_Robot.h:8-21](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.h#L8-L21)
- [Insulator_Zero_Value_Detection_Robot.cpp:243-290](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L243-L290)

## 性能与并发
- 定时器频率：设备状态轮询100ms，手柄输入10ms，避免过高频率导致CPU占用。
- 线程安全：
  - 协议回调在协议线程，通过信号槽（QueuedConnection）切换到UI线程更新界面。
  - **新增**：舵机到位回调使用 `QMetaObject::invokeMethod()` 实现安全的跨线程通信。
  - 共享状态使用 std::mutex 与 std::atomic 保护，如心跳快照、录制标志、探针到位标志。
- 图表更新：
  - 每次appendValue仅追加一个点，动态扩展Y轴范围，减少重绘开销。
  - 表格列宽按表头文字计算，避免过度拉伸导致的布局抖动。
- 录像优化：
  - 帧缓存使用clone避免OpenCV缓冲区复用问题，停止后统一编码，降低实时编码压力。

## 故障排查指南
- 设备未连接
  - 现象：状态显示"未连接"，无法下发指令。
  - 排查：检查IP与端口配置，查看连接状态回调与日志。
- 测量无结果
  - 现象：发出测量指令后长时间无回报。
  - 排查：确认探针到位轮询与结果超时定时器是否正常启动；检查协议线程回调是否被正确注册。
- **新增**：舵机到位异常
  - 现象：探针移动后无到位反馈或超时异常。
  - 排查：检查 `CallBack_ServoArrival()` 回调是否正确注册；确认 `OnServoArrivalFeedback()` 是否在UI线程执行；查看日志中的目标位置与实际位置偏差。
- 图表不更新
  - 现象：表格或曲线未显示新值。
  - 排查：确认appendValue调用路径是否在UI线程；检查表头匹配是否正确。
- 录像卡顿或丢帧
  - 现象：录像延迟或帧率不稳定。
  - 排查：检查m_bRecording标志位与帧缓存大小；确认编码线程工作正常。

**章节来源**
- [Insulator_Zero_Value_Detection_Robot.cpp:413-559](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L413-L559)
- [Insulator_Zero_Value_Detection_Robot.cpp:675-753](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L675-L753)
- [Insulator_Zero_Value_Detection_Robot.cpp:836-893](file://Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp#L836-L893)

## 结论
UI模块以主窗口为核心，结合自定义控件与信号槽机制，实现了设备状态监控、测量流程控制、数据可视化与工单/报告管理的完整功能。**最新增强**：通过新增的舵机到位反馈处理机制，实现了更精确的探针位置控制和完善的异常处理。通过严格的线程边界划分与状态机设计，保证了多线程环境下的稳定性与可维护性。后续扩展可在现有基础上增加新的页面、控件或业务流程，遵循信号槽解耦与线程安全的最佳实践。

## 附录：UI扩展开发指南与最佳实践
- 新增页面或控件
  - 在 .ui 中拖拽布局，或在代码中动态创建 QWidget 并加入布局。
  - 若涉及图表，建议继承 ContentWidget 并重写 doLoad，复用默认ChartView管理能力。
- 数据绑定与更新
  - 使用 ModelDataModel 管理表格数据，通过 setTableLayout 初始化列与行，appendValue 增量更新。
  - 曲线图随数据动态扩展Y轴范围，避免手动计算坐标。
- 信号槽使用模式
  - 跨线程通信：协议线程 emit 信号，UI线程槽函数处理，务必使用 Qt::QueuedConnection。
  - **新增**：对于复杂的回调处理，使用 `QMetaObject::invokeMethod()` 实现安全的跨线程通信。
  - 局部更新：invokeMethod 将耗时或UI相关操作切到UI线程执行。
- 状态机设计
  - 测量流程与定标流程均采用显式状态机，避免重入与竞态条件。
  - 使用定时器与标志位组合实现轮询与兜底超时，提升鲁棒性。
  - **新增**：舵机到位反馈集成到测量流程状态机中，提供精确的位置控制。
- 线程安全
  - 共享状态使用 mutex/atomic 保护，避免直接跨线程访问UI对象。
  - 日志记录与设备通信在独立线程，UI仅负责展示与用户交互。
  - **新增**：所有协议回调都应通过 `QMetaObject::invokeMethod()` 切换到UI线程处理。
- 调试与日志
  - 利用 CWriteLog 记录关键流程节点，便于定位问题。
  - 使用 overlayLabel 临时显示检测结果，辅助调试。
  - **新增**：舵机到位反馈的详细日志包含目标位置、实际位置和偏差信息。