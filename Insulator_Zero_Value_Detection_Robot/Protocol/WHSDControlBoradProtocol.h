#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>

class CSensorData
{
public:
	CSensorData();
	uint8_t m_cSensorIndex;
	uint8_t m_cCmd;
	uint16_t m_wValue;

};

/// <summary>
/// 0x1A 舵机到位反馈帧（对应《舵机到位反馈通讯协议 V1.0》）
/// 主控板主动上报：运动结束或超时后输出
/// </summary>
class CServoArrivalFeedback
{
public:
	CServoArrivalFeedback();

	/// <summary>
	/// 结果：0x01=正常到位，0x00=超时未到位
	/// </summary>
	uint8_t m_cResult;

	/// <summary>
	/// 目标位置（0-4095步），大端无符号16位
	/// </summary>
	uint16_t m_wTargetPos;

	/// <summary>
	/// 实际位置（0-4095步），大端无符号16位，上报瞬间舵机真实读取位置
	/// </summary>
	uint16_t m_wActualPos;

	/// <summary>
	/// 判断是否正常到位：|实际位置-目标位置| ≤20步（约±1.8°）
	/// </summary>
	bool IsArrived() const;

	/// <summary>
	/// 将步数转换为角度：角度 = 步数 × 360 ÷ 4095
	/// </summary>
	static double StepsToAngle(uint16_t wSteps);
};

/// <summary>
/// 0x17 校准命令应答（对应《单点定标协议与流程 V1.1》）
/// 应答数据域：子命令 + 结果 + 原因 + 参数1(4B) [+ 参数2(4B)]
/// 参数1/2 除 0x03、0x04 外均为 int32 毫值大端；
/// 0x03（写 K）与 0x04（读 K）V1.1 起参数1 为 4 字节 IEEE754 float 大端，
/// 通过 m_fK 字段单独回读，m_nValue1 仍保留原始 32 位比特以便回显核对。
/// </summary>
class CCalibAnswer
{
public:
	CCalibAnswer();

	/// <summary>
	/// 子命令：
	/// 0x03 写校准系数 K（V1.1 语义变更，原为写两点线性 k/b）
	/// 0x04 读校准系数 K（V1.1 语义变更，原为读两点线性 k/b）
	/// 0x05 恢复默认  0x06 查询原始值  0x0C 读回系数
	/// 0x0A 补偿验证  0x0E 单点定标
	/// </summary>
	uint8_t m_cSubCmd;

	/// <summary>
	/// 结果：0x01成功，0x00失败
	/// </summary>
	uint8_t m_cResult;

	/// <summary>
	/// 失败原因：0x01原始值已超时 0x04Flash写失败 0x05参数长度/格式错误（含 K 为 NaN/±Inf）0x09参数非法
	/// </summary>
	uint8_t m_cReason;

	/// <summary>
	/// 参数1（毫值）：0x05为k，0x06为原始X，0x0C为a，0x0A为原始值，0x0E为a；
	/// 0x03/0x04 时为 K 的 4 字节 IEEE754 比特原样（请优先使用 m_fK）
	/// </summary>
	int32_t m_nValue1;

	/// <summary>
	/// 参数2（毫值）：0x05/0x0C/0x0E为b，0x0A为校准后值；无第二参数的子命令为0
	/// </summary>
	int32_t m_nValue2;

	/// <summary>
	/// 校准系数 K（V1.1 新增，仅当 m_cSubCmd 为 0x03 或 0x04 且成功时有效）
	/// K 上电默认 10.0，仅存 RAM、掉电/复位后恢复为 10；
	/// 校准模型：校准值 = 测量值 − K × (500 − 测量值) / 500
	/// </summary>
	float m_fK;

	/// <summary>
	/// K 的上电默认值（V1.1）
	/// </summary>
	static constexpr float DefaultK = 10.0f;

	bool IsSuccess() const;

	/// <summary>
	/// 当前应答是否为 K 的写入/读回（0x03/0x04）
	/// </summary>
	bool IsKAnswer() const;
};

class CControlBoardProtocolConfig
{
public:

	CControlBoardProtocolConfig();

	/// <summary>
	/// 激光测距最近距离阈值
	/// </summary>
	uint16_t m_wLidarMinDis;

	/// <summary>
	/// 激光测距最远距离阈值
	/// </summary>
	uint16_t m_wLidarMaxDis;

	/// <summary>
	/// 安全轮角度判断阈值（单位度）
	/// </summary>
	float m_fSafeAngle;

