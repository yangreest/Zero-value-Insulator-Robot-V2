---
kind: error_handling
name: C++/Qt 桌面应用中的混合错误处理：异常、返回值与日志体系
category: error_handling
scope:
    - '**'
source_files:
    - Insulator_Zero_Value_Detection_Robot/Tools/Tools.cpp
    - Insulator_Zero_Value_Detection_Robot/Log/WriteLogIns.h
    - Insulator_Zero_Value_Detection_Robot/Log/ScanS_WriteLog.h
    - Insulator_Zero_Value_Detection_Robot/Log/ScanS_WriteLog.cpp
    - Insulator_Zero_Value_Detection_Robot/Report/WriteReports.cpp
    - Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp
    - Insulator_Zero_Value_Detection_Robot/main.cpp
---

## 1. 整体方案
该仓库是一个基于 Qt/C++ 的 Windows 桌面应用，没有引入统一的错误框架（如 Boost.Exception、自定义异常基类或全局错误码枚举）。错误处理采用**混合模式**：
- 工具层使用 C++ 标准异常（`std::invalid_argument`、`std::runtime_error`）表达参数非法和系统调用失败；
- 业务层（报告生成等）通过 `bool` 返回值 + `qWarning()` / `qDebug()` 输出诊断信息；
- 日志子系统提供独立的线程安全写入能力，并定义自身的 `LogError` 枚举表示内部状态。

## 2. 关键文件与位置
- **异常抛出点**：`Insulator_Zero_Value_Detection_Robot/Tools/Tools.cpp` 中 `WHSD_Tools::ScaleUInt16Array`、`Base64Decode`、`CreateFolderRecursively` 等方法在检测到空指针、非法 Base64 输入、Windows API 失败时 `throw std::invalid_argument` / `std::runtime_error`。上层 `Tools.cpp` 的 `catch (const std::exception& e)` 捕获后记录日志。
- **日志库**：`Insulator_Zero_Value_Detection_Robot/Log/WriteLogIns.h` 定义 `enum LogError { LE_Succeed=0, LE_Init, LE_Open, LE_Overflow }` 以及 `CWriteLogIns` 类，封装带行头/尾格式的异步日志写入（支持同步/异步模式、循环覆盖行数限制、临界区保护）。
- **日志门面**：`Insulator_Zero_Value_Detection_Robot/Log/ScanS_WriteLog.h/.cpp` 暴露 `CWriteLog`，将 `Write` / `WriteFormat` 转发到底层 `CWriteLogIns`，对外隐藏实现细节。
- **业务错误返回**：`Insulator_Zero_Value_Detection_Robot/Report/WriteReports.cpp` 的 `RewriteDocx` / `FillDocxTemplate` / `FillMearDataReport` 等函数以 `bool` 返回值表示成功/失败，并在路径为空、模板打开失败、目录创建失败、XML 结构缺失等场景下调用 `qWarning()` 输出中文描述后 `return false`。
- **UI 调试输出**：`Insulator_Zero_Value_Detection_Robot/UI/Insulator_Zero_Value_Detection_Robot.cpp` 在 RTSP 流打开失败、帧为空、抓取失败时使用 `qDebug()` 输出调试信息。
- **入口初始化**：`main.cpp` 仅在 `_DEBUG` 下分配控制台并重定向 `stdout`，无全局异常处理器或 `try/catch` 包裹 `app.exec()`。

## 3. 架构与约定
- **分层职责清晰**：低层工具函数用异常表达“不可恢复的参数/系统错误”，中层业务函数用 `bool` + `qWarning()` 表达“可恢复的业务错误”（如模板不存在），高层 UI 用 `qDebug()` 做运行时诊断。
- **日志独立且线程安全**：`CWriteLogIns` 内部维护消息队列、写线程、事件句柄和临界区，允许多线程并发调用 `Write` / `WriteFormat`，并通过 `BeginWork()/EndWork()` 生命周期管理。
- **日志格式固定**：每条日志前缀为 `[毫秒时间戳][YYYY-MM-DD HH:MM:SS:ms]`，后缀为双回车换行，便于后续解析。
- **无统一异常类型**：未定义项目级异常基类或错误码枚举（除日志模块的 `LogError`），异常类型直接复用 `std::` 命名空间下的标准异常。
- **无全局 try/catch 兜底**：`main()` 不包裹 `QApplication::exec()`，也没有安装 `std::set_uncaught_exception` 或 Windows SEH 过滤器，崩溃由操作系统默认处理。

## 4. 约定与约束
- **工具函数参数校验**：对空指针、非正长度、非法 Base64 字符串等前置条件，直接 `throw std::invalid_argument(...)`，调用方需自行捕获（当前 `Tools.cpp` 内已有 `catch(const std::exception&)` 示例）。
- **文件系统操作失败**：`CreateFolderRecursively` 在 `CreateDirectoryA` 失败且非 `ERROR_ALREADY_EXISTS` 时抛出 `std::runtime_error`，并附带 `GetLastError()` 数值。
- **报告生成失败策略**：`WriteReports.cpp` 遇到模板路径为空、模板与输出路径相同、模板打开失败、`word/document.xml` 为空、输出目录创建失败等情况，统一走 `qWarning() << ...; return false;` 路径，调用方应检查返回值。
- **日志写入容量约束**：`CWriteLog` 构造函数注释明确要求每行字节数须大于 38 且不超过 1024，最大行数不超过 999999，超出范围属于调用方契约违规。
- **日志内部错误码**：`LogError` 枚举仅用于内部状态标识（成功、初始化错误、打开文件错误、溢出），未在外部广泛传播。
- **调试输出渠道**：所有 `qDebug()` / `qWarning()` 输出依赖 Qt 的消息系统；调试模式下 `main.cpp` 会分配控制台窗口并将 `stdout` 重定向至控制台，但发布构建中无此行为。