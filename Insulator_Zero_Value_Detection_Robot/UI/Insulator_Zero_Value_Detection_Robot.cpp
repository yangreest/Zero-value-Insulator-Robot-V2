#include "Insulator_Zero_Value_Detection_Robot.h"
#include "Config/ConfigManager.h"
#include "Log/ScanS_WriteLog.h"
#include "Protocol/WHSDControlBoradProtocol.h"
#include "Tools/Tools.h"
#include <QTimer>
#include <QScreen>
#include <QApplication>
#include <QMessageBox.h>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QDateTime>
#include <QFileDialog.h>
#include <QVBoxLayout>
#include <random>

Insulator_Zero_Value_Detection_Robot::Insulator_Zero_Value_Detection_Robot(QWidget* parent)
	: QMainWindow(parent), overlayLabel(nullptr)
{
	ui.setupUi(this);
	InitParam();
	InitUI();
	BindAction();
}

Insulator_Zero_Value_Detection_Robot::~Insulator_Zero_Value_Detection_Robot()
{
	continueStreaming = false;
}

void Insulator_Zero_Value_Detection_Robot::InitUI()
{
	showMaximized();
	// 隐藏标题栏
	setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

	ui.label_22->setVisible(false);
	ui.labelTicketLineName->setText("");
	ui.labelPoleNumber->setText("");

	// splitter首次布局前设置，Qt按数值比例分配空间（视频区2:标签页1），之后手动拖拽不受影响
	// 注意：在 showMaximized() 之后调用 setWindowFlags 修改窗口标志会导致窗口销毁并重建，
	// 这会重置 splitter 的布局状态。因此必须延迟到事件循环，等待窗口重建并最大化布局完成后再设置。
	QTimer::singleShot(0, this, [this]() {
		int totalHeight = ui.splitter->height();
		if (totalHeight > 0) {
			// 按 视频区2 : 标签页1 的实际像素比例分配
			ui.splitter->setSizes({ totalHeight * 2 / 3, totalHeight / 3 });
		}
	});

	ui.groupBox_7->setVisible(false);

	// 获取结果，执行下一个
	if (overlayLabel == nullptr)
	{
		overlayLabel = new QLabel(this); // 父对象设置为本窗口！重点
	}

	for (int i = 1; i <= 60; i++)
	{
		QString strName = QString("labelInside%1").arg(i);
		QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (label)
		{
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选：配套底色/边框按需加 */\n    background-color: #1A202B;\n}");
		}
		strName = QString("labelOutside%1").arg(i);
		label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (label)
		{
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选：配套底色/边框按需加 */\n    background-color: #1A202B;\n}");
		}
	}

	for (auto& strNewTicketConfig : m_pConfig->m_vecNewTicketConfig)
	{
		int rowCount = ui.tableWidget_2->rowCount();
		ui.tableWidget_2->insertRow(rowCount);
		SetTicketRow(rowCount, strNewTicketConfig);
	}

	for (auto& strNewReportConfig : m_pConfig->m_vecNewReportConfig)
	{
		int rowCount = ui.tableWidget_3->rowCount();
		ui.tableWidget_3->insertRow(rowCount);
		ui.tableWidget_3->setItem(rowCount, 0, new QTableWidgetItem(QString::number(rowCount + 1)));
		ui.tableWidget_3->item(rowCount, 0)->setData(Qt::UserRole, QVariant::fromValue(strNewReportConfig));
		ui.tableWidget_3->setItem(rowCount, 1, new QTableWidgetItem(QString::fromStdString(strNewReportConfig.m_strReportId)));
		ui.tableWidget_3->setItem(rowCount, 2, new QTableWidgetItem(QString::fromStdString(strNewReportConfig.m_strDetectionUnit)));
		ui.tableWidget_3->setItem(rowCount, 3, new QTableWidgetItem(QString::fromStdString(strNewReportConfig.m_strDetectionPerson)));
		ui.tableWidget_3->setItem(rowCount, 4, new QTableWidgetItem(QString::fromStdString(strNewReportConfig.m_strWorkPlace)));
	}

	// 初始无过滤条件，全部显示
	FilterTicketTable();
	FilterReportTable();

	m_pModelDataWidget = new ModelDataWidget(ui.widget);
	m_activeWidget = m_pModelDataWidget;
	m_activeWidget->load();
	// 构造函数中窗口尚未显示/最大化，此时ui.widget->size()不是最终尺寸；
	// 改用布局管理，让曲线控件自动跟随ui.widget尺寸，避免一次性resize导致启动时不显示
	QVBoxLayout* pChartLayout = new QVBoxLayout(ui.widget);
	pChartLayout->setContentsMargins(0, 0, 0, 0);
	pChartLayout->addWidget(m_activeWidget);
	m_activeWidget->setVisible(true);

	// 设置表格表头填充
	ui.tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	// 或者只拉伸最后一列

	// 如果有其他表格也需要设置
	ui.tableWidget_2->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	ui.tableWidget_3->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

	ui.lineEdit_7->setText(QString::number(m_pConfig->m_memControlBoardConfig.m_cUpAngle));				// 探针向内的角度
    ui.lineEdit_14->setText(QString::number(m_pConfig->m_memControlBoardConfig.m_cDownAngle));			// 探针复原的角度
    ui.lineEdit_13->setText(QString::number(m_pConfig->m_memControlBoardConfig.m_cUpAngle2));			// 探针向外的角度

	ui.comboBox_3->setCurrentIndex(m_pConfig->m_memControlBoardConfig.m_cWalkMotorSpeed);				// 控制电机速度

	// IP地址校验 0‑255.0‑255.0‑255.0‑255
	QRegularExpression ipRx("((2[0-4]\\d|25[0-5]|[01]?\\d\\d?)\\.){3}(2[0-4]\\d|25[0-5]|[01]?\\d\\d?)");
	ui.lineEdit_9->setValidator(new QRegularExpressionValidator(ipRx, ui.lineEdit_9));

	ui.lineEdit_10->setValidator(new QRegularExpressionValidator(ipRx, ui.lineEdit_10));

	ui.lineEdit_9->setText(QString::fromStdString(m_pConfig->m_memControlBoardConfig.m_strIp));			// 设备IP

	ui.lineEdit_10->setText(QString::fromStdString(m_pConfig->m_memCCameraConfig.m_strLeftIp));         // 左摄像头IP	

    ui.lineEdit_15->setText(QString::number(m_pConfig->m_memControlBoardConfig.m_wInsuThreshold));
}

