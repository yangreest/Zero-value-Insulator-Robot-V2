#include "WHSDControlBoradProtocol.h"

#include <thread>
#include <limits>
#include <chrono>
#include <Windows.h>

#include "Tools/Tools.h"

#define DEVICE_HEART_BEAT_LEN 64
#define MOTOR_COUNT 8
#define OTA_PACK_LEN 128
// OTA 事件驱动：单包 ACK 等待超时，与旧实现的 Sleep(500ms) 保持一致的重试节奏
#define OTA_ACK_TIMEOUT_MS 500

uint8_t CWHSDControlBoardProtocol::cPackNumber = 0;

CSensorData::CSensorData()
{
	m_cSensorIndex = 0;
	m_cCmd = 0;
	m_wValue = 0;
}

CServoArrivalFeedback::CServoArrivalFeedback()
	: m_cResult(0), m_wTargetPos(0), m_wActualPos(0)
{
}

bool CServoArrivalFeedback::IsArrived() const
{
	// 到位判定阈值：|实际位置-目标位置| ≤20步（约±1.8°）
	int nDiff = static_cast<int>(m_wActualPos) - static_cast<int>(m_wTargetPos);
	return (nDiff >= -20 && nDiff <= 20);
}

double CServoArrivalFeedback::StepsToAngle(uint16_t wSteps)
{
	return static_cast<double>(wSteps) * 360.0 / 4095.0;
}

CCalibAnswer::CCalibAnswer()
{
	m_cSubCmd = 0;
	m_cResult = 0;
	m_cReason = 0;
	m_nValue1 = 0;
	m_nValue2 = 0;
	// V1.1：默认值与下位机上电默认保持一致，方便调用方在未命中 0x03/0x04 时直接读取
	m_fK = DefaultK;
}

bool CCalibAnswer::IsSuccess() const
{
	return m_cResult == 0x01;
}

bool CCalibAnswer::IsKAnswer() const
{
	// V1.1：0x03 写 K、0x04 读 K 共用 m_fK 字段，其余子命令下 m_fK 无意义
	return m_cSubCmd == 0x03 || m_cSubCmd == 0x04;
}

CControlBoardProtocolConfig::CControlBoardProtocolConfig()
{
	m_wLidarMinDis = 0;
	m_wLidarMaxDis = 0;
	m_fSafeAngle = 0;
	m_fSBAngle = 0;
	m_fWalkingV = 0;
	m_fSafeV = 0;
	m_fSBV = 0;
	m_fJYV = 0;
	m_fJY2V = 0;
	m_nSBRun = 0;
}

std::vector<uint8_t> CControlBoardProtocolConfig::GetDataByte()
{
	auto dataLen = sizeof(m_wLidarMinDis) + sizeof(m_wLidarMaxDis) + sizeof(m_fSafeAngle) + sizeof(m_fSBAngle) + sizeof(
		m_fWalkingV) + sizeof(m_fSafeV) + sizeof(m_fSBV) + sizeof(m_fJYV) + sizeof(m_fJY2V) + sizeof(m_nSBRun);
	std::vector<uint8_t> rzt(dataLen);
	int index = 0;
	int si = 0;

	si = sizeof(m_wLidarMinDis);
	memcpy(rzt.data() + index, &m_wLidarMinDis, si);
	index = index + si;

	si = sizeof(m_wLidarMaxDis);
	memcpy(rzt.data() + index, &m_wLidarMaxDis, si);
	index = index + si;

	si = sizeof(m_fSafeAngle);
	memcpy(rzt.data() + index, &m_fSafeAngle, si);
	index = index + si;

	si = sizeof(m_fSBAngle);
	memcpy(rzt.data() + index, &m_fSBAngle, si);
	index = index + si;

	si = sizeof(m_fWalkingV);
	memcpy(rzt.data() + index, &m_fWalkingV, si);
	index = index + si;

	si = sizeof(m_fSafeV);
	memcpy(rzt.data() + index, &m_fSafeV, si);
	index = index + si;

	si = sizeof(m_fSBV);
	memcpy(rzt.data() + index, &m_fSBV, si);
	index = index + si;

	si = sizeof(m_fJYV);
	memcpy(rzt.data() + index, &m_fJYV, si);
	index = index + si;

	si = sizeof(m_fJY2V);
	memcpy(rzt.data() + index, &m_fJY2V, si);
	index = index + si;

	si = sizeof(m_nSBRun);
	memcpy(rzt.data() + index, &m_nSBRun, si);
	index = index + si;

	return rzt;
}