	/// <summary>
	/// 成像板角度判断阈值（单位度）
	/// </summary>
	float m_fSBAngle;

	/// <summary>
	/// 行走电机异常电压阈值
	/// </summary>
	float m_fWalkingV;

	/// <summary>
	/// 安全轮电机异常电压阈值
	/// </summary>
	float m_fSafeV;

	/// <summary>
	/// 成像板电机异常电压阈值
	/// </summary>
	float m_fSBV;

	/// <summary>
	/// 卷扬电机异常电压阈值
	/// </summary>
	float m_fJYV;

	/// <summary>
	/// 备用卷扬电机异常电压阈值
	/// </summary>
	float m_fJY2V;

	/// <summary>
	/// 成像板运行时间阈值，单位毫秒
	/// </summary>
	uint32_t m_nSBRun;

	std::vector<uint8_t> GetDataByte();
};

class CMotorDeviceStatus
{
public:
	CMotorDeviceStatus();
	bool m_bDeviceEnable;
	uint8_t m_cDeviceStatus;
};

class CDeviceHeartBeat
{
public:
	CDeviceHeartBeat();

	/// <summary>
	/// 行走电机状态
	/// </summary>
	std::vector<CMotorDeviceStatus> m_vectorWalkingMotorStatus;

	/// <summary>
	/// 卷扬电机状态
	/// </summary>
	std::vector<CMotorDeviceStatus> m_vectorWindmillMotorStatus;

	/// <summary>
	/// 安全电机状态
	/// </summary>
	std::vector<CMotorDeviceStatus> m_vectorSafetyMotorStatus;

	/// <summary>
	/// 采样板电机状态
	/// </summary>
	std::vector<CMotorDeviceStatus> m_vectorSBMotorStatus;

	/// <summary>
	/// 电池百分比
	/// </summary>
	uint8_t m_cBattery;

	/// <summary>
	/// 固件发布年份，例2025以0x19表示，下同
	/// </summary>
	uint8_t m_cHardwareYear;

	/// <summary>
	/// 固件发布月份
	/// </summary>
	uint8_t m_cHardwareMonth;

	/// <summary>
	/// 固件发布日期
	/// </summary>
	uint8_t m_cHardwareDay;

	/// <summary>
	/// 固件当天编译次数
	/// </summary>
	uint8_t m_cHardwareVersionOfDay;

	/// <summary>
	/// 射线机状态(0x00:空闲    0x01：延时开启中 0x02：工作完成)
	/// </summary>
	uint8_t m_cXRayDeviceStatus;

	/// <summary>
	/// 总电源，0x00-关，0x01-开
	/// </summary>
	uint8_t m_cMainPowerSupply;

	/// <summary>
	/// 是否为
	/// </summary>
	bool m_bFactoryMode;

	void ExtractMotorStatus(int motorType, const uint8_t* data);
};

class CWHSDControlBoardProtocol
{
public:
	CWHSDControlBoardProtocol(uint16_t wHeartBeatTime);

	bool BeginWork();

	bool EndWork();

	void ReceiveNewData(const uint8_t* p, int len);

	bool Parse();

	void RegisterAnswerFunction(const std::function<bool(uint8_t*, int)>& f);

	void RegisterZeroDataCallBack(const std::function<void(float*)>& p);

	void RegisterDeviceHeartBeat(const std::function<void(const CDeviceHeartBeat&)>& f);

	void RegisterDeviceLog(const std::function<void(const std::string&)>& f);

	void RegisterOTAStatus(const std::function<void(uint8_t, uint32_t, uint32_t)>& f);

	void BeginOTA(const std::vector<uint8_t>& file);

	void RegisterSensorDataCallBack(const std::function<void(CSensorData*)>& p);

	void RegisterCalibCallBack(const std::function<void(const CCalibAnswer&)>& f);

	/// <summary>
	/// 注册舵机到位反馈回调（CMD=0x1A）
	/// 正常到位或超时异常均会上报，上位机需根据结果判断后续处理
	/// </summary>
	void RegisterServoArrivalCallBack(const std::function<void(const CServoArrivalFeedback&)>& f);

	uint8_t m_cPackNumber;

	/// <summary>
	/// 控制电机运行
	/// </summary>
	/// <param name="target">目标电机</param>
	/// <param name="enable">电机使能</param>
	/// <param name="runMode">运行模式</param>
	/// <param name="speed">运行速度</param>
	/// <returns></returns>
	static std::vector<uint8_t> DeviceRun(uint8_t target, uint8_t enable, uint8_t runMode, uint8_t speed);