void Insulator_Zero_Value_Detection_Robot::InitParam()
{
	m_wSensorStatus = 0;

	m_wSensorBat = 0;

	m_strFileName = "C:";

	// 使用标志位控制循环
	continueStreaming = true;

	m_wSensorResult = 0;
	m_pDeviceLog = new CWriteLog(WHSD_Tools::GetAbsolutePath("Log\\DeviceLog.txt"), 10000, 250);
	m_pDeviceLog->BeginWork();
	m_pConfig = new CConfigManager();
	m_pConfig->Read(WHSD_Tools::GetAbsolutePath("Config.xml"));
	m_pXInputHelper = new CXInputHelper(0);
	m_pXInputHelper->RegisterControllerStateCallBack(std::bind(
		&Insulator_Zero_Value_Detection_Robot::CallBack_ControllerState, this, std::placeholders::_1,
		std::placeholders::_2));
	m_pXInputHelper->BeginWork();

	m_mapTicketMearData.clear();

	m_CurrentTicketConfig = CNewTicketConfig();


	newTicketDialog = new NewTicketDialog();
	newReportDialog = new NewReportDialog();

	// 获取设备信息

	m_pComDevice = IDeviceCom::GetIDeviceCom(1);
	m_pWHSDControlBoardProtocol = new CWHSDControlBoardProtocol(
		m_pConfig->m_memControlBoardConfig.m_wDeviceHeartBeat);

	auto pWHSDControlBoardProtocol = m_pWHSDControlBoardProtocol;
	auto pDeviceCom = m_pComDevice;
	pDeviceCom->SetParam(m_pConfig->m_memControlBoardConfig.m_strIp.c_str(),
		m_pConfig->m_memControlBoardConfig.m_wPort);
	pDeviceCom->RegisterReadDataCallBack(std::bind(&CWHSDControlBoardProtocol::ReceiveNewData,
		pWHSDControlBoardProtocol, std::placeholders::_1,
		std::placeholders::_2));
	pDeviceCom->RegisterConnectStatusCallBack(std::bind(
		&Insulator_Zero_Value_Detection_Robot::ComDeviceConnectionChanged, this,
		std::placeholders::_1, std::placeholders::_2, 0));

	pWHSDControlBoardProtocol->RegisterAnswerFunction(std::bind(&IDeviceCom::Write, pDeviceCom, std::placeholders::_1, std::placeholders::_2));
	pWHSDControlBoardProtocol->RegisterDeviceLog(std::bind(&CWriteLog::Write, m_pDeviceLog, std::placeholders::_1));
	pWHSDControlBoardProtocol->RegisterDeviceHeartBeat(std::bind(&Insulator_Zero_Value_Detection_Robot::Callback_DeviceHeartBeat, this, std::placeholders::_1, 0));
	pWHSDControlBoardProtocol->RegisterSensorDataCallBack(std::bind(&Insulator_Zero_Value_Detection_Robot::CallBack_SensorValue, this, std::placeholders::_1));
	pWHSDControlBoardProtocol->RegisterZeroDataCallBack(std::bind(&Insulator_Zero_Value_Detection_Robot::CallBack_ZeroValue, this, std::placeholders::_1));
	//	std::placeholders::_1,
	//	std::placeholders::_2, std::placeholders::_3));
	//pWHSDControlBoardProtocol->RegisterXRaySendResult(xRayResult);
	pDeviceCom->BeginWork();
	pWHSDControlBoardProtocol->BeginWork();

	if (m_pConfig->m_memCCameraConfig.m_bNewCamera)
	{
		std::thread td(&Insulator_Zero_Value_Detection_Robot::NewCameraConnect, this);
		td.detach();
	}
	else
	{
		m_strLeftIp = m_pConfig->m_memCCameraConfig.m_strLeftIp;
		m_strRightIp = m_pConfig->m_memCCameraConfig.m_strRightIp;

		m_pC1 = ICameraBase::GetCameraObj(0);
		m_pC2 = ICameraBase::GetCameraObj(0);

		auto handleR = reinterpret_cast<HWND>(ui.label_22->winId());// winId的功能是获取窗口句柄
		auto handleL = reinterpret_cast<HWND>(ui.label_9->winId());

		m_pC1->RegisterVideoViewHandle(handleL);
		m_pC2->RegisterVideoViewHandle(handleR);

		std::thread td(&Insulator_Zero_Value_Detection_Robot::CameraConnect, this);
		td.detach();
	}

}

void Insulator_Zero_Value_Detection_Robot::BindAction()
{
	m_pTimer = new QTimer(this);
	m_pTimer->setInterval(100);
	connect(m_pTimer, &QTimer::timeout, this, &Insulator_Zero_Value_Detection_Robot::On_timer_timeout);
	m_pTimer->start();

	m_pTimerInput = new QTimer(this);
	m_pTimerInput->setInterval(10);
	connect(m_pTimerInput, &QTimer::timeout, this, &Insulator_Zero_Value_Detection_Robot::On_timerInput_timeout);
	m_pTimerInput->start();

	connect(ui.pushButton, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_TurnOnAll_Click);
	connect(ui.pushButton_2, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_TurnOffAll_Click);
	connect(ui.pBClose, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Close_Click);
	connect(ui.pBInspection, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Inspection_Click);
	connect(ui.pBticket, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Ticket_Click);
	connect(ui.pBSetting, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_pBSetting_Click);
	connect(ui.pBreport, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Report_Click);
	connect(ui.pushButton_4, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Setting_Click);
	connect(ui.pBNewTicket, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_NewTicket_Click);
	connect(ui.pBNewReport, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_NewReport_Click);

	connect(ui.pBDeleteTicket, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_DeleteTicket_Click);
	connect(ui.pBChangeTicket, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_ChangeTicket_Click);
	connect(ui.pBLoadTicket, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_LoadTicket_Click);

	connect(newTicketDialog, &NewTicketDialog::NewTicketSignal, this, &Insulator_Zero_Value_Detection_Robot::On_NewTicketSignal);
	connect(newTicketDialog, &NewTicketDialog::ChangeTicketSignal, this, &Insulator_Zero_Value_Detection_Robot::On_ChangeTicketSignal);
	connect(newReportDialog, &NewReportDialog::NewReportSignal, this, &Insulator_Zero_Value_Detection_Robot::On_NewReportSignal);
	connect(newReportDialog, &NewReportDialog::ChangeReportSignal, this, &Insulator_Zero_Value_Detection_Robot::On_ChangeReportSignal);

	connect(ui.pBTest, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Test_Click);
	connect(ui.pBRetest, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Retest_Click);

	connect(ui.pushButton_14, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_forword_Click);// 前进
	connect(ui.pushButton_15, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_backward_Click);
	connect(ui.pushButton_26, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_stop_Click);
	connect(ui.pushButton_17, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_neddle1_Click);
	connect(ui.pushButton_25, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_neddle2_Click);
	connect(ui.pushButton_27, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_neddle3_Click);
	connect(ui.pushButton_28, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_mear_Click);

	connect(ui.comboBox_2, &QComboBox::currentIndexChanged, this, &Insulator_Zero_Value_Detection_Robot::On_combobox_currentIndexChanged);
	connect(ui.comboBox, &QComboBox::currentIndexChanged, this, &Insulator_Zero_Value_Detection_Robot::On_combobox_currentIndexChanged);

	// 工单列表过滤：线路名称 + 检测人员（实时过滤，也可点查询按钮）
	connect(ui.lineEdit, &QLineEdit::textChanged, this, &Insulator_Zero_Value_Detection_Robot::FilterTicketTable);
	connect(ui.lineEdit_3, &QLineEdit::textChanged, this, &Insulator_Zero_Value_Detection_Robot::FilterTicketTable);
	connect(ui.pushButton_10, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::FilterTicketTable);
	connect(ui.pushButton_11, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_ResetTicket_Click);

	// 报告列表过滤：线路信息 + 检测人员（实时过滤）
	connect(ui.lineEdit_4, &QLineEdit::textChanged, this, &Insulator_Zero_Value_Detection_Robot::FilterReportTable);
	connect(ui.lineEdit_6, &QLineEdit::textChanged, this, &Insulator_Zero_Value_Detection_Robot::FilterReportTable);
	connect(ui.pushButton_18, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_ResetReport_Click);

    connect(ui.pushButton_29, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_SaveInsuThreshold_Click);


	ui.pBInspection->setChecked(true);
	On_Inspection_Click();
}