CMotorDeviceStatus::CMotorDeviceStatus()
{
	m_bDeviceEnable = false;
	m_cDeviceStatus = 0;
}

CDeviceHeartBeat::CDeviceHeartBeat()
{
	m_vectorWalkingMotorStatus.resize(MOTOR_COUNT);
	m_vectorWindmillMotorStatus.resize(MOTOR_COUNT);
	m_vectorSafetyMotorStatus.resize(MOTOR_COUNT);
	m_vectorSBMotorStatus.resize(MOTOR_COUNT);
	m_cBattery = 0;
	// 总电源默认置0(关):首帧心跳到达前避免读到未初始化的不确定值,测量前置校验以"未知即关"为安全默认
	m_cMainPowerSupply = 0;
	m_cHardwareYear = 0;
	m_cHardwareMonth = 0;
	m_cHardwareDay = 0;
	m_cHardwareVersionOfDay = 0;
	m_cXRayDeviceStatus = 0;
}

void CDeviceHeartBeat::ExtractMotorStatus(int motorType, const uint8_t* dat)
{
	CMotorDeviceStatus* motorStatus = nullptr;
	switch (motorType)
	{
	case 0:
	{
		motorStatus = m_vectorWalkingMotorStatus.data();
		break;
	}
	case 1:
	{
		motorStatus = m_vectorWindmillMotorStatus.data();
		break;
	}
	case 2:
	{
		motorStatus = m_vectorSafetyMotorStatus.data();
		break;
	}
	case 3:
	{
		motorStatus = m_vectorSBMotorStatus.data();
		break;
	}
	default:
	{
		return;
	}
	}
	for (int i = 0; i < MOTOR_COUNT; ++i)
	{
		motorStatus[i].m_cDeviceStatus = TOOLS_GET_BIT(dat[0], i);
	}
	//后面3个字节处理状态
	auto data = dat + 1;
	for (int i = 0; i < MOTOR_COUNT; ++i)
	{
		int bitPos = i * 3; // 电机状态的起始位
		int byteIdx = bitPos / 8; // 所在字节索引
		int shift = bitPos % 8; // 起始位在字节内的偏移

		// 提取当前电机的3位状态
		// 从当前字节提取部分或全部位
		uint8_t status = data[byteIdx] >> shift & 0x07;

		// 如果有剩余位在下一个字节中
		if (shift > 5 && byteIdx < 2)
		{
			uint8_t remainingBits = (data[byteIdx + 1] & (1 << (shift - 5)) - 1) << (8 - shift);
			status |= remainingBits;
		}

		motorStatus[i].m_cDeviceStatus = status;
	}
}

CWHSDControlBoardProtocol::CWHSDControlBoardProtocol(uint16_t wHeartBeatTime)
{
	m_cPackNumber = 0;
	m_function_Answer = nullptr;
	m_function_ZeroDataCallBack = nullptr;
	m_function_DeviceHeartBeat = nullptr;
	m_cCmd = 0;
	m_nIsNeedExit = false;
	m_function_WriteLogCallBack = nullptr;
	m_wHeartBeatTime = wHeartBeatTime;
	m_bPauseHeartBeat = false;
	m_cOTAStatus = 0;
	m_cOTAErrorCount = 0;
	m_nOTAPackIndex = 0;
	m_nOTAAllPacks = 0;
	m_nOTAExpectedAckIndex = 0;
	m_bOTAAckReceived = false;
	m_function_OTAStatusCallBack = nullptr;
	m_function_SensorDataCallBack = nullptr;
	m_function_CalibCallBack = nullptr;
}

bool CWHSDControlBoardProtocol::BeginWork()
{
	m_nIsNeedExit = false;
	std::thread td(&CWHSDControlBoardProtocol::LoopSendHeartBeat, this);
	td.detach();
	return true;
}

bool CWHSDControlBoardProtocol::EndWork()
{
	m_nIsNeedExit = true;
	return true;
}

void CWHSDControlBoardProtocol::ReceiveNewData(const uint8_t* p, int len)
{
	//1、先将数据添加至缓冲区最尾部
	auto oldSize = m_vectorDataBuffer.size();
	m_vectorDataBuffer.resize(oldSize + len);
	memcpy(m_vectorDataBuffer.data() + oldSize, p, len);
	while (true)
	{
		if (!Parse())
		{
			break;
		}
	}
}

