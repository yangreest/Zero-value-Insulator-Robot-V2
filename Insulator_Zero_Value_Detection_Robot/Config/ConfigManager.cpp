#include "ConfigManager.h"
#include "../Tools/tinyxml2.h"
#include <Tools/BinData.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

CControlBoardConfig::CControlBoardConfig()
{
	m_strIp = "";
	m_wPort = 0;
	m_wDeviceHeartBeat = 200;
	m_bFactoryMode = false;
}

bool CConfigManager::Read(const std::string& filePath)
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError eResult = doc.LoadFile(filePath.c_str());
	int nIntTemp = 0;
	if (eResult != tinyxml2::XML_SUCCESS)
		return false;
	auto config = doc.RootElement();
	if (config == nullptr)
		return false;
	{
		auto deviceBoard = config->FirstChildElement("DeviceControlBoard");
		if (deviceBoard == nullptr)
			return false;
		{
			auto ipElement = deviceBoard->FirstChildElement("Ip");
			if (ipElement && ipElement->GetText())
			{
				m_memControlBoardConfig.m_strIp = ipElement->GetText();
			}
			auto portElement = deviceBoard->FirstChildElement("Port");
			if (portElement != nullptr && portElement->QueryIntText(&nIntTemp) == tinyxml2::XML_SUCCESS)
			{
				m_memControlBoardConfig.m_wPort = nIntTemp;
			}
			auto ht = deviceBoard->FirstChildElement("DeviceHeartBeat");
			if (ht != nullptr && ht->QueryIntText(&nIntTemp) == tinyxml2::XML_SUCCESS)
			{
				m_memControlBoardConfig.m_wDeviceHeartBeat = nIntTemp;
			}

			auto dm = deviceBoard->FirstChildElement("FactoryMode");
			if (dm != nullptr && dm->QueryIntText(&nIntTemp) == tinyxml2::XML_SUCCESS)
			{
				m_memControlBoardConfig.m_bFactoryMode = nIntTemp > 0;
			}
			auto d3m = deviceBoard->FirstChildElement("UpAngle");
			if (d3m != nullptr && d3m->QueryIntText(&nIntTemp) == tinyxml2::XML_SUCCESS)
			{
				m_memControlBoardConfig.m_cUpAngle = nIntTemp;
			}
			auto d4m = deviceBoard->FirstChildElement("DownAngle");
			if (d4m != nullptr && d4m->QueryIntText(&nIntTemp) == tinyxml2::XML_SUCCESS)
			{
				m_memControlBoardConfig.m_cDownAngle = nIntTemp;
			}
			auto d5m = deviceBoard->FirstChildElement("WalkMotorSpeed");
			if (d5m != nullptr && d5m->QueryIntText(&nIntTemp) == tinyxml2::XML_SUCCESS)
			{
				m_memControlBoardConfig.m_cWalkMotorSpeed = nIntTemp;
			}
            auto d8m = deviceBoard->FirstChildElement("ServoSpeed");
            if (d8m != nullptr && d8m->QueryIntText(&nIntTemp) == tinyxml2::XML_SUCCESS)
            {
                m_memControlBoardConfig.m_cServoSpeed = nIntTemp;
            }
            auto d6m = deviceBoard->FirstChildElement("InsuThreshold");
            if (d6m != nullptr && d6m->QueryIntText(&nIntTemp) == tinyxml2::XML_SUCCESS)
            {
                m_memControlBoardConfig.m_wInsuThreshold = nIntTemp;
            }
			auto d7m = deviceBoard->FirstChildElement("UpAngle2");
			if (d7m != nullptr && d7m->QueryIntText(&nIntTemp) == tinyxml2::XML_SUCCESS)
			{
				m_memControlBoardConfig.m_cUpAngle2 = nIntTemp;
			}
		}
	}
	{
		auto sb = config->FirstChildElement("Camera");
		if (sb == nullptr)
			return false;
		{
			auto nLeft = sb->FirstChildElement("Left");
			if (nLeft != nullptr)
			{
				m_memCCameraConfig.m_strLeftIp = nLeft->GetText();
			}

			auto nMid = sb->FirstChildElement("Mid");
			if (nMid != nullptr)
			{
				m_memCCameraConfig.m_strMidIp = nMid->GetText();
			}

			auto nRight = sb->FirstChildElement("Right");
			if (nRight != nullptr)
			{
				m_memCCameraConfig.m_strRightIp = nRight->GetText();
			}

			auto bNew = sb->FirstChildElement("NewCamera");
			if (bNew != nullptr && bNew->QueryIntText(&nIntTemp) == tinyxml2::XML_SUCCESS)
			{
				m_memCCameraConfig.m_bNewCamera = nIntTemp > 0;
			}

			auto bUse = doc.FirstChildElement("UseMainSp");
			if (bUse != nullptr && bUse->QueryIntText(&nIntTemp) == tinyxml2::XML_SUCCESS)
			{
				m_memCCameraConfig.m_bUseMainSp = nIntTemp > 0;
			}

			auto MainRtsp = sb->FirstChildElement("MainRtsp");
			if (MainRtsp != nullptr && MainRtsp->GetText())
			{
				m_memCCameraConfig.m_strMainRtsp = MainRtsp->GetText();
			}

			auto SubRtsp = sb->FirstChildElement("SubRtsp");
			if (SubRtsp != nullptr && SubRtsp->GetText())
			{
				m_memCCameraConfig.m_strSubRtsp = SubRtsp->GetText();
			}
		}
	}
	{
		// 读取m_vecNewTicketConfig 相关配置数据
        auto NewTicketList = config->FirstChildElement("NewTicketList");
		if (NewTicketList == nullptr)
			return false;
		{
			auto TicketsizeElement = NewTicketList->FirstChildElement("Size");
			while (TicketsizeElement != nullptr)
			{
				 CNewTicketConfig newTicketConfig;
				 const tinyxml2::XMLElement* TicketIdElement = TicketsizeElement->FirstChildElement("TicketId");
                 if (TicketIdElement && TicketIdElement->GetText())
                 {
                     newTicketConfig.m_strTicketId = TicketIdElement->GetText();
                 }

				 const tinyxml2::XMLElement* LineNameElement = TicketsizeElement->FirstChildElement("LineName");
				 if (LineNameElement && LineNameElement->GetText())
				 {
                     newTicketConfig.m_strLineName = LineNameElement->GetText();
				 }
                 const tinyxml2::XMLElement* PoleNumberElement = TicketsizeElement->FirstChildElement("PoleNumber");
                 if (PoleNumberElement && PoleNumberElement->GetText())
                 {
                     newTicketConfig.m_strPoleNumber = PoleNumberElement->GetText();
                 }
				 const tinyxml2::XMLElement* BunchTypeElement = TicketsizeElement->FirstChildElement("BunchType");
                 if (BunchTypeElement && BunchTypeElement->GetText())
                 {
                     newTicketConfig.m_eBunchType = CNewTicketConfig:: m_vecBunchType(BunchTypeElement->GetText());
                 }
                 const tinyxml2::XMLElement* InsulatorSliceNumElement = TicketsizeElement->FirstChildElement("InsulatorSliceNum");
                 if (InsulatorSliceNumElement && InsulatorSliceNumElement->GetText())
                 {
                     newTicketConfig.m_wInsulatorSliceNum = std::stoi(InsulatorSliceNumElement->GetText());
                 }
				 const tinyxml2::XMLElement* LoopTypeElement = TicketsizeElement->FirstChildElement("LoopType");
                 if (LoopTypeElement && LoopTypeElement->GetText())
                 {
                     newTicketConfig.m_eLoopType = CNewTicketConfig::m_vecLoopType(LoopTypeElement->GetText());
                 }
				 const tinyxml2::XMLElement* DetectionUnitElement = TicketsizeElement->FirstChildElement("DetectionUnit");
				 if (DetectionUnitElement && DetectionUnitElement->GetText())
				 {
                     newTicketConfig.m_strDetectionUnit = DetectionUnitElement->GetText();
				 }
                 const tinyxml2::XMLElement* DetectionPersonElement = TicketsizeElement->FirstChildElement("DetectionPerson");
                 if (DetectionPersonElement && DetectionPersonElement->GetText())
                 {
                     newTicketConfig.m_strDetectionPerson = DetectionPersonElement->GetText();
                 }
                 const tinyxml2::XMLElement* CurrentTypeElement = TicketsizeElement->FirstChildElement("CurrentType");
                 if (CurrentTypeElement && CurrentTypeElement->GetText())
                 {
                     newTicketConfig.m_eCurrentType = CNewTicketConfig::m_vecCurrentType(CurrentTypeElement->GetText());
                 }
                 const tinyxml2::XMLElement* StartTimeElement = TicketsizeElement->FirstChildElement("StartTime");
                 if (StartTimeElement && StartTimeElement->GetText())
                 {
                     newTicketConfig.m_strStartTime = StartTimeElement->GetText();
                 }
                 const tinyxml2::XMLElement* EndTimeElement = TicketsizeElement->FirstChildElement("EndTime");
                 if (EndTimeElement && EndTimeElement->GetText())
                 {
                     newTicketConfig.m_strEndTime = EndTimeElement->GetText();
                 }
                 const tinyxml2::XMLElement* RemarkElement = TicketsizeElement->FirstChildElement("Remark");
                 if (RemarkElement && RemarkElement->GetText())
                 {
                     newTicketConfig.m_strRemark = RemarkElement->GetText();
                 }
				 const tinyxml2::XMLElement* MapTicketMearDataElement = TicketsizeElement->FirstChildElement("TicketMearData");
				 if (MapTicketMearDataElement && MapTicketMearDataElement->GetText())
				 {
					 // 测量数据以JSON文本存储,解析为QJsonObject
					 QByteArray jsonBytes = QByteArray::fromStdString(MapTicketMearDataElement->GetText());
					 QJsonParseError parseError;
					 QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonBytes, &parseError);
					 if (parseError.error == QJsonParseError::NoError && jsonDoc.isObject())
						 newTicketConfig.m_mapTicketMearData = jsonDoc.object();
				 }

				 m_vecNewTicketConfig.push_back(newTicketConfig);
				 TicketsizeElement = TicketsizeElement->NextSiblingElement("Size");
			}
		}

	}
	{
		// 读取m_vecNewReportConfig 相关配置数据
        auto NewReportList = config->FirstChildElement("NewReportList");
        if (NewReportList == nullptr)
			return false;
        {
            auto ReportsizeElement = NewReportList->FirstChildElement("Size");
            while (ReportsizeElement != nullptr)
            {
                CNewReportConfig newReportConfig;
                const tinyxml2::XMLElement* DetectionPersonElement = ReportsizeElement->FirstChildElement("DetectionPerson");
                if (DetectionPersonElement && DetectionPersonElement->GetText())
                {
                    newReportConfig.m_strDetectionPerson = DetectionPersonElement->GetText();
                }
                const tinyxml2::XMLElement* DetectionUnitElement = ReportsizeElement->FirstChildElement("DetectionUnit");
                if (DetectionUnitElement && DetectionUnitElement->GetText())
                {
                    newReportConfig.m_strDetectionUnit = DetectionUnitElement->GetText();
                }
                const tinyxml2::XMLElement* ReportIdElement = ReportsizeElement->FirstChildElement("ReportId");
                if (ReportIdElement && ReportIdElement->GetText())
                {
                    newReportConfig.m_strReportId = ReportIdElement->GetText();
                }
                const tinyxml2::XMLElement* WorkPlaceElement = ReportsizeElement->FirstChildElement("WorkPlace");
                if (WorkPlaceElement && WorkPlaceElement->GetText())
                {
                    newReportConfig.m_strWorkPlace = WorkPlaceElement->GetText();
                }
               
                m_vecNewReportConfig.push_back(newReportConfig);
                ReportsizeElement = ReportsizeElement->NextSiblingElement("Size");
            }
        }
	}
	return true;
}