void Insulator_Zero_Value_Detection_Robot::CallBack_ControllerState(int t, const ControllerState* p)
{
	std::lock_guard<std::mutex> g(m_mutexXInput);
	memcpy(&m_memControllerState, p, sizeof(ControllerState));
}

void Insulator_Zero_Value_Detection_Robot::On_timer_timeout()
{
	if (m_nTimeCount++ % 10 == 0)
	{
		auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 2, 0);
		//m_pComDevice->Write(cmds.data(), cmds.size());
		cmds = CWHSDControlBoardProtocol::SensorCmd(0, 3, 0);
		//m_pComDevice->Write(cmds.data(), cmds.size());
		cmds = CWHSDControlBoardProtocol::SensorCmd(0, 4, 0);
		//m_pComDevice->Write(cmds.data(), cmds.size());
	}

	ui.label_34->setText(QString::number(m_nHeartBeatCount));
	ui.label_3->setText(m_bControlBroadConnected ? "已连接" : "未连接");
	const auto memDeviceHeartBeat = m_memDeviceHeartBeat;
	std::string strWalkingMotorStatus("未知");
	switch (memDeviceHeartBeat.m_vectorWalkingMotorStatus.front().m_cDeviceStatus)
	{
	case 0:
	case 3:
	{
		strWalkingMotorStatus = "停止";
		break;
	}
	case 1:
	{
		strWalkingMotorStatus = "右运行";
		break;
	}
	case 2:
	{
		strWalkingMotorStatus = "左运行";
		break;
	}
	case 4:
	{
		strWalkingMotorStatus = "故障";
		break;
	}
	default:
	{
		break;
	}
	}
	ui.label->setText(QString::fromStdString(strWalkingMotorStatus));
	strWalkingMotorStatus = "未知";
	switch (memDeviceHeartBeat.m_vectorSafetyMotorStatus[0].m_cDeviceStatus)
	{
	case 0:
	case 3:
	{
		strWalkingMotorStatus = "停止";
		break;
	}
	case 1:
	case 2:
	{
		strWalkingMotorStatus = "运行中";
		break;
	}
	case 4:
	{
		strWalkingMotorStatus = "故障";
		break;
	}
	default:
	{
		break;
	}
	}

	ui.label_6->setText(QString::fromStdString(strWalkingMotorStatus));
	ui.label_8->setText(memDeviceHeartBeat.m_cMainPowerSupply > 0 ? "开" : "关");
	if (memDeviceHeartBeat.m_cBattery > 100)
	{
		ui.label_13->setText("未知");
	}
	else
	{
		ui.label_13->setText(QString::number(memDeviceHeartBeat.m_cBattery));
	}

	if (m_wSensorBat > 100)
	{
		ui.label_15->setText("未知");
	}
	else
	{
		ui.label_15->setText(QString::number(m_wSensorBat));
	}
	switch (m_wSensorStatus)
	{
	case 0:
	{
		ui.label_17->setText("待机");
		break;
	}
	case 1:
	{
		ui.label_17->setText("触发");
		break;
	}
	case 2:
	{
		ui.label_17->setText("触发完成");
		break;
	}
	case 3:
	{
		ui.label_17->setText("未找到设备");
		break;
	}
	default:
	{
		break;
	}
	}
	switch (m_wSensorResult)
	{
	case 0:
	{
		//ui.label_11->setText("零值");
		break;
	}
	case 1:
	{
		//ui.label_11->setText("正常");
		break;
	}
	case 2:
	{
		//ui.label_11->setText("未知");
		break;
	}
	case 3:
	{
		//ui.label_11->setText("未找到设备");
		break;
	}
	default:
	{
		break;
	}
	}
}

