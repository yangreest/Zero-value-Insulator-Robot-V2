#pragma once
#include <string>
#include <vector>
#include <qmetatype.h>
#include <QMap>
#include <QString>


class CControlBoardConfig
{
public:
	CControlBoardConfig();
	std::string m_strIp;
	uint16_t m_wPort;
	uint16_t m_wDeviceHeartBeat;
	bool m_bFactoryMode;
	uint8_t m_cUpAngle; // 探针1的位置
	uint8_t m_cDownAngle; // 探针待机的位置
	uint8_t m_cUpAngle2;//探针2的位置
	uint8_t m_cWalkMotorSpeed; 
	// 绝缘的阈值
    uint16_t m_wInsuThreshold;
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

	QMap<QString, QMap<QString, QVector<float>>> m_mapTicketMearData;
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

	// 电流类型
	enum class CurrentType
	{
		// 交流，直流
		eAC,
		eDC,
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

	static const LoopType m_vecLoopType(const std::string& strLoopType)
	{
		if (strLoopType == "同塔单回")
			return LoopType::eOne;
		else if (strLoopType == "同塔双回")
			return LoopType::eTwo;
		else if (strLoopType == "同塔四回")
			return LoopType::eFour;
		else
			return LoopType::eOne;
	};

	static const BunchType m_vecBunchType(const std::string& strBunchType)
	{
		if (strBunchType == "单联")
			return BunchType::eSingle;
		else if (strBunchType == "双联")
			return BunchType::eDouble;
		else
			return BunchType::eSingle;
	};

	static const std::string m_vecCurrentType(CurrentType currentType)
	{
		switch (currentType)
		{
		case CurrentType::eAC:
			return "交流";
		case CurrentType::eDC:
			return "直流";
		default:
			break;
		}
		return "交流";
	};

	static const CurrentType m_vecCurrentType(const std::string& strCurrentType)
	{
		if (strCurrentType == "交流")
			return CurrentType::eAC;
		else if (strCurrentType == "直流")
			return CurrentType::eDC;
		else
			return CurrentType::eAC;
	};
	// 增加一个戳
	std::string m_strTicketId;  // 唯一标识符

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
	// 交/直流类型
	CurrentType m_eCurrentType = CurrentType::eAC;
	// 开始时间（yyyy-MM-dd HH:mm）
	std::string m_strStartTime;
	// 结束时间（yyyy-MM-dd HH:mm）
	std::string m_strEndTime;
	// 检测人员
	std::string m_strDetectionPerson;
	//检测单位
	std::string m_strDetectionUnit;
	// 备注信息
	std::string m_strRemark;
	// 是否生成过报告
	bool m_bGenerateReport =false;
};

class CConfigManager
{
public:
	CControlBoardConfig m_memControlBoardConfig;
	CCameraConfig m_memCCameraConfig;
	std::vector<CNewTicketConfig> m_vecNewTicketConfig;
	std::vector<CNewReportConfig> m_vecNewReportConfig;
	void Read(const std::string& filePath);
	void Write(const std::string& filePath);
};

// 关键：注册元类型，让QVariant识别这个类
Q_DECLARE_METATYPE(CNewTicketConfig);
Q_DECLARE_METATYPE(CNewReportConfig);
