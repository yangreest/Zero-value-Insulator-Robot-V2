---
kind: logging_system
name: 自研 CWriteLogIns 异步循环日志系统
category: logging_system
scope:
    - '**'
source_files:
    - Insulator_Zero_Value_Detection_Robot/Log/WriteLogIns.h
    - Insulator_Zero_Value_Detection_Robot/Log/WriteLogIns.cpp
    - Insulator_Zero_Value_Detection_Robot/Log/ScanS_WriteLog.h
    - Insulator_Zero_Value_Detection_Robot/Log/ScanS_WriteLog.cpp
    - Insulator_Zero_Value_Detection_Robot/Log/ScanS_FC.h
    - Insulator_Zero_Value_Detection_Robot/Log/ScanS_FC.cpp
    - Insulator_Zero_Value_Detection_Robot/main.cpp
---

## 1. 使用的系统/方案

本项目没有引入第三方日志库，而是实现了一套自研的、基于 Windows API 的固定宽度行格式日志子系统，核心位于 `Insulator_Zero_Value_Detection_Robot/Log/` 目录。该子系统提供多线程安全、支持同步/异步写入、具备文件头元数据与循环覆盖能力的日志写入器。

- **线程模型**：通过 `std::thread` + Windows `HANDLE` 事件（`m_hEventQuitWrite` / `m_hEventWrite`）驱动一个专用写线程；调用方通过 `CCFRD_CriticalSection`（封装 `CRITICAL_SECTION`）保护消息队列。
- **存储后端**：直接以二进制模式 (`"rb+"`) 打开目标 `.log` 文件，按固定列宽逐块随机写入，避免频繁 `append` 开销。
- **调试输出**：`main.cpp` 在 `_DEBUG` 下通过 `AllocConsole()` + `freopen("CONOUT$", ...)` 将 `stdout` 重定向到控制台，仅用于开发期调试，不属于正式日志通道。

## 2. 关键文件

| 文件 | 职责 |
|---|---|
| `Log/WriteLogIns.h` / `Log/WriteLogIns.cpp` | 核心实现：`CWriteLogIns` 类、`CLogInfo`、`CLogHead`，负责队列、写线程、文件头管理、循环覆盖写入 |
| `Log/ScanS_WriteLog.h` / `Log/ScanS_WriteLog.cpp` | 对外薄包装 `CWriteLog`，暴露 `Write` / `Write_Sync` / `WriteFormat` / `WriteFormat_Sync` 等接口，内部持有 `CWriteLogIns*` |
| `Log/ScanS_FC.h` / `Log/ScanS_FC.cpp` | 基础工具层：`CCFRD_CriticalSection`（临界区）、`CCFRD_Convert`（类型/时间转换）、`CCFRD_Time`（时间与时序），被日志模块依赖 |
| `main.cpp` | 仅在 Debug 构建下分配控制台并输出启动信息，不初始化日志对象 |

## 3. 架构与约定

### 3.1 分层结构
```
调用方
  └─ CWriteLog (ScanS_WriteLog)   ← 对外 API（字符串/格式化）
       └─ CWriteLogIns (WriteLogIns)
            ├─ m_vecMessages_Chunk / Write / Write_Sync 缓冲
            ├─ FuncWrite() 写线程（WaitForMultipleObjects 等待事件）
            ├─ Write_Multi() 批量落盘
            └─ CLogHead（文件首行元数据：最大行数、当前行、下一行、列宽）
```

### 3.2 日志文件格式
每行采用**固定列宽**布局，由常量定义：
- 头部格式：`[%06d][%04d-%02d-%02d %02d:%02d:%02d:%03d] `，即 `行号[YYYY-MM-DD HH:MM:SS.mmm] 消息体`，占 34 字节。
- 尾部：两个回车换行 `\r\n\r\n`，占 4 字节。
- 文件首行是 `CLogHead` 序列化的 4 个 6 位整数：`MaxLineNum CurLine NextLine MaxColNum`，用于恢复滚动位置。

### 3.3 循环覆盖策略
- 构造时指定 `dwMaxRowCount`（最大行数，上限 999999）和 `dwMaxColCount`（每行列宽，下限 38 字节，上限 1024）。
- 写入时维护 `m_nCurLine` / `m_nNextLine`，到达末尾后回绕到 1，形成环形缓冲区。
- 每次批量写入前更新文件头，再定位到 `nFirstLine * m_dwMaxColCount` 处顺序写出整块数据。

### 3.4 同步/异步写入
- 构造函数参数 `nSync` 决定默认行为：`1` 为同步（立即落盘），`0` 为异步（入队后通过事件唤醒写线程）。
- `Write(const char*, nSync=0)` 支持单次调用覆盖默认模式；`Write_Sync` / `WriteFormat_Sync` 强制同步。
- 异步路径：`SetEvent(m_hEventWrite)` → 写线程 `WaitForMultipleObjects` 收到信号 → 交换 `m_vecMessages_Chunk` 与 `m_vecMessages_Write` → 批量写入。

### 3.5 并发安全
- `m_csLogMessages`（`CCFRD_CriticalSection`）保护消息队列的 push/clear。
- `m_CS`（另一个临界区）保护文件句柄及随机读写过程，确保多写线程不会交叉写入同一文件。
- 注释明确约束："多个对象不能写同一个日志文件"（见 `ScanS_WriteLog.h` 注释）。

## 4. 约定与约束

- **行宽约束**：`dwMaxColCount` 必须 ≥ 38 字节（34 字节日期头 + 4 字节尾部），且 ≤ 1024 字节；超出会被截断并返回 `LE_Overflow`。
- **最大行数约束**：`dwMaxRowCount` 被钳制在 `[1, 999999]`。
- **单文件单实例**：每个 `CWriteLog` 实例对应一个独立日志文件，不允许跨实例共享同一文件路径。
- **生命周期管理**：必须先调用 `BeginWork()` 启动写线程，析构前调用 `EndWork()` 停止线程并 join；否则写线程可能未正确退出。
- **日志级别**：本子系统**没有内置日志级别**（无 DEBUG/INFO/WARN/ERROR 枚举或过滤），所有 `Write` 调用等价输出；如需分级需在调用方自行拼接消息内容。
- **结构化字段**：每条日志包含的字段仅为 `行号`、`本地时间（含毫秒）`、`原始消息文本`，无额外键值对或 JSON 结构。
- **错误码**：写入结果通过 `UINT` 返回，使用 `LogError` 枚举（`LE_Succeed` / `LE_Init` / `LE_Open` / `LE_Overflow`），但上层 `Write` 方法目前吞掉返回值，未向上传播。
- **调试输出**：仅在 `_DEBUG` 宏下启用控制台输出，Release 构建中不产生任何标准输出日志。