bool CConfigManager::Write(const std::string& filePath)
{
	tinyxml2::XMLDocument doc;
	// 添加 XML 声明
	//doc.InsertEndChild(doc.NewDeclaration());

	// 创建根节点（请根据实际 XML 文件的根节点名称调整，例如 "Config" 或 "Root"）
	tinyxml2::XMLElement* root = doc.NewElement("Config");
	doc.InsertEndChild(root);

	// --- 写入 DeviceControlBoard ---
	{
		tinyxml2::XMLElement* deviceBoard = doc.NewElement("DeviceControlBoard");
		root->InsertEndChild(deviceBoard);

		tinyxml2::XMLElement* ipElem = doc.NewElement("Ip");
		ipElem->SetText(m_memControlBoardConfig.m_strIp.c_str());
		deviceBoard->InsertEndChild(ipElem);

		tinyxml2::XMLElement* portElem = doc.NewElement("Port");
		portElem->SetText(m_memControlBoardConfig.m_wPort);
		deviceBoard->InsertEndChild(portElem);

		tinyxml2::XMLElement* hbElem = doc.NewElement("DeviceHeartBeat");
		hbElem->SetText(m_memControlBoardConfig.m_wDeviceHeartBeat);
		deviceBoard->InsertEndChild(hbElem);

		tinyxml2::XMLElement* fmElem = doc.NewElement("FactoryMode");
		fmElem->SetText(m_memControlBoardConfig.m_bFactoryMode ? 1 : 0);
		deviceBoard->InsertEndChild(fmElem);

		tinyxml2::XMLElement* upElem = doc.NewElement("UpAngle");
		upElem->SetText(m_memControlBoardConfig.m_cUpAngle);
		deviceBoard->InsertEndChild(upElem);

		tinyxml2::XMLElement* downElem = doc.NewElement("DownAngle");
		downElem->SetText(m_memControlBoardConfig.m_cDownAngle);
		deviceBoard->InsertEndChild(downElem);

		tinyxml2::XMLElement* speedElem = doc.NewElement("WalkMotorSpeed");
		speedElem->SetText(m_memControlBoardConfig.m_cWalkMotorSpeed);
		deviceBoard->InsertEndChild(speedElem);

        tinyxml2::XMLElement* ServoElem = doc.NewElement("ServoSpeed");
		ServoElem->SetText(m_memControlBoardConfig.m_cServoSpeed);
        deviceBoard->InsertEndChild(ServoElem);

        tinyxml2::XMLElement* upElem2 = doc.NewElement("UpAngle2");
        upElem2->SetText(m_memControlBoardConfig.m_cUpAngle2);
		deviceBoard->InsertEndChild(upElem2);

        tinyxml2::XMLElement* insuThresholdElem = doc.NewElement("InsuThreshold");
        insuThresholdElem->SetText(m_memControlBoardConfig.m_wInsuThreshold);
		deviceBoard->InsertEndChild(insuThresholdElem);
	}

	// --- 写入 Camera ---
	{
		tinyxml2::XMLElement* camera = doc.NewElement("Camera");
		root->InsertEndChild(camera);

		tinyxml2::XMLElement* leftElem = doc.NewElement("Left");
		leftElem->SetText(m_memCCameraConfig.m_strLeftIp.c_str());
		camera->InsertEndChild(leftElem);

		tinyxml2::XMLElement* midElem = doc.NewElement("Mid");
		midElem->SetText(m_memCCameraConfig.m_strMidIp.c_str());
		camera->InsertEndChild(midElem);

		tinyxml2::XMLElement* rightElem = doc.NewElement("Right");
		rightElem->SetText(m_memCCameraConfig.m_strRightIp.c_str());
		camera->InsertEndChild(rightElem);

		tinyxml2::XMLElement* NewCameraElem = doc.NewElement("NewCamera");
		NewCameraElem->SetText(m_memCCameraConfig.m_bNewCamera ? "1" : "0");
		camera->InsertEndChild(NewCameraElem);

		tinyxml2::XMLElement* UseMainSpElem = doc.NewElement("UseMainSp");
		UseMainSpElem->SetText(m_memCCameraConfig.m_bUseMainSp ? "1" : "0");
		camera->InsertEndChild(UseMainSpElem);

		tinyxml2::XMLElement* MainRtspElem = doc.NewElement("MainRtsp");
		MainRtspElem->SetText(m_memCCameraConfig.m_strMainRtsp.c_str());
		camera->InsertEndChild(MainRtspElem);

		tinyxml2::XMLElement* SubRtspElem = doc.NewElement("SubRtsp");
		SubRtspElem->SetText(m_memCCameraConfig.m_strSubRtsp.c_str());
		camera->InsertEndChild(SubRtspElem);
	}

	// --- 写入 NewTicketConfig ---
    { 
        tinyxml2::XMLElement* newTicketList = doc.NewElement("NewTicketList");
        root->InsertEndChild(newTicketList);
        for (auto& newTicketConfig : m_vecNewTicketConfig)
        {
            tinyxml2::XMLElement* sizeElement = doc.NewElement("Size");
            newTicketList->InsertEndChild(sizeElement);
            {
                tinyxml2::XMLElement* ticketIdElement = doc.NewElement("TicketId");
                ticketIdElement->SetText(newTicketConfig.m_strTicketId.c_str());
                sizeElement->InsertEndChild(ticketIdElement);

                tinyxml2::XMLElement* lineNameElement = doc.NewElement("LineName");
                lineNameElement->SetText(newTicketConfig.m_strLineName.c_str());
                sizeElement->InsertEndChild(lineNameElement);
                
                tinyxml2::XMLElement* bunchTypeElement = doc.NewElement("BunchType");
                bunchTypeElement->SetText(CNewTicketConfig::m_vecBunchType(newTicketConfig.m_eBunchType).c_str());
                sizeElement->InsertEndChild(bunchTypeElement);

                tinyxml2::XMLElement* lineTypeElement = doc.NewElement("InsulatorSliceNum");
                lineTypeElement->SetText(newTicketConfig.m_wInsulatorSliceNum);
                sizeElement->InsertEndChild(lineTypeElement);

                tinyxml2::XMLElement* loopTypeElement = doc.NewElement("LoopType");
                loopTypeElement->SetText(CNewTicketConfig::m_vecLoopType(newTicketConfig.m_eLoopType).c_str());
                sizeElement->InsertEndChild(loopTypeElement);

                tinyxml2::XMLElement* poleNumberElement = doc.NewElement("PoleNumber");
                poleNumberElement->SetText(newTicketConfig.m_strPoleNumber.c_str());
                sizeElement->InsertEndChild(poleNumberElement);

                tinyxml2::XMLElement* detectionUnitElement = doc.NewElement("DetectionUnit");
                detectionUnitElement->SetText(newTicketConfig.m_strDetectionUnit.c_str());
                sizeElement->InsertEndChild(detectionUnitElement);

                tinyxml2::XMLElement* detectionPersonElement = doc.NewElement("DetectionPerson");
                detectionPersonElement->SetText(newTicketConfig.m_strDetectionPerson.c_str());
                sizeElement->InsertEndChild(detectionPersonElement);

                tinyxml2::XMLElement* currentTypeElement = doc.NewElement("CurrentType");
                currentTypeElement->SetText(CNewTicketConfig::m_vecCurrentType(newTicketConfig.m_eCurrentType).c_str());
                sizeElement->InsertEndChild(currentTypeElement);

                tinyxml2::XMLElement* startTimeElement = doc.NewElement("StartTime");
                startTimeElement->SetText(newTicketConfig.m_strStartTime.c_str());
                sizeElement->InsertEndChild(startTimeElement);

                tinyxml2::XMLElement* endTimeElement = doc.NewElement("EndTime");
                endTimeElement->SetText(newTicketConfig.m_strEndTime.c_str());
                sizeElement->InsertEndChild(endTimeElement);

                tinyxml2::XMLElement* remarkElement = doc.NewElement("Remark");
                remarkElement->SetText(newTicketConfig.m_strRemark.c_str());
                sizeElement->InsertEndChild(remarkElement);

				tinyxml2::XMLElement* ticketMearDataElement = doc.NewElement("TicketMearData");
				// 测量数据序列化为JSON文本存储
				QJsonDocument jsonDoc(newTicketConfig.m_mapTicketMearData);
				std::string jsonStr = jsonDoc.toJson(QJsonDocument::Compact).toStdString();
				ticketMearDataElement->SetText(jsonStr.c_str());
				sizeElement->InsertEndChild(ticketMearDataElement);
            }
        }
    }

	// --- 写入 NewReportConfig ---
    {
        tinyxml2::XMLElement* newReportList = doc.NewElement("NewReportList");
        root->InsertEndChild(newReportList);
        for (auto& newReportConfig : m_vecNewReportConfig)
        {
			tinyxml2::XMLElement* sizeElement = doc.NewElement("Size");
			newReportList->InsertEndChild(sizeElement);
			{
				tinyxml2::XMLElement* reportElement = doc.NewElement("ReportId");
                reportElement->SetText(newReportConfig.m_strReportId.c_str());
				sizeElement->InsertEndChild(reportElement);

                tinyxml2::XMLElement* workPlaceElement = doc.NewElement("WorkPlace");
                workPlaceElement->SetText(newReportConfig.m_strWorkPlace.c_str());
                sizeElement->InsertEndChild(workPlaceElement);

                tinyxml2::XMLElement* detectionPersonElement = doc.NewElement("DetectionPerson");
                detectionPersonElement->SetText(newReportConfig.m_strDetectionPerson.c_str());
                sizeElement->InsertEndChild(detectionPersonElement);

                tinyxml2::XMLElement* detectionUnitElement = doc.NewElement("DetectionUnit");
                detectionUnitElement->SetText(newReportConfig.m_strDetectionUnit.c_str());
                sizeElement->InsertEndChild(detectionUnitElement);

			}
        }
    }

	// 保存至文件
	tinyxml2::XMLError eResult = doc.SaveFile(filePath.c_str());
	if (eResult != tinyxml2::XML_SUCCESS)
	{
		return false; //可根据项目规范添加日志记录或异常处理
	}
	return true;
}