void Insulator_Zero_Value_Detection_Robot::On_timerInput_timeout()
{
	ControllerState tp;
	{
		std::lock_guard<std::mutex> g(m_mutexXInput);
		memcpy(&tp, &m_memControllerState, sizeof(ControllerState));
	}


	if (!m_bLastButton && tp.buttons[5])
	{
		auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 1, 0);

		m_pComDevice->Write(cmds.data(), cmds.size());
	}
	m_bLastButton = tp.buttons[5];

	//ui.label_29->setText(m_bLastButton ? "ON" : "OFF");
	if (m_nLastDir == 0)
	{
		switch (tp.dpad)
		{
		case 1:
		{
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
				m_pConfig->m_memControlBoardConfig.m_cUpAngle);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("内上");
			break;
		}
		case 2:
		{
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x01, 0b11, 0x01, m_pConfig->m_memControlBoardConfig.m_cWalkMotorSpeed);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("右");
			break;
		}
		case 3:
		{
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
				m_pConfig->m_memControlBoardConfig.m_cDownAngle);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("复原");
			break;
		}
		case 4:
		{
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x01, 0b11, 0x02, m_pConfig->m_memControlBoardConfig.m_cWalkMotorSpeed);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("左");
			break;
		}
		case 5: // 探针向外
		{
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
				m_pConfig->m_memControlBoardConfig.m_cUpAngle2);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("外上");
			break;
		}
		case 0:
		default:
		{
			ui.label_29->setText("");
			break;
		}
		}
	}
	else if (m_nLastDir == 2 || m_nLastDir == 4)
	{
		if (tp.dpad == 0)
		{
			auto cmds = CWHSDControlBoardProtocol::DeviceStop(0x01);
			m_pComDevice->Write(cmds.data(), cmds.size());
		}
	}
	m_nLastDir = tp.dpad;
}

void Insulator_Zero_Value_Detection_Robot::ComDeviceConnectionChanged(const bool connected, int guid, int index)
{
	m_bControlBroadConnected = connected;
}

void Insulator_Zero_Value_Detection_Robot::RefreshControllerState(const ControllerState* p)
{
	// 刷新控制器状态
	std::lock_guard<std::mutex> g(m_mutexXInput);
	memcpy(&m_memControllerState, p, sizeof(ControllerState));
}

void Insulator_Zero_Value_Detection_Robot::CallBack_SensorValue(CSensorData* p)
{
	switch (p->m_cCmd)
	{
	case 2:
	{
		m_wSensorStatus = p->m_wValue;
		break;
	}
	case 3:
	{
		m_wSensorBat = p->m_wValue;
		break;
	}
	case 4:
	{
		m_wSensorResult = p->m_wValue;
		break;
	}
	default:
	{
		break;
	}
	}
}

void Insulator_Zero_Value_Detection_Robot::CallBack_ZeroValue(float* p)
{
	float value = p[0];

	overlayLabel->setWindowFlags(Qt::Widget);
	overlayLabel->setStyleSheet("background-color:rgba(0,0,0,20);color:white;font-size:20px;");
	overlayLabel->setText(QString("当前检测结果：%1MΩ").arg(value));
	overlayLabel->setMinimumWidth(250);

	// 铺满整个窗口，也可以自定义大小位置
	// 显示在label_9 右下角，获取绝对的坐标
	overlayLabel->move(ui.label_9->mapToGlobal(QPoint(ui.label_9->width() - overlayLabel->width(), ui.label_9->height() - overlayLabel->height())));
	//overlayLabel->setGeometry(ui.label_9->geometry().x(), ui.label_9->geometry().y(), 50, 200);

	overlayLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

	// 默认显示
	overlayLabel->show();

	QString strSide = ui.comboBox_2->currentText();
	QString strDira = ui.comboBox->currentText();
	m_mapTicketMearData[strSide][strDira].push_back(value);
	QVector<float> vecData = m_mapTicketMearData[strSide][strDira];
	bool visible = (m_CurrentTicketConfig.m_eBunchType == CNewTicketConfig::BunchType::eDouble);

	// 每获得一个测量值，填充到表格对应列的空单元格并绘制曲线（回调在协议线程，切到UI线程执行）
	// 双联时每相拆为两列：奇数次测量为内侧，偶数次为外侧
	QString strHeader = strDira;
	if (visible)
		strHeader += (vecData.size() % 2 == 1) ? QStringLiteral("内侧") : QStringLiteral("外侧");
	ModelDataWidget* pModelDataWidget = m_pModelDataWidget;
	double dValue = value;
	QMetaObject::invokeMethod(this, [pModelDataWidget, strHeader, dValue]() {
		if (pModelDataWidget)
			pModelDataWidget->appendValue(strHeader, dValue);
	}, Qt::QueuedConnection);

	if (visible) // 双联
	{
		if (vecData.size() % 2 == 1)
		{
			// 奇数是内侧
			QString strName = QString("labelInside%1").arg(vecData.size() / 2 + 1);
			QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
			if (!label)return;
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选：配套底色/边框按需加 */\n    background-color: #10b981;\n}");
		}
		else
		{
			// 偶数是外侧
			QString strName = QString("labelOutside%1").arg(vecData.size() / 2 );
			QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
			if (!label)return;
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选：配套底色/边框按需加 */\n    background-color: #10b981;\n}");
		}
	}
	else //单联
	{
		// 奇数是内侧
		QString strName = QString("labelInside%1").arg(vecData.size());
		QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (!label)return;
		label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选：配套底色/边框按需加 */\n    background-color: #10b981;\n}");
	}
}

void Insulator_Zero_Value_Detection_Robot::CameraConnect()
{
	int initStatus = 0;
	bool needBreak = false;
	while (true)
	{
		if (needBreak)
		{
			break;
		}
		switch (initStatus)
		{
		case 0:
		{
			if (m_pC1->Init({}))
			{
				initStatus = 1;
			}
			break;
		}
		case 1:
		{
			if (m_pC1->Connect(m_strLeftIp, 0, "", ""))
			{
				initStatus = 2;
			}
			break;
		}
		case 2:
		{
			if (m_pC2->Connect(m_strRightIp, 0, "", ""))
			{
				initStatus = 3;
			}
			break;
		}
		//case 3:
		//{
		//	if (m_pC3->Connect(m_strRightIp, 0, "", ""))
		//	{
		//		initStatus = 4;
		//	}
		//	break;
		//}
		default:
		{
			break;
		}
		}
	}
}

void Insulator_Zero_Value_Detection_Robot::NewCameraConnect()
{
	// 使用项目配置管理器获取RTSP参数

	std::string rtsp_url;
	if (m_pConfig->m_memCCameraConfig.m_bUseMainSp)
	{
		rtsp_url = m_pConfig->m_memCCameraConfig.m_strMainRtsp;
	}
	else
	{
		rtsp_url = m_pConfig->m_memCCameraConfig.m_strSubRtsp;
	}

	cv::VideoCapture cap;

	// 设置低延迟参数
	cap.set(cv::CAP_PROP_BUFFERSIZE, 1);  // 最小化缓冲区

	// 尝试打开RTSP流
	bool isOpen = cap.open(rtsp_url, cv::CAP_FFMPEG);

	if (!isOpen)
	{
		qDebug() << "Failed to open RTSP stream: " << rtsp_url;
		return; // 不应该返回-1，因为这不是main函数
	}

	cv::Mat frame;


	while (continueStreaming)
	{
		// 低延迟处理：获取最新帧
		if (cap.grab())
		{
			cap.retrieve(frame);

			if (!frame.empty())
			{
				//cv::imshow("RTSP Low Delay", frame);
				// 将frame 转成QImage显示在lable上
				QImage qImg = Mat2QImage(frame);
				ui.label->setPixmap(QPixmap::fromImage(qImg));

				//// 检查退出条件
				//int key = cv::waitKey(1) & 0xFF;
				//if (key == 27) // ESC键退出
				//{
				//	break;
				//}
			}
			else
			{
				qDebug() << "Frame is empty";
				break;
			}
		}
		else
		{
			qDebug() << "Failed to grab frame";
			break;
		}
	}
	// 清理资源
	cap.release();
}