	static std::vector<uint8_t> DeviceRun(uint8_t target, uint8_t enable, uint8_t runMode,uint8_t runAngel, uint16_t speed);

	static std::vector<uint8_t> DeviceStop(uint8_t target);

	static std::vector<uint8_t> DeviceStopAll();

	static std::vector<uint8_t> DeviceBreak();

	static std::vector<uint8_t> SendNumberOfPulses(uint8_t mc);

	static std::vector<uint8_t> SendDelayTime(uint8_t mc);

	static std::vector<uint8_t> StartXRay(uint8_t startMode, uint8_t startTTL, uint8_t isDelay);

	static std::vector<uint8_t> StopXRay();

	static std::vector<uint8_t> TurnOnAll();

	static std::vector<uint8_t> TurnOffAll();

	static std::vector<uint8_t> SetControlBoardConfig(CControlBoardProtocolConfig* config);

	static std::vector<uint8_t> SetFactoryMode(bool bFactoryMode);

	static std::vector<uint8_t> SensorCmd(uint8_t sensorIndex, uint8_t cmd, uint16_t value);

	/// <summary>
	/// 恢复默认（0x17/0x05）：清空全部校准（两点系数、折点表、公式系数），
	/// 测量通道直通（0x0F直接回报原始值）。单点定标前必发。
	/// </summary>
	static std::vector<uint8_t> CalibReset();

	/// <summary>
	/// 查询原始值（0x17/0x06）：回传最近一次原始X。
	/// 下位机只认最近一次0x0F回报后5秒内刷新的值，超时返回结果00、原因01。
	/// </summary>
	static std::vector<uint8_t> CalibQueryRaw();

	/// <summary>
	/// 读回系数（0x17/0x0C）：核对Flash中已存的a/b毫值
	/// </summary>
	static std::vector<uint8_t> CalibReadCoef();

	/// <summary>
	/// 补偿验证（0x17/0x0A）：不挂电阻，直接下发任意原始值，
	/// 应答回显"原始值 + 校准后值"，用于离线抽查系数是否生效。
	/// </summary>
	/// <param name="nRawMilli">原始值（int32毫值）</param>
	static std::vector<uint8_t> CalibVerify(int32_t nRawMilli);

	/// <summary>
	/// 单点定标（0x17/0x0E，核心命令）：下发"标准值 + 实测原始值"，
	/// 固件自动计算补偿系数c并立即写入Flash（擦写需1~2秒，应答超时建议≥3秒）。
	/// 参数必须正好8字节，后发覆盖先发，无须先清除。
	/// V1.1 注意：本命令仍会成功写 Flash 并回显系数，但被 §4.7 增益校准恒生效遮蔽，
	/// 0x0E 算出的 a/b 不会改变 0x0F 回报值；如需实际生效请改用 CalibWriteK。
	/// </summary>
	/// <param name="nStdMilli">标准值（int32毫值，如500 MΩ传500000）</param>
	/// <param name="nRawMilli">实测原始值（int32毫值，必须小于标准值）</param>
	static std::vector<uint8_t> CalibSinglePoint(int32_t nStdMilli, int32_t nRawMilli);

	/// <summary>
	/// 写校准系数 K（0x17/0x03，V1.1 新增，当前主用）
	/// 数据域：17 + 03 + K(4B IEEE754 float 大端)，共 4 字节参数。
	/// 完整帧示例（K = 10）：FF FE 0D 01 17 03 41 20 00 00 86 FD FC
	/// 固件不做任何二次加工，写什么就是什么，立即生效；不写 Flash，掉电/复位后恢复为默认 10。
	/// 校准模型：校准值 = 测量值 − K × (500 − 测量值) / 500，展开为 (1 + K/500)×测量值 − K。
	/// K = 0 表示直通（不校准）；K 允许为负与小数；K = −500 为退化值（增益为 0），勿使用。
	/// V1.1 语义变更：原为写两点线性系数 k/b（数据域 8 字节），旧格式不再支持。
	/// </summary>
	/// <param name="fK">校准系数（float，NaN/±Inf 会被下位机拒绝，返回结果 00、原因 05）</param>
	static std::vector<uint8_t> CalibWriteK(float fK);