bool CWHSDControlBoardProtocol::Parse()
{
	while (m_vectorDataBuffer.size() > 8)
	{
		if (m_vectorDataBuffer[0] == 0xff && m_vectorDataBuffer[1] == 0xfe)
		{
			//找到了包头
			auto packLen = m_vectorDataBuffer[2];
			if (m_vectorDataBuffer.size() < packLen)
			{
				return false;
			}
			if (m_vectorDataBuffer[packLen - 2] == 0xfd && m_vectorDataBuffer[packLen - 1] == 0xfc)
			{
				//找到了包尾
				auto checkSum = m_vectorDataBuffer[packLen - 3];
				uint8_t checkSum2 = 0;
				for (int i = 0; i < packLen - 3; i++)
				{
					checkSum2 = checkSum2 + m_vectorDataBuffer[i];
				}
				if (checkSum == checkSum2)
				{
					m_cPackNumber = m_vectorDataBuffer[3];
					m_vectorCmdData.resize(packLen - 8);
					m_cCmd = m_vectorDataBuffer[4];
					memcpy(m_vectorCmdData.data(), m_vectorDataBuffer.data() + 5, packLen - 8);
					Erase(packLen);
					if (m_function_Answer != nullptr)
					{
						Answer();
					}
					switch (m_cCmd)
					{
					case 0:
					{
						DealHeartBeat();
						break;
					}
					case 0x07:
					{
						if (m_function_WriteLogCallBack != nullptr)
						{
							m_function_WriteLogCallBack(std::string(
								m_vectorCmdData.data(), m_vectorCmdData.data() + m_vectorCmdData.size()));
						}
						break;
					}
					case 0xa:
					{
						//进入OTA模式的反馈
						if (!m_vectorCmdData.empty())
						{
							if (m_vectorCmdData[0] > 0x00)
							{
								m_cOTAErrorCount = 0;
								m_cOTAStatus = 2;
								if (m_vectorCmdData[0] == 0x02)
								{
									m_bPauseHeartBeat = true;
								}
							}
						}
						break;
					}
					case 0x0b:
					{
						if (m_vectorCmdData.size() >= 5)
						{
							if (m_vectorCmdData[0] > 0x00)
							{
								// OTA 单包应答：载荷 = 结果(1B) + 包序号(4B)
								uint32_t nAckIndex = 0;
								memcpy(&nAckIndex, m_vectorCmdData.data() + 1, sizeof(uint32_t));
								{
									std::lock_guard<std::mutex> lock(m_mutexOTA);
									// 仅当 ACK 索引与当前已发包序号一致时才推进并唤醒，避免旧/乱序应答误发下一包
									if (nAckIndex == m_nOTAExpectedAckIndex)
									{
										m_nOTAPackIndex = nAckIndex;
										m_bOTAAckReceived = true;
										m_cOTAErrorCount = 0;
									}
								}
								// 无论是否命中期望索引都 notify：命中时发送线程立即发下一包，未命中时发送线程继续等至超时
								m_cvOTA.notify_all();
							}
						}
						break;
					}
					case 0x0c:
					{
						//进入OTA模式的反馈
						//if (!m_vectorCmdData.empty())
						//{
						//	if (m_vectorCmdData[0] > 0x00)
						//	{
						//		m_cOTAResult = 255;
						//	}
						//	else
						//	{
						//		m_cOTAResult = 254;
						//	}
						//}
						break;
					}
					case 0x0f:
					{
						// 测量结果回报：X值位于数据域第7~10字节（1-based），int32有符号大端毫值
						// 未校准时回报原始值，定标生效后回报修正值
						if (m_vectorCmdData.size() < 10)
						{
							break;
						}
						const uint8_t datatype = m_vectorCmdData[1];

						switch (datatype)
						{
						case 0: // int32
						{
							// X值有符号，用uint32接会把负值解成巨大正数
							const int32_t nRaw = GetInt32BE(m_vectorCmdData.data() + 6);
							float ZeroData = static_cast<float>(nRaw) / 1000.0f;
							if (m_function_ZeroDataCallBack != nullptr)
							{
								m_function_ZeroDataCallBack(&ZeroData);
							}
							break;
						}
						case 1: // float32
						{
							break;
						}
						case 2: // double64
						{
							break;
						}
						default:
						{
							// 未知类型只丢弃本帧。不能return false：本帧已Erase，
							// 提前返回会让缓冲区里后续完整帧等到下次收数据才被解析
							break;
						}
						}
						// 必须break：原代码缺break会fall-through到case 0x11，
						// 用结果帧数据覆盖m_memCSensorData并误触发传感器回调
						break;
					}
					case 0x11:
					{
						//传感器指令反馈
						if (m_vectorCmdData.size() >= 3)
						{
							switch (m_vectorCmdData[1])
							{
							case 3:
							case 2:
							case 4:
							{
								m_memCSensorData.m_cSensorIndex = m_vectorCmdData[0];
								m_memCSensorData.m_cCmd = m_vectorCmdData[1];
								break;
							}
							default:
							{
								break;
							}
							}

							memcpy(&(m_memCSensorData.m_wValue), m_vectorCmdData.data() + 2, sizeof(m_memCSensorData.m_wValue));
							if (m_function_SensorDataCallBack != nullptr)
							{
								m_function_SensorDataCallBack(&m_memCSensorData);
							}
						}
						break;
					}
					case 0x17:
					{
						// 校准命令应答（V1.1）：子命令 + 结果 + 原因 + 参数1(4B) [+ 参数2(4B)]
						// 子命令：0x03写K 0x04读K 0x05恢复默认 0x06查询原始值 0x0C读回系数 0x0A补偿验证 0x0E单点定标
						// 参数编码：0x03/0x04 为 IEEE754 float 大端（K），其余为 int32 毫值大端
						if (m_vectorCmdData.size() < 3)
						{
							break;
						}
						CCalibAnswer answer;
						answer.m_cSubCmd = m_vectorCmdData[0];
						answer.m_cResult = m_vectorCmdData[1];
						answer.m_cReason = m_vectorCmdData[2];
						// 0x06 只带参数1（原始X），0x03/0x04 成功时只带参数1（K），其余成功帧带参数1+参数2，按长度逐个取
						if (m_vectorCmdData.size() >= 7)
						{
							if (answer.m_cSubCmd == 0x03 || answer.m_cSubCmd == 0x04)
							{
								// V1.1：K 为 4 字节 IEEE754 float 大端，不是 int32 毫值。
								// m_nValue1 仍按比特原样保留（方便日志核对 float 编码），m_fK 存实际浮点值。
								// 失败帧（如 FF FE 0B 01 17 03 00 05 28 FD FC）不带 K，size==3，此处不会进入
								answer.m_fK = GetFloatBE(m_vectorCmdData.data() + 3);
								answer.m_nValue1 = GetInt32BE(m_vectorCmdData.data() + 3);
							}
							else
							{
								answer.m_nValue1 = GetInt32BE(m_vectorCmdData.data() + 3);
							}
						}
						if (m_vectorCmdData.size() >= 11)
						{
							answer.m_nValue2 = GetInt32BE(m_vectorCmdData.data() + 7);
						}
						if (m_function_CalibCallBack != nullptr)
						{
							m_function_CalibCallBack(answer);
						}
						break;
					}
					case 0x1A:
					{
						// 舵机到位反馈帧（对应《舵机到位反馈通讯协议 V1.0》）
						// 载荷：结果(1B) + 目标位置(2B大端) + 实际位置(2B大端)
						if (m_vectorCmdData.size() < 5)
						{
							break;
						}
						CServoArrivalFeedback feedback;
						feedback.m_cResult = m_vectorCmdData[0];
						// 目标位置和实际位置均为大端16位无符号
						feedback.m_wTargetPos = (static_cast<uint16_t>(m_vectorCmdData[1]) << 8) | m_vectorCmdData[2];
						feedback.m_wActualPos = (static_cast<uint16_t>(m_vectorCmdData[3]) << 8) | m_vectorCmdData[4];
						if (m_function_ServoArrivalCallBack != nullptr)
						{
							m_function_ServoArrivalCallBack(feedback);
						}
						break;
					}
					case 0x1F:
					{

					}
					break;
					default:
					{
						break;
					}
					}
					return true;
				}
				Erase(1);
			}
			else
			{
				Erase(1);
			}
		}
		else
		{
			Erase(1);
		}
	}
	return false;
}