// 函数实现
QImage Insulator_Zero_Value_Detection_Robot::Mat2QImage(const cv::Mat& mat)
{
	// 处理空矩阵
	if (mat.empty()) {
		return QImage();
	}

	// 如果是彩色图像（BGR -> RGB）
	if (mat.channels() == 3) {
		cv::Mat rgb;
		cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
		return QImage((const unsigned char*)rgb.data,
			rgb.cols,
			rgb.rows,
			rgb.step,
			QImage::Format_RGB888);
	}
	// 如果是灰度图像
	else if (mat.channels() == 1) {
		return QImage((const unsigned char*)mat.data,
			mat.cols,
			mat.rows,
			mat.step,
			QImage::Format_Grayscale8); // Qt 5.13+ 支持，更早版本可用 Format_Indexed8
	}
	// 如果是RGBA图像
	else if (mat.channels() == 4) {
		cv::Mat rgba;
		cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
		return QImage((const unsigned char*)rgba.data,
			rgba.cols,
			rgba.rows,
			rgba.step,
			QImage::Format_RGBA8888);
	}

	// 对于其他通道数，先转换为RGB
	cv::Mat rgb;
	if (mat.channels() == 3) {
		cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
	}
	else {
		cv::cvtColor(mat, rgb, cv::COLOR_GRAY2RGB);
	}

	return QImage((const unsigned char*)rgb.data,
		rgb.cols,
		rgb.rows,
		rgb.step,
		QImage::Format_RGB888);
}

void Insulator_Zero_Value_Detection_Robot::Callback_DeviceHeartBeat(const CDeviceHeartBeat& b, int nComdeviceIndex)
{
	m_mutexDeviceInfoLock.lock();
	m_memDeviceHeartBeat = b;
	m_time_LastHeartBeatTime.GetCurTime();
	m_nHeartBeatCount++;
	m_mutexDeviceInfoLock.unlock();
}


void Insulator_Zero_Value_Detection_Robot::On_TurnOnAll_Click(bool bState)
{
	if (!bState)
	{
		On_TurnOffAll_Click();
	}
	else
	{
		auto cmds = CWHSDControlBoardProtocol::TurnOnAll();
		m_pComDevice->Write(cmds.data(), cmds.size());
	}
}

