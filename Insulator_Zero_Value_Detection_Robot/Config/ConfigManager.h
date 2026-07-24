#pragma once
#include <string>
#include <vector>
#include <qmetatype.h>


class CControlBoardConfig
{
public:
	CControlBoardConfig();
	std::string m_strIp;
	uint16_t m_wPort;
	uint16_t m_wDeviceHeartBeat;
	bool m_bFactoryMode;
	uint8_t m_cUpAngle;
	uint8_t m_cDownAngle;
	uint8_t m_cWalkMotorSpeed;
};

class CCameraConfig
{
public:
	std::string m_strLeftIp;
	std::string m_strMidIp;
	std::string m_strRightIp;

	bool m_bNewCamera; // 使用新款相机
	bool m_bUseMainSp;// 使用主码流
	std::string m_strMainRtsp; // 主码流
	std::string m_strSubRtsp;// 子码流
};

class CNewReportConfig
{
public:
	//报告的编号
	std::string m_strReportId;
	// 检测单位 
	std::string m_strDetectionUnit;
	// 检测人员
	std::string m_strDetectionPerson;
	// 作业地点
	std::string m_strWorkPlace;
};

class CNewTicketConfig
{
public:
	// 串数类型
	enum class BunchType
	{
		// 单联，双联
		eSingle,
		eDouble,
	};

	// 回路数
	enum class LoopType
	{
		// 同塔单回，同塔双回，同塔四回
		eOne,
		eTwo,
		eFour,
	};

	static const std::string m_vecLoopType(LoopType loopType)
	{
		switch (loopType)
		{
		case LoopType::eOne:
			return "同塔单回";
		case LoopType::eTwo:
			return "同塔双回";
		case LoopType::eFour:
			return "同塔四回";
		default:
			break;
		}
	};

	static const std::string m_vecBunchType(BunchType bunchType)
	{
		switch (bunchType)
		{
		case BunchType::eSingle:
			return "单联";
		case BunchType::eDouble:
			return "双联";
		default:
			break;
		}
	};

	//线路名称
	std::string m_strLineName;
	// 杆塔号
	std::string m_strPoleNumber;
	// 串数
	BunchType  m_eBunchType;
	// 绝缘子片数
	uint16_t m_wInsulatorSliceNum;
	// 回路数
	LoopType m_eLoopType;
	//检测单位
	std::string m_strDetectionUnit;
	// 备注信息
	std::string m_strRemark;
};


class CConfigManager
{
public:
	CControlBoardConfig m_memControlBoardConfig;
	CCameraConfig m_memCCameraConfig;
	void Read(const std::string& filePath);
	void Write(const std::string& filePath);
};

// 关键：注册元类型，让QVariant识别这个类
Q_DECLARE_METATYPE(CNewTicketConfig);
Q_DECLARE_METATYPE(CNewReportConfig);