void CWHSDControlBoardProtocol::RegisterAnswerFunction(const std::function<bool(uint8_t*, int)>& f)
{
	m_function_Answer = f;
}

void CWHSDControlBoardProtocol::RegisterZeroDataCallBack(const std::function<void(float*)>& p)
{
	m_function_ZeroDataCallBack = p;
}

void CWHSDControlBoardProtocol::RegisterDeviceHeartBeat(const std::function<void(const CDeviceHeartBeat&)>& f)
{
	m_function_DeviceHeartBeat = f;
}

void CWHSDControlBoardProtocol::RegisterDeviceLog(const std::function<void(const std::string&)>& f)
{
	m_function_WriteLogCallBack = f;
}

void CWHSDControlBoardProtocol::RegisterOTAStatus(const std::function<void(uint8_t, uint32_t, uint32_t)>& f)
{
	m_function_OTAStatusCallBack = f;
}

void CWHSDControlBoardProtocol::BeginOTA(const std::vector<uint8_t>& file)
{
	m_vector_OTAFile = file;
	std::thread td(&CWHSDControlBoardProtocol::OTAThread, this);
	td.detach();
}

void CWHSDControlBoardProtocol::RegisterSensorDataCallBack(const std::function<void(CSensorData*)>& p)
{
	m_function_SensorDataCallBack = p;
}