void Insulator_Zero_Value_Detection_Robot::On_TurnOffAll_Click()
{
	auto cmds = CWHSDControlBoardProtocol::TurnOffAll();
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_Close_Click()
{
	this->close();
}

void Insulator_Zero_Value_Detection_Robot::On_Inspection_Click()
{
	ui.stackedWidget_3->setCurrentIndex(0);
	if (overlayLabel) overlayLabel->show();
}

void Insulator_Zero_Value_Detection_Robot::On_Ticket_Click()
{
	ui.stackedWidget_3->setCurrentIndex(1);
	if (overlayLabel) overlayLabel->hide();
}

void Insulator_Zero_Value_Detection_Robot::On_pBSetting_Click()
{
	ui.stackedWidget_3->setCurrentIndex(3);
	if (overlayLabel) overlayLabel->hide();
}

void Insulator_Zero_Value_Detection_Robot::On_ZeroTest_Click()
{
	auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 1, 0);

	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_Setting_Click()
{
	//if (xmlManagerWindow == nullptr)
	//{
	//	xmlManagerWindow = new XmlManagerWindow();
	//}
	//xmlManagerWindow->show();
}

void Insulator_Zero_Value_Detection_Robot::On_Report_Click()
{
	ui.stackedWidget_3->setCurrentIndex(2);
	if (overlayLabel) overlayLabel->hide();
}

void Insulator_Zero_Value_Detection_Robot::captureCurrentWindow()
{
	// 获取当前窗口的句柄（Windows）/ID（Linux）
	WId windowId = ui.label_9->winId();

	// 获取当前窗口所在的屏幕
	QScreen* screen = QApplication::screenAt(ui.label_9->pos());
	if (!screen) {
		screen = QApplication::primaryScreen();
	}

	// 截取指定窗口
	QPixmap pixmap = screen->grabWindow(windowId);

	// 保存截图
	savePixmap(pixmap);
}

void Insulator_Zero_Value_Detection_Robot::On_SetFileName_Click()
{
	// 此处弹出对话框选择一个文件夹
	QString filePath = QFileDialog::getExistingDirectory(this, "选择文件夹");
	if (!filePath.isEmpty()) {
		m_strFileName = filePath;
		return;
	}
	QMessageBox::information(this, "提示", "文件保存失败");
}

void Insulator_Zero_Value_Detection_Robot::On_NewTicket_Click()
{
	newTicketDialog->show();
}

void Insulator_Zero_Value_Detection_Robot::On_NewReport_Click()
{
	//获取当前工单的ID
	std::string strTicketId = m_CurrentTicketConfig.m_strTicketId;
	if (m_CurrentTicketConfig.m_bGenerateReport)
	{
		// 已存在报告，是否需要重新生成？
		QMessageBox::StandardButton reply = QMessageBox::question(this, "提示", "已存在报告，是否需要重新生成？", QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) 
		{
			// 删除报告
			for (int i = 0; i < ui.tableWidget_3->rowCount(); i++) 
			{
				QTableWidgetItem* item = ui.tableWidget_3->item(i, 1);
                if (item && item->data(Qt::UserRole).value<CNewReportConfig>().m_strReportId == strTicketId) {
					ui.tableWidget_3->removeRow(i);
					break;
				}
			}
        }
        else  return;
	}
	CNewReportConfig m_memNewReportConfig;
    m_memNewReportConfig.m_strReportId = strTicketId;
    newReportDialog->SetReport(m_memNewReportConfig);

	newReportDialog->show();
}

void Insulator_Zero_Value_Detection_Robot::On_DeleteTicket_Click()
{
	// 删除tableWidget_2选中的行
	int row = ui.tableWidget_2->currentRow();
	if (row >= 0) {
		ui.tableWidget_2->removeRow(row);
	}
}

void Insulator_Zero_Value_Detection_Robot::On_ChangeTicket_Click()
{
	int row = ui.tableWidget_2->currentRow();
	if (row < 0) {
		return;
	}

	CNewTicketConfig m_memNewTicketConfig = ui.tableWidget_2->item(row, 0)->data(Qt::UserRole).value<CNewTicketConfig>();
	newTicketDialog->SetTicket(m_memNewTicketConfig);
	newTicketDialog->show();
}

void Insulator_Zero_Value_Detection_Robot::On_LoadTicket_Click()
{
	int row = ui.tableWidget_2->currentRow();
	if (row < 0)return;
	m_CurrentTicketConfig = ui.tableWidget_2->item(row, 0)->data(Qt::UserRole).value<CNewTicketConfig>();

	ui.labelTicketLineName->setText(QString::fromStdString(m_CurrentTicketConfig.m_strLineName));
	ui.labelPoleNumber->setText(QString::fromStdString(m_CurrentTicketConfig.m_strPoleNumber));

	bool visible = (m_CurrentTicketConfig.m_eBunchType == CNewTicketConfig::BunchType::eDouble);
	SetVisibles(visible, m_CurrentTicketConfig.m_wInsulatorSliceNum);

	ui.comboBox->clear();

	QStringList phaseList;
	if (m_CurrentTicketConfig.m_eLoopType == CNewTicketConfig::LoopType::eOne)
	{
		phaseList << "A相" << "B相" << "C相";
	}
	else if (m_CurrentTicketConfig.m_eLoopType == CNewTicketConfig::LoopType::eTwo)
	{
		phaseList << "右A" << "右B" << "右C" << "左A" << "左B" << "左C";
	}
	else if (m_CurrentTicketConfig.m_eLoopType == CNewTicketConfig::LoopType::eFour)
	{
		phaseList << "左上A" << "左上B" << "左上C" << "左下A" << "左下B" << "左下C" << "右上A" << "右上B" << "右上C" << "右下A" << "右下B" << "右下C";
	}
	ui.comboBox->addItems(phaseList);

	// 表格列随comboBox的item，行随片数；双联时每相拆为内侧/外侧两列，每相各占一片数的行
	QStringList tableHeaders;
	bool bDouble = (m_CurrentTicketConfig.m_eBunchType == CNewTicketConfig::BunchType::eDouble);
	if (bDouble)
	{
		for (const QString& strPhase : phaseList)
			tableHeaders << strPhase + QStringLiteral("内侧") << strPhase + QStringLiteral("外侧");
	}
	else
	{
		tableHeaders = phaseList;
	}
	if (m_pModelDataWidget)
		m_pModelDataWidget->setTableLayout(tableHeaders, m_CurrentTicketConfig.m_wInsulatorSliceNum);

	ui.comboBox->setEnabled(true);
	ui.comboBox_2->setEnabled(true);
	ui.pBTest->setEnabled(true);
	ui.pBRetest->setEnabled(true);
}

void Insulator_Zero_Value_Detection_Robot::On_Test_Click()
{
	if (m_CurrentTicketConfig.m_strTicketId == "")
	{
        QMessageBox::information(this, "提示", "请加载一个工单");
		return;
	}
	QString strDira = ui.comboBox->currentText();
	QString strSide = ui.comboBox_2->currentText();
	QVector<float> vecData = m_mapTicketMearData[strSide][strDira];
	if (vecData.size() >= m_CurrentTicketConfig.m_wInsulatorSliceNum)
	{
        QMessageBox::information(this, "提示", "请 换相 或换 号侧 ！");
        return;
	}

	auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 1, 0);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_Retest_Click()
{
	// 删除最后一个数据
	QString strDira = ui.comboBox->currentText();
	QString strSide = ui.comboBox_2->currentText();
	QVector<float>& vecData = m_mapTicketMearData[strSide][strDira];
	if (vecData.isEmpty())return;

	// 被删除的是第size个测量值：双联时奇数次为内侧、偶数次为外侧，据此定位列
	QString strHeader = strDira;
	bool bDouble = (m_CurrentTicketConfig.m_eBunchType == CNewTicketConfig::BunchType::eDouble);
	if (bDouble)
		strHeader += (vecData.size() % 2 == 1) ? QStringLiteral("内侧") : QStringLiteral("外侧");
	vecData.pop_back();

	// 同步删除表格/曲线中最近一个测量值，等待重测值回填
	if (m_pModelDataWidget)
		m_pModelDataWidget->removeLastValue(strHeader);

	auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 1, 0);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_SaveInsuThreshold_Click()
{
    m_pConfig->m_memControlBoardConfig.m_wInsuThreshold = ui.lineEdit_15->text().toInt();

	m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
}

void Insulator_Zero_Value_Detection_Robot::On_forword_Click()
{
	auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x01, 0b11, 0x01, 0x00);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_backward_Click()
{
	auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x01, 0b11, 0x02, 0x00);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_neddle1_Click()
{
	auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
		m_pConfig->m_memControlBoardConfig.m_cUpAngle);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_neddle2_Click()
{
	auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
		m_pConfig->m_memControlBoardConfig.m_cUpAngle2);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_neddle3_Click()
{
	auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
		m_pConfig->m_memControlBoardConfig.m_cDownAngle);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_stop_Click()
{
	auto cmds = CWHSDControlBoardProtocol::DeviceStop(0x01);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_mear_Click()
{
	auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 1, 0);

	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_combobox_currentIndexChanged(int index)
{
	QString strDira = ui.comboBox->currentText();
	QString strSide = ui.comboBox_2->currentText();
	QVector<float> vecData = m_mapTicketMearData[strSide][strDira];
	bool visible = (m_CurrentTicketConfig.m_eBunchType == CNewTicketConfig::BunchType::eDouble);

	for (int i = 1; i <= 60; i++)
	{
		QString strName = QString("labelInside%1").arg(i);
		QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (label)
		{
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选：配套底色/边框按需加 */\n    background-color: #1A202B;\n}");
		}
		strName = QString("labelOutside%1").arg(i);
		label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (label)
		{
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选：配套底色/边框按需加 */\n    background-color: #1A202B;\n}");
		}
	}

	if (visible) // 双联
	{
		for (int i = 1; i <= (vecData.size() + 1) / 2; i++)
		{
			QString strName = QString("labelInside%1").arg(i);
			QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
			if (label)
			{
				// TODO:根据数据设置颜色
				float valueInside = vecData[2 * i - 2];
				label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选：配套底色/边框按需加 */\n    background-color: #10b981;\n}");
			}
			strName = QString("labelOutside%1").arg(i);
			label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
			if(2 * i - 1>=vecData.size())continue;
			if (label)
			{
				float valueOutside = vecData[2 * i - 1];
				label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选：配套底色/边框按需加 */\n    background-color: #10b981;\n}");
			}
		}
	}
	else // 单联
	{
		for (int i = 1; i <= vecData.size(); i++)
		{
			QString strName = QString("labelInside%1").arg(i);
			QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
			if(!label) continue;
            label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选：配套底色/边框按需加 */\n    background-color: #10b981;\n}");
		}
	}
	
}

void Insulator_Zero_Value_Detection_Robot::On_ChangeTicketSignal(CNewTicketConfig strTicket)
{
	// 更新tableWidget_2选中行的数据
	int currentRow = ui.tableWidget_2->currentRow();

	SetTicketRow(currentRow, strTicket);
	FilterTicketTable();

	// 根据m_strReportId找到m_pConfig->m_vecNewTicketConfig 并覆盖strTicket
	for (auto& ticket : m_pConfig->m_vecNewTicketConfig)
	{
		if (ticket.m_strTicketId == strTicket.m_strTicketId)
		{
			ticket = strTicket;
			break;
		}
	}
	m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
}

void Insulator_Zero_Value_Detection_Robot::On_NewReportSignal(CNewReportConfig strReport)
{
	//strReport.m_strReportId = GenerateUniqueReportId();
	int rowCount = ui.tableWidget_3->rowCount();
	ui.tableWidget_3->insertRow(rowCount);
	strReport.m_mapTicketMearData = m_mapTicketMearData;

	ui.tableWidget_3->setItem(rowCount, 0, new QTableWidgetItem(QString::number(rowCount + 1)));
	ui.tableWidget_3->item(rowCount, 0)->setData(Qt::UserRole, QVariant::fromValue(strReport));
	ui.tableWidget_3->setItem(rowCount, 1, new QTableWidgetItem(QString::fromStdString(strReport.m_strReportId)));
	ui.tableWidget_3->setItem(rowCount, 2, new QTableWidgetItem(QString::fromStdString(strReport.m_strDetectionUnit)));
	ui.tableWidget_3->setItem(rowCount, 3, new QTableWidgetItem(QString::fromStdString(strReport.m_strDetectionPerson)));
	ui.tableWidget_3->setItem(rowCount, 4, new QTableWidgetItem(QString::fromStdString(strReport.m_strWorkPlace)));
	FilterReportTable();

	m_pConfig->m_vecNewReportConfig.push_back(strReport);
	m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));

	// 将m_mapTicketMearData按照map层级和顺序全部保存到csv
	QString path = QString("MearData_%1.csv").arg(QString::fromStdString(strReport.m_strReportId));
    std::string strCsvPath = WHSD_Tools::GetAbsolutePath(path.toStdString().c_str());
    QFile csvFile(QString::fromStdString(strCsvPath));
    if (csvFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
		//csvFile.write("\xEF\xBB\xBF", 3);

        QTextStream stream(&csvFile);
		stream.setEncoding(QStringConverter::Utf8);
		stream.setGenerateByteOrderMark(true);
        //stream << "\xEF\xBB\xBF"; // UTF-8 BOM，防止Excel打开中文乱码
        //stream << "侧别,方向,序号,测量值\n";

        // 第一层map：侧别（QMap按key有序）
        for (auto itSide = m_mapTicketMearData.constBegin(); itSide != m_mapTicketMearData.constEnd(); ++itSide)
        {
            // 第二层map：方向（QMap按key有序）
            for (auto itDira = itSide.value().constBegin(); itDira != itSide.value().constEnd(); ++itDira)
            {
				stream << itSide.key() << ","
					<< itDira.key() << ",";
                // vector：按存入顺序依次导出
                const QVector<float>& vecData = itDira.value();
                for (int i = 0; i < vecData.size(); ++i)
                {
             
					stream << QString::number(vecData[i], 'f', 3) << ",";
                }
				stream << "\n";
            }
        }
        csvFile.close();
    }
    else
    {
        QMessageBox::warning(this, "错误", QString("测量数据CSV保存失败：\n%1").arg(strCsvPath));
    }
}

