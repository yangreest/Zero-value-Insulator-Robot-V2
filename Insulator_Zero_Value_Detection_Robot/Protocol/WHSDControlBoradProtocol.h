#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class CSensorData
{
public:
	CSensorData();
	uint8_t m_cSensorIndex;
	uint8_t m_cCmd;
	uint16_t m_wValue;

};

/// <summary>
/// 0x17 校准命令应答（对应《单点定标协议与流程 V1.0》）
/// 应答数据域统一为：子命令 + 结果 + 原因 + 参数1(4B) + 参数2(4B)，参数均为int32毫值大端
/// </summary>
class CCalibAnswer
{
public:
	CCalibAnswer();

	/// <summary>
	/// 子命令：0x05恢复默认 0x06查询原始值 0x0C读回系数 0x0A补偿验证 0x0E单点定标
	/// </summary>
	uint8_t m_cSubCmd;

	/// <summary>
	/// 结果：0x01成功，0x00失败
	/// </summary>
	uint8_t m_cResult;

	/// <summary>
	/// 失败原因：0x01原始值已超时 0x04Flash写失败 0x05参数长度/格式错误 0x09参数非法
	/// </summary>
	uint8_t m_cReason;

	/// <summary>
	/// 参数1（毫值）：0x05为k，0x06为原始X，0x0C为a，0x0A为原始值，0x0E为a
	/// </summary>
	int32_t m_nValue1;

	/// <summary>
	/// 参数2（毫值）：0x05/0x0C/0x0E为b，0x0A为校准后值；无第二参数的子命令为0
	/// </summary>
	int32_t m_nValue2;

	bool IsSuccess() const;
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
	/// </summary>
	/// <param name="nStdMilli">标准值（int32毫值，如500 MΩ传500000）</param>
	/// <param name="nRawMilli">实测原始值（int32毫值，必须小于标准值）</param>
	static std::vector<uint8_t> CalibSinglePoint(int32_t nStdMilli, int32_t nRawMilli);

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

	CSensorData m_memCSensorData;
};