void CWHSDControlBoardProtocol::RegisterCalibCallBack(const std::function<void(const CCalibAnswer&)>& f)
{
	m_function_CalibCallBack = f;
}

void CWHSDControlBoardProtocol::RegisterServoArrivalCallBack(const std::function<void(const CServoArrivalFeedback&)>& f)
{
	m_function_ServoArrivalCallBack = f;
}

std::vector<uint8_t> CWHSDControlBoardProtocol::DeviceRun(uint8_t target, uint8_t enable, uint8_t runMode,
	uint8_t speed)
{
	return GetCmdData(0x05, { target, enable, runMode, speed });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::DeviceRun(uint8_t target, uint8_t enable, uint8_t runMode, uint8_t runAngel, uint16_t speed)
{
	// speed 转大端 
	uint8_t speed_H = (speed >> 8) & 0xFF; // speed 转大端 
	uint8_t speed_L = (speed >> 0) & 0xFF; // speed 转大端 
	return GetCmdData(0x05, { target, enable, runMode,runAngel, speed_H, speed_L });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::DeviceStop(uint8_t target)
{
	return GetCmdData(0x05, { target, 0x03, 0, 0 });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::DeviceStopAll()
{
	return GetCmdData(0x05, { 0x00, 0x03, 0x00, 0x00 });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::DeviceBreak()
{
	return GetCmdData(0x05, { 0x00, 0x00, 0x02, 0b11 });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::SendNumberOfPulses(uint8_t mc)
{
	return GetCmdData(0x02, { mc });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::SendDelayTime(uint8_t mc)
{
	return GetCmdData(0x01, { mc });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::StartXRay(uint8_t startMode, uint8_t startTTL, uint8_t isDelay)
{
	return GetCmdData(0x03, { startMode, startTTL, isDelay });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::StopXRay()
{
	return GetCmdData(0x04, { 0x00 });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::TurnOnAll()
{
	return GetCmdData(0x08, { 0x01 });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::TurnOffAll()
{
	return GetCmdData(0x08, { 0x00 });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::SetControlBoardConfig(CControlBoardProtocolConfig* config)
{
	return GetCmdData(0x0d, config->GetDataByte());
}

std::vector<uint8_t> CWHSDControlBoardProtocol::SetFactoryMode(bool bFactoryMode)
{
	return GetCmdData(0x09, { (uint8_t)(bFactoryMode ? 0x01 : 0x00) });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::SensorCmd(uint8_t sensorIndex, uint8_t cmd, uint16_t value)
{
	return GetCmdData(0x11, { sensorIndex ,cmd ,(uint8_t)(value >> 8),(uint8_t)(value & 0xff) });
}

void CWHSDControlBoardProtocol::AppendInt32BE(std::vector<uint8_t>& data, int32_t nValue)
{
	//协议规定多字节整数一律大端（高位在前）
	const uint32_t nRaw = static_cast<uint32_t>(nValue);
	data.push_back(static_cast<uint8_t>((nRaw >> 24) & 0xff));
	data.push_back(static_cast<uint8_t>((nRaw >> 16) & 0xff));
	data.push_back(static_cast<uint8_t>((nRaw >> 8) & 0xff));
	data.push_back(static_cast<uint8_t>(nRaw & 0xff));
}

int32_t CWHSDControlBoardProtocol::GetInt32BE(const uint8_t* p)
{
	//X值与定标参数均为int32有符号大端毫值，不能用uint32接，否则负值会变成巨大正数
	const uint32_t nRaw = (static_cast<uint32_t>(p[0]) << 24)
		| (static_cast<uint32_t>(p[1]) << 16)
		| (static_cast<uint32_t>(p[2]) << 8)
		| static_cast<uint32_t>(p[3]);
	return static_cast<int32_t>(nRaw);
}

void CWHSDControlBoardProtocol::AppendFloatBE(std::vector<uint8_t>& data, float fValue)
{
	// V1.1：K 为 4 字节 IEEE754 float 大端，不是毫值整数。
	// 用 memcpy 做 float→uint32 的位拷贝，避免严格别名与对齐问题。
	// 例：K=10 → 0x41200000 → 41 20 00 00；K=0 → 00 00 00 00。
	uint32_t nBits = 0;
	memcpy(&nBits, &fValue, sizeof(nBits));
	data.push_back(static_cast<uint8_t>((nBits >> 24) & 0xff));
	data.push_back(static_cast<uint8_t>((nBits >> 16) & 0xff));
	data.push_back(static_cast<uint8_t>((nBits >> 8) & 0xff));
	data.push_back(static_cast<uint8_t>(nBits & 0xff));
}

float CWHSDControlBoardProtocol::GetFloatBE(const uint8_t* p)
{
	// V1.1：0x03/0x04 应答中的 K 为 4 字节 IEEE754 float 大端，位拷贝回 float
	const uint32_t nBits = (static_cast<uint32_t>(p[0]) << 24)
		| (static_cast<uint32_t>(p[1]) << 16)
		| (static_cast<uint32_t>(p[2]) << 8)
		| static_cast<uint32_t>(p[3]);
	float fValue = 0.0f;
	memcpy(&fValue, &nBits, sizeof(fValue));
	return fValue;
}

std::vector<uint8_t> CWHSDControlBoardProtocol::CalibReset()
{
	//FF FE 09 01 17 05 23 FD FC
	return GetCmdData(0x17, { 0x05 });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::CalibQueryRaw()
{
	//FF FE 09 01 17 06 24 FD FC
	return GetCmdData(0x17, { 0x06 });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::CalibReadCoef()
{
	//FF FE 09 01 17 0C 2A FD FC
	return GetCmdData(0x17, { 0x0C });
}

std::vector<uint8_t> CWHSDControlBoardProtocol::CalibVerify(int32_t nRawMilli)
{
	//FF FE 0D 01 17 0A + 原始值(4B大端) + CHK + FD FC
	std::vector<uint8_t> cmdData = { 0x0A };
	AppendInt32BE(cmdData, nRawMilli);
	return GetCmdData(0x17, cmdData);
}

std::vector<uint8_t> CWHSDControlBoardProtocol::CalibSinglePoint(int32_t nStdMilli, int32_t nRawMilli)
{
	//FF FE 11 01 17 0E + 标准值(4B大端) + 实测原始值(4B大端) + CHK + FD FC
	//参数必须正好8字节，否则下位机回结果00、原因05
	std::vector<uint8_t> cmdData = { 0x0E };
	AppendInt32BE(cmdData, nStdMilli);
	AppendInt32BE(cmdData, nRawMilli);
	return GetCmdData(0x17, cmdData);
}

std::vector<uint8_t> CWHSDControlBoardProtocol::CalibWriteK(float fK)
{
	// V1.1 §4.7 写校准系数 K（0x17/0x03）
	// FF FE 0D 01 17 03 + K(4B IEEE754 float 大端) + CHK + FD FC
	// K=10 时：FF FE 0D 01 17 03 41 20 00 00 86 FD FC
	// K=0  时：FF FE 0D 01 17 03 00 00 00 00 25 FD FC（直通，不校准）
	// K=50 时：FF FE 0D 01 17 03 42 48 00 00 AF FD FC（增益 +10%）
	// 注意：K 是 float，不是 int32 毫值；NaN/±Inf 会被下位机以"结果00、原因05"拒绝。
	// 立即生效，不写 Flash，掉电/复位后恢复为默认 10。
	std::vector<uint8_t> cmdData = { 0x03 };
	AppendFloatBE(cmdData, fK);
	return GetCmdData(0x17, cmdData);
}

std::vector<uint8_t> CWHSDControlBoardProtocol::CalibReadK()
{
	// V1.1 §4.8 读校准系数 K（0x17/0x04）
	// FF FE 09 01 17 04 22 FD FC
	// 无参数；应答数据域为 17 + 04 + 结果 + 原因 + K(4B float 大端)。
	// 由于 K 不落 Flash，每次上电后首次读到的必然是默认值 10。
	return GetCmdData(0x17, { 0x04 });
}

float CWHSDControlBoardProtocol::CalibComputeKFromSinglePoint(float fX0, float fY0)
{
	// V1.1 §5.1 单点拉齐：把当前读数 x0 抬/压到目标 y0
	// K = 500 × (y0 − x0) / (x0 − 500)
	// x0 == 500 时该式无解——500 是公式不动点，任何 K 都无法改变它；
	// 此时只能改测量链路本身。返回 NaN 让调用方显式处理，避免误下发。
	const float fDenom = fX0 - 500.0f;
	if (fDenom == 0.0f)
	{
		return std::numeric_limits<float>::quiet_NaN();
	}
	return 500.0f * (fY0 - fX0) / fDenom;
}


void CWHSDControlBoardProtocol::Erase(int en)
{
	if (en < m_vectorDataBuffer.size())
	{
		m_vectorDataBuffer.erase(m_vectorDataBuffer.begin(), m_vectorDataBuffer.begin() + en);
	}
	else
	{
		m_vectorDataBuffer.clear();
	}
}

void CWHSDControlBoardProtocol::Answer()
{
	std::vector<uint8_t> needSendData;
	switch (m_cCmd)
	{
	case 0x06:
	{
		needSendData = GetCmdData(0x06, { 0x01 });
		break;
	}
	case 0x0F:
	{
		needSendData = GetCmdData(0x0f, { 0x01 });
		break;
	}
	default:
	{
		return;
	}
	}
	m_function_Answer(needSendData.data(), needSendData.size());
}

std::vector<uint8_t> CWHSDControlBoardProtocol::GetCmdData(uint8_t cmd, const std::vector<uint8_t>& cmdData)
{
	if (++cPackNumber == 0)
	{
		cPackNumber = 0;
	}
	uint8_t panckLen = cmdData.size() + 8;
	std::vector<uint8_t> result(panckLen);
	result[0] = 0xff;
	result[1] = 0xfe;
	result[2] = panckLen;
	result[3] = cPackNumber;
	result[4] = cmd;
	memcpy(result.data() + 5, cmdData.data(), cmdData.size());
	uint8_t checkSum2 = 0;
	for (int i = 0; i < panckLen - 3; i++)
	{
		checkSum2 = checkSum2 + result[i];
	}
	result[panckLen - 3] = checkSum2;
	result[panckLen - 2] = 0xfd;
	result[panckLen - 1] = 0xfc;
	return result;
}

void CWHSDControlBoardProtocol::OTAThread()
{
	//m_bPauseHeartBeat = false;
	//开始进行OTA升级
	m_nOTAPackIndex = 0;
	m_cOTAStatus = 1;
	bool bIsNeedExit = false;
	m_cOTAErrorCount = 0;
	{
		// 新一轮升级开始前重置 ACK 同步状态，避免上一轮残留的期望索引/标志干扰本次判断
		std::lock_guard<std::mutex> lock(m_mutexOTA);
		m_nOTAExpectedAckIndex = 0;
		m_bOTAAckReceived = false;
	}
	//这是分包的逻辑
	auto otaPack = WHSD_Tools::SplitVectorData(m_vector_OTAFile, OTA_PACK_LEN);
	std::vector<uint8_t> packData(138);
	m_nOTAAllPacks = otaPack.size();
	memcpy(packData.data(), &m_nOTAAllPacks, sizeof(int));
	if (m_function_OTAStatusCallBack != nullptr)
	{
		m_function_OTAStatusCallBack(m_cOTAStatus, m_nOTAAllPacks, m_nOTAPackIndex);
	}
	while (!bIsNeedExit)
	{
		if (m_cOTAStatus == 1)
		{
			//通知设备进入OTA状态
			Sleep(1000);
			auto cmd = GetCmdData(0x0a, { 0x00 });
			SendData(cmd.data(), cmd.size());
			if (m_cOTAErrorCount++ > 5)
			{
				m_cOTAStatus = 255;
				bIsNeedExit = true;
			}
			if (m_function_OTAStatusCallBack != nullptr)
			{
				m_function_OTAStatusCallBack(m_cOTAStatus, m_nOTAAllPacks, m_nOTAPackIndex);
			}
		}
		if (m_cOTAStatus == 2)
		{
			if (m_nOTAPackIndex < m_nOTAAllPacks)
			{
				// 事件驱动发包：先登记期望索引、清 ACK 标志，再发送，最后在条件变量上等 ACK 或超时。
				// ACK 命中时接收线程会推进 m_nOTAPackIndex 并唤醒，本循环下一轮直接发下一包（无需 Sleep）；
				// 超时时 m_nOTAPackIndex 不变，下一轮重发同一包（与旧行为一致）。
				const uint32_t currentPackNumber = m_nOTAPackIndex + 1;
				{
					std::lock_guard<std::mutex> lock(m_mutexOTA);
					m_nOTAExpectedAckIndex = currentPackNumber;
					m_bOTAAckReceived = false;
				}
				memcpy(packData.data() + 4, &currentPackNumber, sizeof(uint32_t));
				packData[8] = OTA_PACK_LEN;
				memcpy(packData.data() + 9, otaPack[m_nOTAPackIndex].data(), OTA_PACK_LEN);
				uint8_t checksum = 0;
				for (int i = 0; i < OTA_PACK_LEN + 9; ++i)
				{
					checksum = checksum + packData[i];
				}
				packData[packData.size() - 1] = checksum;
				auto cmd = GetCmdData(0x0b, packData);
				SendData(cmd.data(), cmd.size());

				// 等 ACK：命中谓词立即返回；超时（OTA_ACK_TIMEOUT_MS）后重试当前包。
				// 谓词内重新判定 m_bOTAAckReceived，可避免 notify 与 wait 竞态造成的信号丢失。
				bool bAcked = false;
				{
					std::unique_lock<std::mutex> lock(m_mutexOTA);
					bAcked = m_cvOTA.wait_for(lock, std::chrono::milliseconds(OTA_ACK_TIMEOUT_MS),
						[this]() { return m_bOTAAckReceived; });
				}
				if (!bAcked)
				{
					// 超时才计错；与旧实现保持一致的后自增比较（>5），连续6次未得ACK则失败退出
					if (m_cOTAErrorCount++ > 5)
					{
						m_cOTAStatus = 255;
						bIsNeedExit = true;
					}
				}
			}
			else
			{
				m_cOTAStatus = 3;
			}

			if (m_function_OTAStatusCallBack != nullptr)
			{
				m_function_OTAStatusCallBack(m_cOTAStatus, m_nOTAAllPacks, m_nOTAPackIndex);
			}
		}
		if (m_cOTAStatus == 3)
		{
			bIsNeedExit = true;
		}
	}
	auto cmd = GetCmdData(0x0c, { 0x00 });
	SendData(cmd.data(), cmd.size());
	if (m_function_OTAStatusCallBack != nullptr)
	{
		m_function_OTAStatusCallBack(m_cOTAStatus, m_nOTAAllPacks, m_nOTAPackIndex);
	}
	//m_bPauseHeartBeat = false;
}

void CWHSDControlBoardProtocol::SendData(uint8_t* p, int len)
{
	if (m_function_Answer != nullptr)
	{
		m_function_Answer(p, len);
	}
}

// 心跳线程
void CWHSDControlBoardProtocol::LoopSendHeartBeat()
{
	//return;
	while (!m_nIsNeedExit)
	{
		Sleep(m_wHeartBeatTime);
		if (!m_bPauseHeartBeat)
		{
			auto needSendData = GetCmdData(0x00, { m_cPackNumber });
			SendData(needSendData.data(), needSendData.size());
		}
	}
}

void CWHSDControlBoardProtocol::DealHeartBeat()
{
	if (m_vectorCmdData.size() == DEVICE_HEART_BEAT_LEN && m_function_DeviceHeartBeat != nullptr)
	{
		for (int i = 0; i < 4; ++i)
		{
			m_memDeviceHeartBeat.ExtractMotorStatus(i, m_vectorCmdData.data() + i * 4);
		}
		m_memDeviceHeartBeat.m_cBattery = m_vectorCmdData[16];
		m_memDeviceHeartBeat.m_cHardwareYear = m_vectorCmdData[17];
		m_memDeviceHeartBeat.m_cHardwareMonth = m_vectorCmdData[18];
		m_memDeviceHeartBeat.m_cHardwareDay = m_vectorCmdData[19];
		m_memDeviceHeartBeat.m_cHardwareVersionOfDay = m_vectorCmdData[20];
		m_memDeviceHeartBeat.m_cXRayDeviceStatus = m_vectorCmdData[21];
		m_memDeviceHeartBeat.m_cMainPowerSupply = m_vectorCmdData[22];
		m_memDeviceHeartBeat.m_bFactoryMode = TOOLS_GET_BIT(m_vectorCmdData[23], 0) > 0;
		m_function_DeviceHeartBeat(m_memDeviceHeartBeat);
	}
}