void Insulator_Zero_Value_Detection_Robot::On_ChangeReportSignal(CNewReportConfig strReport)
{
	int rowCount = ui.tableWidget_3->currentRow();
	ui.tableWidget_3->setItem(rowCount, 0, new QTableWidgetItem(QString::number(rowCount + 1)));
	ui.tableWidget_3->item(rowCount, 0)->setData(Qt::UserRole, QVariant::fromValue(strReport));
	ui.tableWidget_3->setItem(rowCount, 1, new QTableWidgetItem(QString::fromStdString(strReport.m_strReportId)));
	ui.tableWidget_3->setItem(rowCount, 2, new QTableWidgetItem(QString::fromStdString(strReport.m_strDetectionUnit)));
	ui.tableWidget_3->setItem(rowCount, 3, new QTableWidgetItem(QString::fromStdString(strReport.m_strDetectionPerson)));
	ui.tableWidget_3->setItem(rowCount, 4, new QTableWidgetItem(QString::fromStdString(strReport.m_strWorkPlace)));
	FilterReportTable();


	// 根据m_strReportId找到m_pConfig->m_vecNewReportConfig 并覆盖strReport
	for (auto& report : m_pConfig->m_vecNewReportConfig)
	{
		if (report.m_strReportId == strReport.m_strReportId)
		{
			report = strReport;
			break;
		}
	}
	m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
}

void Insulator_Zero_Value_Detection_Robot::On_NewTicketSignal(CNewTicketConfig config)
{
	config.m_strTicketId = GenerateUniqueTicketId();  // 设置唯一 ID
	// 在tableWidget_2中新增一行
	int rowCount = ui.tableWidget_2->rowCount();
	ui.tableWidget_2->insertRow(rowCount);
	SetTicketRow(rowCount, config);
	FilterTicketTable();

	m_pConfig->m_vecNewTicketConfig.push_back(config);
	m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
	return;
}