	/// <summary>
	/// 读校准系数 K（0x17/0x04，V1.1 新增）
	/// 无参数。应答数据域：17 + 04 + 结果 + 原因 + K(4B IEEE754 float 大端)。
	/// 完整帧示例：FF FE 09 01 17 04 22 FD FC
	/// 由于 K 不落 Flash，每次上电后首次读到的必然是默认值 10。
	/// 若读回值与下发值不符，说明中途发生过复位/掉电，需重新下发 CalibWriteK。
	/// V1.1 语义变更：原为读两点线性系数 k/b（应答 8 字节），现改为只回读 K（应答 4 字节）。
	/// </summary>
	static std::vector<uint8_t> CalibReadK();

	/// <summary>
	/// 单点拉齐辅助计算（V1.1 §5.1）：由当前读数 x0 与期望目标 y0 反解 K，
	/// K = 500 × (y0 − x0) / (x0 − 500)，要求 x0 ≠ 500（500 是公式不动点，无解）。
	/// 参数为 MΩ 物理值（非毫值），返回值可直接送入 CalibWriteK。
	/// x0 == 500 时返回 NaN，调用方需自行判断。
	/// </summary>
	static float CalibComputeKFromSinglePoint(float fX0, float fY0);

private:
	static std::vector<uint8_t> GetCmdData(uint8_t cmd, const std::vector<uint8_t>& cmdData);

	/// <summary>
	/// 按大端追加一个int32到数据域
	/// </summary>
	static void AppendInt32BE(std::vector<uint8_t>& data, int32_t nValue);

	/// <summary>
	/// 从缓冲区按大端读取一个int32（协议规定多字节整数一律大端、X值有符号）
	/// </summary>
	static int32_t GetInt32BE(const uint8_t* p);

	/// <summary>
	/// 按大端追加一个 IEEE754 float 到数据域（V1.1 校准系数 K 专用，非毫值整数）
	/// </summary>
	static void AppendFloatBE(std::vector<uint8_t>& data, float fValue);

	/// <summary>
	/// 从缓冲区按大端读取一个 IEEE754 float（V1.1 校准系数 K 应答解析专用）
	/// </summary>
	static float GetFloatBE(const uint8_t* p);

	static uint8_t cPackNumber;

	void OTAThread();

	void SendData(uint8_t* p, int len);

	void LoopSendHeartBeat();

	void DealHeartBeat();

	CDeviceHeartBeat m_memDeviceHeartBeat;

	std::vector<uint8_t> m_vectorDataBuffer;

	void Erase(int en);

	std::function<bool(uint8_t*, int)> m_function_Answer;

    std::function<void(float*)> m_function_ZeroDataCallBack;

	std::function<void(CDeviceHeartBeat)> m_function_DeviceHeartBeat;

	std::function<void(const std::string&)> m_function_WriteLogCallBack;

	std::function<void(uint8_t, uint32_t, uint32_t)> m_function_OTAStatusCallBack;

	std::function<void(CSensorData*)> m_function_SensorDataCallBack;

	std::function<void(const CCalibAnswer&)> m_function_CalibCallBack;

	/// <summary>
	/// 舵机到位反馈回调（CMD=0x1A）：结果/目标位置/实际位置
	/// </summary>
	std::function<void(const CServoArrivalFeedback&)> m_function_ServoArrivalCallBack;

	std::vector<uint8_t> m_vectorCmdData;

	int m_cCmd;

	bool m_nIsNeedExit;

	void Answer();

	uint16_t m_wHeartBeatTime;

	uint8_t m_cOTAStatus;

	bool m_bPauseHeartBeat;

	std::vector<uint8_t> m_vector_OTAFile;

	uint8_t m_cOTAErrorCount;

	uint32_t m_nOTAAllPacks;

	uint32_t m_nOTAPackIndex;

	// ===== OTA 事件驱动同步（V1.1）=====
	// 旧实现为固定 Sleep(500ms) 轮询，即使 ACK 已到达也要等到下一个周期才发下一包；
	// 现改为条件变量等待：0x0b ACK 到达时由接收线程唤醒，发送线程立即发下一包，
	// 超时（OTA_ACK_TIMEOUT_MS）时仍按原节奏重发当前包，保持与旧行为一致的重试容错。
	std::mutex m_mutexOTA;
	std::condition_variable m_cvOTA;
	// 当前已发、等待 ACK 的包序号（1-based）；接收到的 0x0b 应答必须与之一致才视为有效 ACK，
	// 避免旧/乱序应答误推进下一包
	uint32_t m_nOTAExpectedAckIndex;
	// 当前包的 ACK 是否已到达（仅 m_mutexOTA 保护下读写）
	bool m_bOTAAckReceived;

	CSensorData m_memCSensorData;
};