// 保存截图到文件
void Insulator_Zero_Value_Detection_Robot::savePixmap(const QPixmap& pixmap)
{
	if (pixmap.isNull()) {
		QMessageBox::warning(this, "错误", "截图失败，像素数据为空");
		return;
	}

	//// 弹出保存文件对话框
	//QString filePath = QFileDialog::getSaveFileName(
	//	this,
	//	"保存截图",
	//	QString("截图_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmss")),
	//	"PNG图片 (*.png);;JPG图片 (*.jpg);;BMP图片 (*.bmp)"
	//);

	QString filePath = QString("%1/%2.png").arg(m_strFileName).arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmss"));

	if (!filePath.isEmpty()) {
		// 保存图片
		bool success = pixmap.save(filePath);
		if (success) {
			QMessageBox::information(this, "成功", QString("截图已保存到：\n%1").arg(filePath));
		}
		else {
			QMessageBox::warning(this, "错误", "截图保存失败");
		}
	}
}

std::string Insulator_Zero_Value_Detection_Robot::GenerateUniqueTicketId()
{
	auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(10000, 99999);  // 5位随机数

	return "TICKET_" + std::to_string(now) + "_" + std::to_string(dis(gen));
}

std::string Insulator_Zero_Value_Detection_Robot::GenerateUniqueReportId()
{
	auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(10000, 99999);  // 5位随机数

	return "REPORT" + std::to_string(now) + "_" + std::to_string(dis(gen));
}

void Insulator_Zero_Value_Detection_Robot::SetTicketRow(int row, const CNewTicketConfig& ticket)
{
	ui.tableWidget_2->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
	ui.tableWidget_2->item(row, 0)->setData(Qt::UserRole, QVariant::fromValue(ticket));
	ui.tableWidget_2->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(ticket.m_strLineName)));
	ui.tableWidget_2->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(ticket.m_strPoleNumber)));
	ui.tableWidget_2->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(CNewTicketConfig::m_vecBunchType(ticket.m_eBunchType))));
	ui.tableWidget_2->setItem(row, 4, new QTableWidgetItem(QString::number(ticket.m_wInsulatorSliceNum)));
	ui.tableWidget_2->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(CNewTicketConfig::m_vecLoopType(ticket.m_eLoopType))));
	ui.tableWidget_2->setItem(row, 6, new QTableWidgetItem(QString::fromStdString(CNewTicketConfig::m_vecCurrentType(ticket.m_eCurrentType))));
	ui.tableWidget_2->setItem(row, 7, new QTableWidgetItem(QString::fromStdString(ticket.m_strStartTime)));
	ui.tableWidget_2->setItem(row, 8, new QTableWidgetItem(QString::fromStdString(ticket.m_strEndTime)));
	ui.tableWidget_2->setItem(row, 9, new QTableWidgetItem(QString::fromStdString(ticket.m_strDetectionPerson)));
}

void Insulator_Zero_Value_Detection_Robot::FilterTicketTable()
{
	// lineEdit：线路名称（列1）；lineEdit_3：检测人员（列9），模糊匹配，留空则不参与过滤
	QString strLineName = ui.lineEdit->text().trimmed();
	QString strPerson = ui.lineEdit_3->text().trimmed();
	for (int i = 0; i < ui.tableWidget_2->rowCount(); i++)
	{
		bool bMatch = true;
		if (!strLineName.isEmpty())
		{
			QTableWidgetItem* item = ui.tableWidget_2->item(i, 1);
			if (!item || !item->text().contains(strLineName, Qt::CaseInsensitive))
				bMatch = false;
		}
		if (bMatch && !strPerson.isEmpty())
		{
			QTableWidgetItem* item = ui.tableWidget_2->item(i, 9);
			if (!item || !item->text().contains(strPerson, Qt::CaseInsensitive))
				bMatch = false;
		}
		ui.tableWidget_2->setRowHidden(i, !bMatch);
	}
}

void Insulator_Zero_Value_Detection_Robot::FilterReportTable()
{
	// lineEdit_4：线路名称（匹配线路信息列4）；lineEdit_6：检测人员（列3），模糊匹配，留空则不参与过滤
	QString strLineName = ui.lineEdit_4->text().trimmed();
	QString strPerson = ui.lineEdit_6->text().trimmed();
	for (int i = 0; i < ui.tableWidget_3->rowCount(); i++)
	{
		bool bMatch = true;
		if (!strLineName.isEmpty())
		{
			QTableWidgetItem* item = ui.tableWidget_3->item(i, 4);
			if (!item || !item->text().contains(strLineName, Qt::CaseInsensitive))
				bMatch = false;
		}
		if (bMatch && !strPerson.isEmpty())
		{
			QTableWidgetItem* item = ui.tableWidget_3->item(i, 3);
			if (!item || !item->text().contains(strPerson, Qt::CaseInsensitive))
				bMatch = false;
		}
		ui.tableWidget_3->setRowHidden(i, !bMatch);
	}
}

void Insulator_Zero_Value_Detection_Robot::On_ResetTicket_Click()
{
	// 清空查询条件并恢复全部显示（textChanged会自动触发过滤）
	ui.lineEdit->clear();
	ui.lineEdit_2->clear();
	ui.lineEdit_3->clear();
	FilterTicketTable();
}

void Insulator_Zero_Value_Detection_Robot::On_ResetReport_Click()
{
	// 清空查询条件并恢复全部显示（textChanged会自动触发过滤）
	ui.lineEdit_4->clear();
	ui.lineEdit_5->clear();
	ui.lineEdit_6->clear();
	FilterReportTable();
}

void Insulator_Zero_Value_Detection_Robot::SetVisibles(bool bVisible, int nSliceNum)
{
	ui.labelInside->setVisible(bVisible);
	ui.labelOutside->setVisible(bVisible);

	for (int i = 1; i <= 60; i++)
	{
		QString strName = QString("labelInside%1").arg(i);
		QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (label)
		{
			label->setVisible(false);
		}
		strName = QString("labelOutside%1").arg(i);
		label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (label)
		{
			label->setVisible(false);
		}
	}

	// 根据字符串找到对应的lable控件
	for (int i = 1; i <= nSliceNum; i++)
	{
		QString strName = QString("labelInside%1").arg(i);
		QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (label)
		{
			label->setVisible(true);
		}
		if (!bVisible) continue;
		strName = QString("labelOutside%1").arg(i);
		label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (label)
		{
			label->setVisible(true);
		}
	}
}
