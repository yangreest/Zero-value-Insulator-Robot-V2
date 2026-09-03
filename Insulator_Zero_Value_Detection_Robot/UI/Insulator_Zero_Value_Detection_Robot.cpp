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
#include <QDoubleValidator>
#include <QScrollBar>
#include <cmath>
#include <QDateTime>
#include <QFileDialog.h>
#include <QVBoxLayout>
#include <QDialog>
#include <QKeyEvent>
#include <QDir>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <random>
#include <Report/WriteReports.h>

static const QString strRTSP_URL = "rtsp://admin:123456@192.168.1.123/stream0";
static const QString strRTSP_URL_2 = "rtsp://admin:123456@192.168.1.123/stream1";

// m_mapTicketMearData(JSON)读写辅助:结构为 { 侧别: { 相别/方向: [测量值...] } }
// 读取指定侧别/相别的测量值数组
static QJsonArray GetMearDataArray(const QJsonObject& root, const QString& strSide, const QString& strDira)
{
	return root.value(strSide).toObject().value(strDira).toArray();
}

// 回写指定侧别/相别的测量值数组
static void SetMearDataArray(QJsonObject& root, const QString& strSide, const QString& strDira, const QJsonArray& arrData)
{
	QJsonObject objSide = root.value(strSide).toObject();
	objSide[strDira] = arrData;
	root[strSide] = objSide;
}

// 向指定侧别/相别追加一个测量值
static void AppendMearData(QJsonObject& root, const QString& strSide, const QString& strDira, float value)
{
	QJsonArray arrData = GetMearDataArray(root, strSide, strDira);
	arrData.append(static_cast<double>(value));
	SetMearDataArray(root, strSide, strDira, arrData);
}

// ===== X值单点定标流程参数（对应《单点定标协议与流程 V1.0》）=====
// 0x0F在触发后约3.2秒主动上报，文档要求等待超时≥ 5秒
static const int CALIB_MEASURE_TIMEOUT_MS = 6000;
// 0x0E需擦写Flash Sector4耗时1~2秒，文档要求应答超时≥ 3秒
static const int CALIB_FLASH_TIMEOUT_MS = 5000;
// 0x05/0x06/0x0C/0x0A为普通命令，应答较快
static const int CALIB_CMD_TIMEOUT_MS = 3000;
// 测原始值连测次数（文档建议2~3次取平均）
static const int CALIB_MEASURE_COUNT = 3;
// 两次测量之间的间隔，给模块读数稳定的时间
static const int CALIB_MEASURE_INTERVAL_MS = 800;

// 把待发送帧转成HEX文本，便于与协议文档的示例帧逐字节比对
static QString CalibFrameToHex(const std::vector<uint8_t>& vecData)
{
	QString strHex;
	for (size_t i = 0; i < vecData.size(); ++i)
	{
		if (i > 0)
			strHex += QLatin1Char(' ');
		strHex += QString("%1").arg(vecData[i], 2, 16, QLatin1Char('0')).toUpper();
	}
	return strHex;
}

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
	// 停止录像标志,避免取流线程继续往缓存追加帧；编码线程已取走帧不受影响。
	m_bRecording = false;
}

void Insulator_Zero_Value_Detection_Robot::InitUI()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("InitUI:界面初始化开始");
	showMaximized();
	// 隐藏标题栏
	setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

	ui.label_22->setVisible(false);
	// label_9显示摄像头画面,设置缩放以适应容器大小,避免图片过大撑出界面
	ui.label_9->setScaledContents(true);
	ui.labelTicketLineName->setText("");
	ui.labelPoleNumber->setText("");

	// splitter手柄加宽并美化,方便触摸屏操作
	ui.splitter->setStyleSheet(
		"QSplitter::handle { background-color: #303846; }"
		"QSplitter::handle:horizontal { width: 2px; }"
		"QSplitter::handle:vertical { height: 6px; margin: 0 20%; }"
	);
	ui.splitter->setHandleWidth(12); // 加宽手柄触摸区域（含透明扩展区）

	// splitter首次布局前设置,Qt按数值比例分配空间（视频区2:标签页1）,之后手动拖拽不受影响
	// 注意:在 showMaximized() 之后调用 setWindowFlags 修改窗口标志会导致窗口销毁并重建,
	// 这会重置 splitter 的布局状态。因此必须延迟到事件循环,等待窗口重建并最大化布局完成后再设置。
	QTimer::singleShot(0, this, [this]() {
		int totalHeight = ui.splitter->height();
		if (totalHeight > 0) {
			// 按 视频区2 : 标签页1 的实际像素比例分配
			ui.splitter->setSizes({ totalHeight * 2 / 3, totalHeight / 3 });
		}
		});

	ui.groupBox_7->setVisible(false);

	// 获取结果,执行下一个
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
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #1A202B;\n}");
		}
		strName = QString("labelOutside%1").arg(i);
		label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (label)
		{
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #1A202B;\n}");
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

	// 初始无过滤条件,全部显示
	FilterTicketTable();
	FilterReportTable();

	m_pModelDataWidget = new ModelDataWidget(ui.widget);
	m_activeWidget = m_pModelDataWidget;
	m_activeWidget->load();
	// 构造函数中窗口尚未显示/最大化,此时ui.widget->size()不是最终尺寸；
	// 改用布局管理,让曲线控件自动跟随ui.widget尺寸,避免一次性resize导致启动时不显示
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
    ui.comboBox_4->setCurrentIndex(m_pConfig->m_memControlBoardConfig.m_cServoSpeed);

	// IP地址校验 0‑255.0‑255.0‑255.0‑255
	QRegularExpression ipRx("((2[0-4]\\d|25[0-5]|[01]?\\d\\d?)\\.){3}(2[0-4]\\d|25[0-5]|[01]?\\d\\d?)");
	ui.lineEdit_9->setValidator(new QRegularExpressionValidator(ipRx, ui.lineEdit_9));

	ui.lineEdit_10->setValidator(new QRegularExpressionValidator(ipRx, ui.lineEdit_10));

	ui.lineEdit_9->setText(QString::fromStdString(m_pConfig->m_memControlBoardConfig.m_strIp));			// 设备IP

	ui.lineEdit_10->setText(QString::fromStdString(m_pConfig->m_memCCameraConfig.m_strLeftIp));         // 左摄像头IP	

	ui.lineEdit_15->setText(QString::number(m_pConfig->m_memControlBoardConfig.m_wInsuThreshold));

	// X值单点定标：按步骤进度初始化按钮可用态（自上而下顺序执行）
	ui.lineEdit_CalibStd->setValidator(new QDoubleValidator(0.001, 100000.0, 3, ui.lineEdit_CalibStd));
	ui.textEdit_CalibLog->clear();
	CalibSetUiEnabled(true);
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
	m_pDeviceLog->Write("InitParam:参数初始化开始,设备日志已启动");
	m_pConfig = new CConfigManager();
	if(!m_pConfig->Read(WHSD_Tools::GetAbsolutePath("Config.xml")))
        m_pDeviceLog->Write("InitParam:参数初始化失败,请检查Config.xml文件");
	m_pXInputHelper = new CXInputHelper(0);
	m_pXInputHelper->RegisterControllerStateCallBack(std::bind(
		&Insulator_Zero_Value_Detection_Robot::CallBack_ControllerState, this, std::placeholders::_1,
		std::placeholders::_2));
	m_pXInputHelper->BeginWork();

	m_mapTicketMearData = QJsonObject();

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
	pWHSDControlBoardProtocol->RegisterCalibCallBack(std::bind(&Insulator_Zero_Value_Detection_Robot::CallBack_CalibAnswer, this, std::placeholders::_1));
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
	if (m_pDeviceLog)
		m_pDeviceLog->Write("BindAction:绑定定时器与按钮信号槽");
	m_pTimer = new QTimer(this);
	m_pTimer->setInterval(100);
	connect(m_pTimer, &QTimer::timeout, this, &Insulator_Zero_Value_Detection_Robot::On_timer_timeout);
	m_pTimer->start();

	m_pTimerInput = new QTimer(this);
	m_pTimerInput->setInterval(10);
	connect(m_pTimerInput, &QTimer::timeout, this, &Insulator_Zero_Value_Detection_Robot::On_timerInput_timeout);
	m_pTimerInput->start();

	connect(ui.pushButton, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_TurnOnAll_Click);
	connect(ui.pushButton_2, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Screenshot_Click);
	connect(ui.pushButton_12, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_RefeshTable_Click);
	connect(ui.pBClose, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Close_Click);
	connect(ui.pBInspection, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Inspection_Click);
	connect(ui.pBticket, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Ticket_Click);
	connect(ui.pBSetting, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_pBSetting_Click);
	connect(ui.pBreport, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Report_Click);
	
	connect(ui.pushButton_4, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_Record_Click);
	connect(ui.pBNewTicket, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_NewTicket_Click);
	connect(ui.pBNewReport, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_NewReport_Click);

	connect(ui.pBDeleteTicket, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_DeleteTicket_Click);
	connect(ui.pBChangeTicket, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_ChangeTicket_Click);
	connect(ui.pBLoadTicket, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_LoadTicket_Click);
	connect(ui.pushButton_20, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_DeleteReport_Click);

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
	connect(ui.pushButton_30, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_WriteReport_Click);

	connect(ui.comboBox_2, &QComboBox::currentIndexChanged, this, &Insulator_Zero_Value_Detection_Robot::On_combobox_currentIndexChanged);
	connect(ui.comboBox, &QComboBox::currentIndexChanged, this, &Insulator_Zero_Value_Detection_Robot::On_combobox_currentIndexChanged);

	// 工单列表过滤:线路名称 + 检测人员（实时过滤,也可点查询按钮）
	connect(ui.lineEdit, &QLineEdit::textChanged, this, &Insulator_Zero_Value_Detection_Robot::FilterTicketTable);
	connect(ui.lineEdit_3, &QLineEdit::textChanged, this, &Insulator_Zero_Value_Detection_Robot::FilterTicketTable);
	connect(ui.pushButton_10, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::FilterTicketTable);
	connect(ui.pushButton_11, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_ResetTicket_Click);

	// 报告列表过滤:线路信息 + 检测人员（实时过滤）
	connect(ui.lineEdit_4, &QLineEdit::textChanged, this, &Insulator_Zero_Value_Detection_Robot::FilterReportTable);
	connect(ui.lineEdit_6, &QLineEdit::textChanged, this, &Insulator_Zero_Value_Detection_Robot::FilterReportTable);
	connect(ui.pushButton_18, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_ResetReport_Click);

	// 保存数据
	connect(ui.pushButton_3, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_SaveProbeAngle_Click); // 探针角度
	connect(ui.pushButton_6, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_SaveMotorSpeed_Click); // 电机速度
    connect(ui.pushButton_19, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_SaveServoSpeed_Click);
	connect(ui.pushButton_8, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_SaveRobotIp_Click); // 机器人ip	
	connect(ui.pushButton_9, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_SaveCameraIp_Click); // 摄像头ip
	connect(ui.pushButton_29, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_SaveInsuThreshold_Click);// 保存绝缘阈值

	// ===== X值单点定标（groupBox_3）：步骤自上而下顺序执行 =====
	connect(ui.pushButton_CalibReset, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_CalibReset_Click);
	connect(ui.pushButton_CalibMeasure, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_CalibMeasure_Click);
	connect(ui.pushButton_CalibDo, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_CalibDo_Click);
	connect(ui.pushButton_CalibVerify, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_CalibVerify_Click);
	connect(ui.pushButton_CalibCheck, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_CalibCheck_Click);
	connect(ui.pushButton_CalibReadCoef, &QPushButton::clicked, this, &Insulator_Zero_Value_Detection_Robot::On_CalibReadCoef_Click);

	// 等待回测结果：下位机应答/回报在协议线程回调，用队列连接切到UI线程再推进流程
	connect(this, &Insulator_Zero_Value_Detection_Robot::CalibMeasureValueSignal,
		this, &Insulator_Zero_Value_Detection_Robot::On_CalibMeasureValue, Qt::QueuedConnection);
	connect(this, &Insulator_Zero_Value_Detection_Robot::CalibAnswerSignal,
		this, &Insulator_Zero_Value_Detection_Robot::On_CalibAnswer, Qt::QueuedConnection);


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

	if (m_nLastDir == 0)
	{
		switch (tp.dpad)
		{
		case 1:
		{
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
				m_pConfig->m_memControlBoardConfig.m_cUpAngle, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
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
				m_pConfig->m_memControlBoardConfig.m_cDownAngle, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
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
				m_pConfig->m_memControlBoardConfig.m_cUpAngle2, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("外上");
			break;
		}
		case 6: // 探针检测
		{
			On_Test_Click();
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
	if (m_pDeviceLog)
		m_pDeviceLog->Write(std::string("控制板连接状态变化:") + (connected ? "已连接" : "断开"));
}

//void Insulator_Zero_Value_Detection_Robot::RefreshControllerState(const ControllerState* p)
//{
//	// 刷新控制器状态
//	std::lock_guard<std::mutex> g(m_mutexXInput);
//	memcpy(&m_memControllerState, p, sizeof(ControllerState));
//}

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

	// 定标流程中的0x0F结果只走定标信号槽，不得写入工单测量数据与曲线（回调在协议线程）
	if (m_bCalibMeasuring)
	{
		emit CalibMeasureValueSignal(static_cast<double>(value));
		return;
	}

	if (m_pDeviceLog)
		m_pDeviceLog->WriteFormat("收到测量结果:%.3f MΩ", value);

	// 测量流程:记录本次结果所属步骤,切到UI线程推进后续流程（回调在协议线程）
	int nMeasureStep = m_nMeasureStep;
	QMetaObject::invokeMethod(this, [this, nMeasureStep]() {
		OnMeasureResult(nMeasureStep);
		}, Qt::QueuedConnection);

	overlayLabel->setWindowFlags(Qt::Widget);
	overlayLabel->setStyleSheet("background-color:rgba(0,0,0,20);color:white;font-size:20px;");
	overlayLabel->setText(QString("当前检测结果:%1MΩ").arg(value));
	overlayLabel->setMinimumWidth(250);

	// 铺满整个窗口,也可以自定义大小位置
	// 显示在label_9 右下角,获取绝对的坐标
	overlayLabel->move(ui.label_9->mapToGlobal(QPoint(ui.label_9->width() - overlayLabel->width(), ui.label_9->height() - overlayLabel->height())));
	//overlayLabel->setGeometry(ui.label_9->geometry().x(), ui.label_9->geometry().y(), 50, 200);

	overlayLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

	// 默认显示
	overlayLabel->show();

	QString strSide = ui.comboBox_2->currentText();
	QString strDira = ui.comboBox->currentText();
	AppendMearData(m_mapTicketMearData, strSide, strDira, value);

	// 将测量数据同步到当前工单配置并直接保存到XML文件
	m_CurrentTicketConfig.m_mapTicketMearData = m_mapTicketMearData;
	for (auto& ticket : m_pConfig->m_vecNewTicketConfig)
	{
		if (ticket.m_strTicketId == m_CurrentTicketConfig.m_strTicketId)
		{
			ticket.m_mapTicketMearData = m_mapTicketMearData;
			break;
		}
	}
	m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));

	QJsonArray vecData = GetMearDataArray(m_mapTicketMearData, strSide, strDira);
	bool visible = (m_CurrentTicketConfig.m_eBunchType == CNewTicketConfig::BunchType::eDouble);

	// 每获得一个测量值,填充到表格对应列的空单元格并绘制曲线（回调在协议线程,切到UI线程执行）
	// 双联时每相拆为两列:奇数次测量为内侧,偶数次为外侧
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
			if(m_pConfig->m_memControlBoardConfig.m_wInsuThreshold<=value)
			{
				label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #10b981;\n}");
			}
			else
			{
				label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #f56c6c;\n}");
			}
			
		}
		else
		{
			// 偶数是外侧
			QString strName = QString("labelOutside%1").arg(vecData.size() / 2);
			QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
			if (!label)return;
			if(m_pConfig->m_memControlBoardConfig.m_wInsuThreshold<=value)
			{
				label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #10b981;\n}");
			}
			else
			{
				label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #f56c6c;\n}");
			}
		}
	}
	else //单联
	{
		// 奇数是内侧
		QString strName = QString("labelInside%1").arg(vecData.size());
		QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (!label)return;
		if(m_pConfig->m_memControlBoardConfig.m_wInsuThreshold<=value)
		{
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #10b981;\n}");
		}
		else
		{
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #f56c6c;\n}");
		}
	}
}

// ===================== X值单点定标流程 =====================
// 流程对应文档§5：①恢复默认 → ②测原始值(连测3次取平均) → ③下发定标 → ④验证
// 每步发出命令后进入等待态，下位机回报在协议线程回调，统一转成信号队列到UI线程推进

void Insulator_Zero_Value_Detection_Robot::CallBack_CalibAnswer(const CCalibAnswer& answer)
{
	// 回调运行在协议线程，不得直接操作界面，发信号由队列连接切到UI线程
	emit CalibAnswerSignal(answer.m_cSubCmd, answer.m_cResult, answer.m_cReason,
		answer.m_nValue1, answer.m_nValue2);
}

void Insulator_Zero_Value_Detection_Robot::CalibAppendLog(const QString& strText)
{
	ui.textEdit_CalibLog->append(QString("[%1] %2")
		.arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(strText));
	// 自动滚到底部，保证最新一步不被遮住
	ui.textEdit_CalibLog->verticalScrollBar()->setValue(ui.textEdit_CalibLog->verticalScrollBar()->maximum());
	// 错误只写日志不弹模态框，避免与测量等待弹窗叠加导致界面卡死
	if (m_pDeviceLog)
		m_pDeviceLog->Write(QString("定标流程:%1").arg(strText).toStdString());
}

void Insulator_Zero_Value_Detection_Robot::CalibSetStepState(QLabel* pLabel, const QString& strText, int nType)
{
	if (pLabel == nullptr)
		return;
	// 0=未执行/灰 1=进行中/蓝 2=成功/绿 3=失败/红
	static const char* arrColor[] = { "#6b7280", "#60a5fa", "#10b981", "#f56c6c" };
	const int nIndex = (nType >= 0 && nType <= 3) ? nType : 0;
	pLabel->setStyleSheet(QString("color:%1;").arg(arrColor[nIndex]));
	pLabel->setText(strText);
}

void Insulator_Zero_Value_Detection_Robot::CalibSetUiEnabled(bool bEnable)
{
	// 步骤自上而下顺序执行：上一步未完成时不放开下一步的按钮
	ui.lineEdit_CalibStd->setEnabled(bEnable);
	ui.pushButton_CalibReset->setEnabled(bEnable);
	ui.pushButton_CalibMeasure->setEnabled(bEnable && m_bCalibResetDone);
	ui.pushButton_CalibDo->setEnabled(bEnable && m_bCalibRawDone);
	ui.pushButton_CalibVerify->setEnabled(bEnable && m_bCalibDoDone);
	ui.pushButton_CalibCheck->setEnabled(bEnable && m_bCalibDoDone);
	ui.pushButton_CalibReadCoef->setEnabled(bEnable);
}

bool Insulator_Zero_Value_Detection_Robot::CalibStartWait(ECalibStep eStep, int nTimeoutMs)
{
	// 工单测量流程优先：定标期间0x0F结果会被改道，两者同时进行会互相抢数据
	if (m_nMeasureStep != 0)
	{
		CalibAppendLog(QStringLiteral("工单测量流程正在进行中，请等其结束后再执行定标"));
		return false;
	}
	m_eCalibStep = eStep;
	CalibSetUiEnabled(false);
	if (m_pCalibTimeoutTimer == nullptr)
	{
		m_pCalibTimeoutTimer = new QTimer(this);
		m_pCalibTimeoutTimer->setSingleShot(true);
		connect(m_pCalibTimeoutTimer, &QTimer::timeout, this, &Insulator_Zero_Value_Detection_Robot::On_CalibTimeout);
	}
	m_pCalibTimeoutTimer->start(nTimeoutMs);
	return true;
}

void Insulator_Zero_Value_Detection_Robot::CalibStopWait()
{
	if (m_pCalibTimeoutTimer != nullptr)
		m_pCalibTimeoutTimer->stop();
	m_eCalibStep = eCalibIdle;
	m_bCalibMeasuring = false;
	CalibSetUiEnabled(true);
}

void Insulator_Zero_Value_Detection_Robot::CalibTriggerMeasure()
{
	// 0x11触发一次测量，应答仅确认"检测中"，真正的值约3.2秒后由0x0F主动上报
	CalibSendFrame(CWHSDControlBoardProtocol::SensorCmd(0, 1, 0));
}

bool Insulator_Zero_Value_Detection_Robot::CalibSendFrame(const std::vector<uint8_t>& vecData)
{
	if (m_pComDevice == nullptr || !m_bControlBroadConnected)
	{
		CalibAppendLog(QStringLiteral("控制板未连接，命令未发送"));
		CalibStopWait();
		return false;
	}
	CalibAppendLog(QStringLiteral("下行: %1").arg(CalibFrameToHex(vecData)));
	m_pComDevice->Write(const_cast<uint8_t*>(vecData.data()), vecData.size());
	return true;
}

bool Insulator_Zero_Value_Detection_Robot::CalibGetStdMilli(qint32& nStdMilli)
{
	bool bOk = false;
	const double dStd = ui.lineEdit_CalibStd->text().trimmed().toDouble(&bOk);
	if (!bOk || dStd <= 0.0)
		return false;
	// 毫值 = 物理值 × 1000 的整数（500 MΩ → 500000）
	nStdMilli = static_cast<qint32>(qRound(dStd * 1000.0));
	return nStdMilli > 0;
}

QString Insulator_Zero_Value_Detection_Robot::CalibReasonText(quint8 cReason)
{
	switch (cReason)
	{
	case 0x01:
		return QStringLiteral("距上次0x0F回报超5秒，原始值已失效，请重新触发检测");
	case 0x04:
		return QStringLiteral("Flash写失败，可重发；反复失败需检查Flash");
	case 0x05:
		return QStringLiteral("参数长度/格式错误（参数必须正好8字节）");
	case 0x09:
		return QStringLiteral("参数非法：标准值或原始值≤0，或原始值≥标准值（负补偿需求），模块读数偏高，请检查硬件，勿强行定标");
	default:
		return QStringLiteral("未知原因码 0x%1").arg(cReason, 2, 16, QLatin1Char('0')).toUpper();
	}
}

void Insulator_Zero_Value_Detection_Robot::On_CalibReset_Click()
{
	if (m_eCalibStep != eCalibIdle)
		return;

	qint32 nStdMilli = 0;
	if (!CalibGetStdMilli(nStdMilli))
	{
		CalibAppendLog(QStringLiteral("标准电阻值非法，请先填入大于0的数值（单位MΩ）"));
		CalibSetStepState(ui.label_CalibStep1State, QStringLiteral("标准值非法"), 3);
		return;
	}

	CalibAppendLog(QStringLiteral("① 恢复默认(0x17/0x05)：清空两点系数/折点表/公式系数，测量通道直通"));
	// 先占等待态：失败（如工单测量流程在跑）时直接返回，不能把已取得的数据和步骤态抹掉
	if (!CalibStartWait(eCalibWaitReset, CALIB_CMD_TIMEOUT_MS))
		return;

	// 清空旧校准后系数失效，后续步骤必须重做
	m_bCalibResetDone = false;
	m_bCalibRawDone = false;
	m_bCalibDoDone = false;
	m_nCalibMeasureIndex = 0;
	m_dCalibRawAvg = 0.0;
	m_arCalibRaw[0] = m_arCalibRaw[1] = m_arCalibRaw[2] = 0.0;
	ui.label_CalibRaw1->setText(QStringLiteral("--"));
	ui.label_CalibRaw2->setText(QStringLiteral("--"));
	ui.label_CalibRaw3->setText(QStringLiteral("--"));
	ui.label_CalibRawAvg->setText(QStringLiteral("--"));
	ui.label_CalibCoef->setText(QStringLiteral("--"));
	ui.label_CalibResult->setText(QStringLiteral("--"));
	CalibSetStepState(ui.label_CalibStep2State, QStringLiteral("未执行"), 0);
	CalibSetStepState(ui.label_CalibStep3State, QStringLiteral("未执行"), 0);
	CalibSetStepState(ui.label_CalibStep4State, QStringLiteral("未执行"), 0);
	CalibSetStepState(ui.label_CalibStep1State, QStringLiteral("等待应答..."), 1);
	CalibSendFrame(CWHSDControlBoardProtocol::CalibReset());
}

void Insulator_Zero_Value_Detection_Robot::On_CalibMeasure_Click()
{
	if (m_eCalibStep != eCalibIdle || !m_bCalibResetDone)
		return;

	CalibAppendLog(QStringLiteral("② 测原始值：已挂标准电阻并读数稳定后，连测%1次取平均")
		.arg(CALIB_MEASURE_COUNT));

	// 先占等待态再清数据：CalibStartWait可能因工单测量流程在跑而拒绝，
	// 拒绝时不得抹掉上一轮已取得的三次原始值
	if (!CalibStartWait(eCalibWaitMeasure, CALIB_MEASURE_TIMEOUT_MS))
		return;

	m_nCalibMeasureIndex = 0;
	m_dCalibRawAvg = 0.0;
	m_arCalibRaw[0] = m_arCalibRaw[1] = m_arCalibRaw[2] = 0.0;
	ui.label_CalibRaw1->setText(QStringLiteral("--"));
	ui.label_CalibRaw2->setText(QStringLiteral("--"));
	ui.label_CalibRaw3->setText(QStringLiteral("--"));
	ui.label_CalibRawAvg->setText(QStringLiteral("--"));
	m_bCalibRawDone = false;
	CalibSetStepState(ui.label_CalibStep2State, QStringLiteral("第1/%1次测量...").arg(CALIB_MEASURE_COUNT), 1);

	// 置位后0x0F结果改走定标信号槽，不再写入工单测量数据
	m_bCalibMeasuring = true;
	CalibTriggerMeasure();
}

void Insulator_Zero_Value_Detection_Robot::On_CalibMeasureValue(double dValue)
{
	if (m_eCalibStep == eCalibWaitMeasure)
	{
		if (m_nCalibMeasureIndex >= CALIB_MEASURE_COUNT)
			return;

		// 记录本次原始值（此时已执行0x05恢复默认，0x0F回报的就是未补偿的原始值）
		m_arCalibRaw[m_nCalibMeasureIndex] = dValue;
		CalibAppendLog(QStringLiteral("第%1次原始值: %2 MΩ")
			.arg(m_nCalibMeasureIndex + 1).arg(dValue, 0, 'f', 3));
		switch (m_nCalibMeasureIndex)
		{
		case 0: ui.label_CalibRaw1->setText(QString::number(dValue, 'f', 3)); break;
		case 1: ui.label_CalibRaw2->setText(QString::number(dValue, 'f', 3)); break;
		default: ui.label_CalibRaw3->setText(QString::number(dValue, 'f', 3)); break;
		}
		++m_nCalibMeasureIndex;

		// 文档建议：0x0F回报后5秒内可用0x06复核原始值，此处只做比对日志，不阻塞流程
		CalibSendFrame(CWHSDControlBoardProtocol::CalibQueryRaw());

		if (m_nCalibMeasureIndex < CALIB_MEASURE_COUNT)
		{
			CalibSetStepState(ui.label_CalibStep2State,
				QStringLiteral("第%1/%2次测量...").arg(m_nCalibMeasureIndex + 1).arg(CALIB_MEASURE_COUNT), 1);
			// 重启超时定时器，继续等下一次回报
			m_pCalibTimeoutTimer->start(CALIB_MEASURE_TIMEOUT_MS);
			QTimer::singleShot(CALIB_MEASURE_INTERVAL_MS, this, [this]() {
				if (m_eCalibStep == eCalibWaitMeasure)
					CalibTriggerMeasure();
				});
			return;
		}

		// 三次完成，取平均值作为定标用的实测原始值
		double dSum = 0.0;
		for (int i = 0; i < CALIB_MEASURE_COUNT; ++i)
			dSum += m_arCalibRaw[i];
		m_dCalibRawAvg = dSum / CALIB_MEASURE_COUNT;
		ui.label_CalibRawAvg->setText(QString::number(m_dCalibRawAvg, 'f', 3));
		m_bCalibRawDone = true;
		CalibSetStepState(ui.label_CalibStep2State,
			QStringLiteral("完成，平均 %1 MΩ").arg(m_dCalibRawAvg, 0, 'f', 3), 2);
		CalibAppendLog(QStringLiteral("② 完成：%1次原始值平均 = %2 MΩ（毫值 %3）")
			.arg(CALIB_MEASURE_COUNT).arg(m_dCalibRawAvg, 0, 'f', 3)
			.arg(static_cast<qint64>(qRound(m_dCalibRawAvg * 1000.0))));
		CalibStopWait();
		return;
	}

	if (m_eCalibStep == eCalibWaitVerify)
	{
		// 定标生效后0x0F回报的是修正值，直接对比标准值算误差
		qint32 nStdMilli = 0;
		const bool bStdOk = CalibGetStdMilli(nStdMilli);
		const double dStd = bStdOk ? (static_cast<double>(nStdMilli) / 1000.0) : 0.0;
		const double dErrPercent = (dStd > 0.0) ? ((dValue - dStd) / dStd * 100.0) : 0.0;
		ui.label_CalibResult->setText(QStringLiteral("%1 MΩ（标准 %2 MΩ，误差 %3%）")
			.arg(dValue, 0, 'f', 3).arg(dStd, 0, 'f', 3).arg(dErrPercent, 0, 'f', 2));
		CalibSetStepState(ui.label_CalibStep4State, QStringLiteral("验证完成"), 2);
		CalibAppendLog(QStringLiteral("④ 验证：0x0F回报修正值 %1 MΩ，标准 %2 MΩ，误差 %3%")
			.arg(dValue, 0, 'f', 3).arg(dStd, 0, 'f', 3).arg(dErrPercent, 0, 'f', 2));
		CalibStopWait();
		return;
	}

	// 非定标等待态收到的值属于工单测量流程，此处不处理
	CalibAppendLog(QStringLiteral("收到非定标流程的测量值 %1 MΩ，已忽略").arg(dValue, 0, 'f', 3));
}

void Insulator_Zero_Value_Detection_Robot::On_CalibDo_Click()
{
	if (m_eCalibStep != eCalibIdle || !m_bCalibRawDone)
		return;

	qint32 nStdMilli = 0;
	if (!CalibGetStdMilli(nStdMilli))
	{
		CalibAppendLog(QStringLiteral("标准电阻值非法，无法下发定标"));
		CalibSetStepState(ui.label_CalibStep3State, QStringLiteral("标准值非法"), 3);
		return;
	}

	// 实测原始值取三次平均，转int32毫值大端
	const qint32 nRawMilli = static_cast<qint32>(qRound(m_dCalibRawAvg * 1000.0));
	const double dStd = static_cast<double>(nStdMilli) / 1000.0;

	// 上位机先做一遍合法性检查：原始值≥标准值属负补偿需求，下位机会回原因09
	if (nRawMilli <= 0 || nRawMilli >= nStdMilli)
	{
		CalibAppendLog(QStringLiteral("③ 参数非法：原始值平均 %1 MΩ 必须大于0且小于标准值 %2 MΩ；模块读数偏高，请检查接线与硬件，勿强行定标")
			.arg(m_dCalibRawAvg, 0, 'f', 3).arg(dStd, 0, 'f', 3));
		CalibSetStepState(ui.label_CalibStep3State, QStringLiteral("参数非法"), 3);
		return;
	}

	// 固件内部自行核算：p% = (1 − 原始值/标准值)×100，c = p ÷ √原始值，上位机零计算
	// 此处只算一遍理论值用于与应答回显的a比对
	const double dP = (1.0 - m_dCalibRawAvg / dStd) * 100.0;
	const double dExpectC = dP / std::sqrt(m_dCalibRawAvg);
	CalibAppendLog(QStringLiteral("③ 下发定标(0x17/0x0E)：标准值 %1 毫值 + 原始值 %2 毫值；理论 c ≈ %3")
		.arg(nStdMilli).arg(nRawMilli).arg(dExpectC, 0, 'f', 5));
	CalibSetStepState(ui.label_CalibStep3State, QStringLiteral("写Flash中(1~2秒)..."), 1);
	if (!CalibStartWait(eCalibWaitDo, CALIB_FLASH_TIMEOUT_MS))
		return;
	CalibSendFrame(CWHSDControlBoardProtocol::CalibSinglePoint(nStdMilli, nRawMilli));
}

void Insulator_Zero_Value_Detection_Robot::On_CalibVerify_Click()
{
	if (m_eCalibStep != eCalibIdle || !m_bCalibDoDone)
		return;

	CalibAppendLog(QStringLiteral("④ 验证：定标生效后0x0F回报修正值，对比标准值看误差"));
	CalibSetStepState(ui.label_CalibStep4State, QStringLiteral("等待修正值..."), 1);
	if (!CalibStartWait(eCalibWaitVerify, CALIB_MEASURE_TIMEOUT_MS))
		return;
	m_bCalibMeasuring = true;
	CalibTriggerMeasure();
}

void Insulator_Zero_Value_Detection_Robot::On_CalibCheck_Click()
{
	if (m_eCalibStep != eCalibIdle || !m_bCalibDoDone)
		return;

	if (m_dCalibRawAvg <= 0.0)
	{
		CalibAppendLog(QStringLiteral("离线抽查需先完成步骤②取得原始值平均值"));
		return;
	}

	// 不挂电阻，直接把原始值下发，下位机回显"原始值 + 校准后值"
	const qint32 nRawMilli = static_cast<qint32>(qRound(m_dCalibRawAvg * 1000.0));
	CalibAppendLog(QStringLiteral("④ 离线抽查(0x17/0x0A)：不挂电阻，下发原始值 %1 毫值").arg(nRawMilli));
	CalibSetStepState(ui.label_CalibStep4State, QStringLiteral("等待应答..."), 1);
	if (!CalibStartWait(eCalibWaitAux, CALIB_CMD_TIMEOUT_MS))
		return;
	CalibSendFrame(CWHSDControlBoardProtocol::CalibVerify(nRawMilli));
}

void Insulator_Zero_Value_Detection_Robot::On_CalibReadCoef_Click()
{
	if (m_eCalibStep != eCalibIdle)
		return;

	CalibAppendLog(QStringLiteral("读回系数(0x17/0x0C)：核对Flash中已存系数"));
	if (!CalibStartWait(eCalibWaitAux, CALIB_CMD_TIMEOUT_MS))
		return;
	CalibSendFrame(CWHSDControlBoardProtocol::CalibReadCoef());
}

void Insulator_Zero_Value_Detection_Robot::On_CalibAnswer(quint8 cSubCmd, quint8 cResult, quint8 cReason,
	qint32 nValue1, qint32 nValue2)
{
	const bool bSuccess = (cResult == 0x01);
	const QString strSub = QString("%1").arg(static_cast<int>(cSubCmd), 2, 16, QLatin1Char('0')).toUpper();
	CalibAppendLog(QStringLiteral("上行: 17 %1 结果=%2 原因=%3 参数1=%4 参数2=%5（毫值）")
		.arg(strSub).arg(static_cast<int>(cResult)).arg(static_cast<int>(cReason)).arg(nValue1).arg(nValue2));

	switch (cSubCmd)
	{
	case 0x05:	// 恢复默认
	{
		if (m_eCalibStep != eCalibWaitReset)
			break;
		if (bSuccess)
		{
			m_bCalibResetDone = true;
			// 应答回显k/b（k=1, b=0）即直通状态，此时0x0F回报的就是原始值
			CalibAppendLog(QStringLiteral("① 恢复默认成功，测量通道已直通（k=%1 b=%2）；请挂标准电阻预热、读数稳定后执行步骤②")
				.arg(nValue1 / 1000.0, 0, 'f', 3).arg(nValue2 / 1000.0, 0, 'f', 3));
			CalibSetStepState(ui.label_CalibStep1State, QStringLiteral("已恢复默认"), 2);
		}
		else
		{
			CalibAppendLog(QStringLiteral("① 恢复默认失败：%1").arg(CalibReasonText(cReason)));
			CalibSetStepState(ui.label_CalibStep1State, QStringLiteral("失败"), 3);
		}
		CalibStopWait();
		break;
	}
	case 0x06:	// 查询原始值：仅用于复核比对，不推进流程也不结束等待
	{
		if (bSuccess)
			CalibAppendLog(QStringLiteral("0x06复核原始值: %1 MΩ").arg(nValue1 / 1000.0, 0, 'f', 3));
		else
			CalibAppendLog(QStringLiteral("0x06复核失败：%1").arg(CalibReasonText(cReason)));
		break;
	}
	case 0x0C:	// 读回系数
	{
		if (bSuccess)
		{
			const double dC = nValue1 / 1000.0;
			ui.label_CalibCoef->setText(QString::number(dC, 'f', 3));
			CalibAppendLog(QStringLiteral("0x0C读回系数：a=%1 毫值 → c=%2，b=%3 毫值")
				.arg(nValue1).arg(dC, 0, 'f', 3).arg(nValue2));
		}
		else
		{
			CalibAppendLog(QStringLiteral("0x0C读回系数失败：%1").arg(CalibReasonText(cReason)));
		}
		if (m_eCalibStep == eCalibWaitAux)
			CalibStopWait();
		break;
	}
	case 0x0A:	// 补偿验证
	{
		if (bSuccess)
		{
			const double dRaw = nValue1 / 1000.0;
			const double dFixed = nValue2 / 1000.0;
			ui.label_CalibResult->setText(QStringLiteral("离线抽查：%1 → %2 MΩ")
				.arg(dRaw, 0, 'f', 3).arg(dFixed, 0, 'f', 3));
			CalibSetStepState(ui.label_CalibStep4State, QStringLiteral("抽查完成"), 2);
			CalibAppendLog(QStringLiteral("0x0A补偿验证：原始值 %1 MΩ → 校准后 %2 MΩ")
				.arg(dRaw, 0, 'f', 3).arg(dFixed, 0, 'f', 3));
		}
		else
		{
			CalibSetStepState(ui.label_CalibStep4State, QStringLiteral("抽查失败"), 3);
			CalibAppendLog(QStringLiteral("0x0A补偿验证失败：%1").arg(CalibReasonText(cReason)));
		}
		if (m_eCalibStep == eCalibWaitAux)
			CalibStopWait();
		break;
	}
	case 0x0E:	// 单点定标
	{
		if (m_eCalibStep != eCalibWaitDo)
			break;
		if (bSuccess)
		{
			// 应答回显固件算出的系数：c = a ÷ 1000，b固定为0，已立即生效并写入Flash
			const double dC = nValue1 / 1000.0;
			m_bCalibDoDone = true;
			ui.label_CalibCoef->setText(QString::number(dC, 'f', 3));
			CalibSetStepState(ui.label_CalibStep3State, QStringLiteral("定标成功 c=%1").arg(dC, 0, 'f', 3), 2);
			CalibAppendLog(QStringLiteral("③ 定标成功：a=%1 毫值 → c=%2，b=%3；校准已立即生效并写入Flash Sector4，掉电保存")
				.arg(nValue1).arg(dC, 0, 'f', 3).arg(nValue2));
			CalibAppendLog(QStringLiteral("提示：可执行步骤④验证；模块漂移后重测该电阻再发一次0x0E即可（后发覆盖先发，无须先清除）"));
		}
		else
		{
			CalibSetStepState(ui.label_CalibStep3State, QStringLiteral("定标失败"), 3);
			CalibAppendLog(QStringLiteral("③ 定标失败：%1").arg(CalibReasonText(cReason)));
		}
		CalibStopWait();
		break;
	}
	default:
	{
		CalibAppendLog(QStringLiteral("未知的校准子命令 0x%1").arg(strSub));
		break;
	}
	}
}

void Insulator_Zero_Value_Detection_Robot::On_CalibTimeout()
{
	QString strMsg;
	switch (m_eCalibStep)
	{
	case eCalibWaitReset:
	{
		strMsg = QStringLiteral("① 恢复默认应答超时（%1ms），请检查链路与心跳").arg(CALIB_CMD_TIMEOUT_MS);
		CalibSetStepState(ui.label_CalibStep1State, QStringLiteral("应答超时"), 3);
		break;
	}
	case eCalibWaitMeasure:
	{
		// 未集齐三次不作数，必须重测，避免用残缺平均值污染系数
		strMsg = QStringLiteral("② 测原始值超时：0x0F约3.2秒回报，已完成 %1/%2 次，请重新执行本步骤")
			.arg(m_nCalibMeasureIndex).arg(CALIB_MEASURE_COUNT);
		CalibSetStepState(ui.label_CalibStep2State,
			QStringLiteral("超时(%1/%2)").arg(m_nCalibMeasureIndex).arg(CALIB_MEASURE_COUNT), 3);
		m_bCalibRawDone = false;
		break;
	}
	case eCalibWaitDo:
	{
		strMsg = QStringLiteral("③ 定标应答超时（0x0E擦写Flash需1~2秒），请重试或检查链路");
		CalibSetStepState(ui.label_CalibStep3State, QStringLiteral("应答超时"), 3);
		break;
	}
	case eCalibWaitVerify:
	{
		strMsg = QStringLiteral("④ 验证超时：未收到0x0F修正值回报");
		CalibSetStepState(ui.label_CalibStep4State, QStringLiteral("超时"), 3);
		break;
	}
	case eCalibWaitAux:
	{
		strMsg = QStringLiteral("辅助命令应答超时");
		break;
	}
	default:
	{
		strMsg = QStringLiteral("定标等待超时（未知步骤）");
		break;
	}
	}
	CalibAppendLog(strMsg);
	CalibStopWait();
}

void Insulator_Zero_Value_Detection_Robot::CameraConnect()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("CameraConnect:开始连接摄像头");
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
				if (m_pDeviceLog)
					m_pDeviceLog->Write("CameraConnect:左右摄像头连接成功");
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
	if (rtsp_url.empty())
	{
		rtsp_url = strRTSP_URL.toStdString();
	}
	if (m_pDeviceLog)
		m_pDeviceLog->Write("NewCameraConnect:开始连接RTSP摄像头 " + rtsp_url);

	cv::VideoCapture cap;

	// 设置低延迟参数
	cap.set(cv::CAP_PROP_BUFFERSIZE, 1);  // 最小化缓冲区

	// 尝试打开RTSP流
	bool isOpen = cap.open(rtsp_url, cv::CAP_FFMPEG);

	if (!isOpen)
	{
		qDebug() << "Failed to open RTSP stream: " << rtsp_url;
		if (m_pDeviceLog)
			m_pDeviceLog->Write("NewCameraConnect:打开RTSP流失败 " + rtsp_url);
		return; // 不应该返回-1,因为这不是main函数
	}

	cv::Mat frame;


	while (continueStreaming)
	{
		// 低延迟处理:获取最新帧
		if (cap.grab())
		{
			cap.retrieve(frame);

			if (!frame.empty())
			{
				// 录像:录制期间仅将帧缓存到内存,不直接写文件；
				// 停止后由UI线程弹框选路径,再将缓存帧统一转成视频。
				if (m_bRecording)
				{
					std::lock_guard<std::mutex> g(m_mutexRecordBuf);
					// clone避免grab复用缓冲区导致缓存帧被后续帧覆盖/错乱；
					// MJPG按帧压缩,内存占用有限,单帧仅几十字节级别开销可忽略。
					m_vecRecordFrames.push_back(frame.clone());
				}

				//cv::imshow("RTSP Low Delay", frame);
				// 将frame 转成QImage,通过信号安全地传递到UI线程
				QImage qImg = Mat2QImage(frame);
				QPixmap pix = QPixmap::fromImage(qImg);
				QMetaObject::invokeMethod(ui.label_9, [label = ui.label_9, pix]() {
					// sizeHint改为Ignored,避免大图把label撑大超出布局区域,尺寸完全由布局决定；
					// 重复设置无副作用,仅首帧生效。
					label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
					// 按label当前显示区域自适应缩放,保持纵横比,避免画面超出界面；
					// 同时开启SmoothTransformation,缩小后画面更平滑,避免频繁闪烁拉伸。
					QPixmap scaled = pix.scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
					label->setPixmap(scaled);
					label->setAlignment(Qt::AlignCenter);
					}, Qt::QueuedConnection);

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
				if (m_pDeviceLog)
					m_pDeviceLog->Write("NewCameraConnect:取流帧为空,停止取流");
				break;
			}
		}
		else
		{
			qDebug() << "Failed to grab frame";
			if (m_pDeviceLog)
				m_pDeviceLog->Write("NewCameraConnect:抓取帧失败,停止取流");
			break;
		}
	}
	// 清理资源（退出时仍在录像则丢弃缓存帧,避免占内存）
	{
		std::lock_guard<std::mutex> g(m_mutexRecordBuf);
		m_vecRecordFrames.clear();
	}
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
		// 使用copy()深拷贝,避免局部Mat销毁后QImage持有悬空指针
		return QImage((const unsigned char*)rgb.data,
			rgb.cols,
			rgb.rows,
			rgb.step,
			QImage::Format_RGB888).copy();
	}
	// 如果是灰度图像
	else if (mat.channels() == 1) {
		// 使用copy()深拷贝,避免引用的外部Mat数据被后续帧覆盖
		return QImage((const unsigned char*)mat.data,
			mat.cols,
			mat.rows,
			mat.step,
			QImage::Format_Grayscale8).copy(); // Qt 5.13+ 支持,更早版本可用 Format_Indexed8
	}
	// 如果是RGBA图像
	else if (mat.channels() == 4) {
		cv::Mat rgba;
		cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
		// 使用copy()深拷贝,避免局部Mat销毁后QImage持有悬空指针
		return QImage((const unsigned char*)rgba.data,
			rgba.cols,
			rgba.rows,
			rgba.step,
			QImage::Format_RGBA8888).copy();
	}

	// 对于其他通道数,先转换为RGB
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
		QImage::Format_RGB888).copy();
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
	if (m_pDeviceLog)
		m_pDeviceLog->Write(bState ? "一键开机" : "一键关机");
	if (!bState)
	{
		auto cmds = CWHSDControlBoardProtocol::TurnOffAll();
		m_pComDevice->Write(cmds.data(), cmds.size());
	}
	else
	{
		auto cmds = CWHSDControlBoardProtocol::TurnOnAll();
		m_pComDevice->Write(cmds.data(), cmds.size());
	}
}

void Insulator_Zero_Value_Detection_Robot::On_Screenshot_Click()
{
	//截图
	if (m_pDeviceLog)
		m_pDeviceLog->Write("手动截图");
	// 获取当前窗口的句柄（Windows）/ID（Linux）
	WId windowId = ui.label_9->winId();

	// 获取当前窗口所在的屏幕
	QScreen* screen = QApplication::screenAt(ui.label_9->pos());
	if (!screen) {
		screen = QApplication::primaryScreen();
	}

	// 截取指定窗口
	QPixmap pixmap = screen->grabWindow(windowId);
	if (pixmap.isNull()) {
		if (m_pDeviceLog)
			m_pDeviceLog->Write("测量截图失败:像素数据为空");
		return;
	}

    QString fileName = QFileDialog::getSaveFileName(this, "保存图片", "截图", "PNG 文件 (*.png)");
    if (!fileName.isEmpty()) {
		if (!pixmap.save(fileName)) {
			if (m_pDeviceLog)
				m_pDeviceLog->Write("测量截图失败:保存图片失败");
		}
		else if (m_pDeviceLog)
		{
			m_pDeviceLog->Write("截图保存成功:" + fileName.toStdString());
		}
	}
}

void Insulator_Zero_Value_Detection_Robot::On_RefeshTable_Click()
{
	QMessageBox::StandardButton reply = QMessageBox::question(this, QStringLiteral("确认刷新"), QStringLiteral("是否确认刷新摄像头？"), QMessageBox::Yes | QMessageBox::No);
	if (reply == QMessageBox::Yes) {
		QProcess::startDetached(QApplication::applicationFilePath(), QStringList());
		QApplication::exit();
	}
}

void Insulator_Zero_Value_Detection_Robot::On_Close_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("关闭软件");
	this->close();
}

void Insulator_Zero_Value_Detection_Robot::On_Inspection_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("切换到检测界面");
	ui.stackedWidget_3->setCurrentIndex(0);
	if (overlayLabel) overlayLabel->show();
}

void Insulator_Zero_Value_Detection_Robot::On_Ticket_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("切换到工单界面");
	ui.stackedWidget_3->setCurrentIndex(1);
	if (overlayLabel) overlayLabel->hide();
}

void Insulator_Zero_Value_Detection_Robot::On_pBSetting_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("切换到设置界面");
	ui.stackedWidget_3->setCurrentIndex(3);
	if (overlayLabel) overlayLabel->hide();
}

void Insulator_Zero_Value_Detection_Robot::On_ZeroTest_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("发送零值检测指令");
	auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 1, 0);

	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_Record_Click(bool bState)
{
	// 当开始录屏时,将cv::Mat 保存成视频,待录屏结束时保存视频文件；
	// UI线程只置标志并准备路径,实际写帧由取流线程（NewCameraConnect）完成
	if (bState)
	{
		if (!m_pConfig->m_memCCameraConfig.m_bNewCamera)
		{
			// 旧摄像头为SDK直显窗口句柄,取不到cv::Mat帧,无法录像
			if (m_pDeviceLog)
				m_pDeviceLog->Write("录像失败:当前摄像头模式不支持录像");
			QMessageBox::warning(this, "提示", "当前摄像头模式不支持录像");
			ui.pushButton_4->setChecked(false);
			return;
		}

		std::lock_guard<std::mutex> g(m_mutexRecordBuf);
		// clone避免grab复用缓冲区导致缓存帧被后续帧覆盖/错乱；
		// MJPG按帧压缩,内存占用有限,单帧仅几十字节级别开销可忽略。
		m_vecRecordFrames.clear();

		m_bRecording = true;
		if (m_pDeviceLog)
			m_pDeviceLog->Write("开始录像");
		ui.pushButton_4->setText(QStringLiteral("停止录像"));
	}
	else
	{
		// 停止录像:先清零标志,取流线程不再追加帧,再取走全部缓存帧。
		m_bRecording = false;
		ui.pushButton_4->setText(QStringLiteral("录像"));

		std::vector<cv::Mat> vecFrames;
		{
			std::lock_guard<std::mutex> g(m_mutexRecordBuf);
			vecFrames.swap(m_vecRecordFrames);
		}
		if (m_pDeviceLog)
			m_pDeviceLog->WriteFormat("停止录像,共捕获 %d 帧", (int)vecFrames.size());
		if (vecFrames.empty())
		{
			QMessageBox::warning(this, "提示", "录像失败:未捕获到任何画面");
			return;
		}

		// 按录制时长与帧数反推帧率（限到1~60）,比固定帧率更贴近真实播放速度。
		double fps = m_timeRecordStart.isValid()
			? vecFrames.size() * 1000.0 / qMax<qint64>(1, m_timeRecordStart.msecsTo(QDateTime::currentDateTime()))
			: 25.0;
		fps = qBound(1.0, fps, 60.0);

		// 弹保存对话框选择路径；取消则丢弃缓存帧不保存。
		QString strDefaultName = QString("录像_%1.avi")
			.arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
		QString strPath = QFileDialog::getSaveFileName(this, "保存录像", strDefaultName, "AVI 视频 (*.avi)");
		if (strPath.isEmpty())
		{
			return;
		}

		// 后台线程将缓存帧编码成视频,避免长时间编码阻塞UI；
		// 帧已取走,与取流线程无共享数据,仅通过invokeMethod回UI线程提示结果。
		double dFps = fps;
		std::thread tdEncode([this, strPath, dFps, vecFrames = std::move(vecFrames)]() mutable {
			bool bOk = false;
			cv::VideoWriter writer;
			writer.open(strPath.toStdString(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
				dFps, vecFrames.front().size());
			if (writer.isOpened())
			{
				for (const auto& f : vecFrames)
				{
					writer.write(f);
				}
				writer.release();
				bOk = true;
			}
			// 回UI线程提示保存结果（用户取消已在弹框后提前处理）。
			QMetaObject::invokeMethod(this, [this, strPath, bOk]() {
				if (bOk)
				{
					if (m_pDeviceLog)
						m_pDeviceLog->Write("录像保存成功:" + strPath.toStdString());
					QMessageBox::information(this, "提示", QString("录像已保存到:\n%1").arg(strPath));
				}
				else
				{
					if (m_pDeviceLog)
						m_pDeviceLog->Write("录像失败:创建视频文件失败 " + strPath.toStdString());
					QMessageBox::warning(this, "提示", "录像保存失败:无法创建视频文件");
				}
				}, Qt::QueuedConnection);
			});
		tdEncode.detach();
	}
}

void Insulator_Zero_Value_Detection_Robot::On_Report_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("切换到报告界面");
	ui.stackedWidget_3->setCurrentIndex(2);
	if (overlayLabel) overlayLabel->hide();
}

void Insulator_Zero_Value_Detection_Robot::captureCurrentWindow(bool bInside)
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
	if (pixmap.isNull()) {
		if (m_pDeviceLog)
			m_pDeviceLog->Write("测量截图失败:像素数据为空");
		return;
	}

	// 测量截图保存路径:与软件根目录同级的 测量图像/工单名_杆塔号/相别/
	QString filePath = GetMeasureImageFileName(bInside);
	if (!pixmap.save(filePath))
	{
		// 测量流程中弹窗模态屏蔽主窗口,不弹提示框,仅记日志
		if (m_pDeviceLog)
			m_pDeviceLog->Write(("测量截图保存失败:" + filePath.toStdString()));
	}
	else if (m_pDeviceLog)
	{
		m_pDeviceLog->Write("测量截图保存成功:" + filePath.toStdString());
	}
}

QString Insulator_Zero_Value_Detection_Robot::GetMeasureImageFileName(bool bInside)
{
	// 存图根目录与软件根目录（exe目录）同级:../测量图像/
	QString strRoot = QDir::cleanPath(
		QDir(QString::fromStdString(WHSD_Tools::GetExeDirectory())).filePath(QStringLiteral("../测量图像")));
	// 一级子目录:工单名（线路名称）_杆塔号；二级子目录:当前comboBox（相别）
	QString strTicket = QString::fromStdString(m_CurrentTicketConfig.m_strLineName).trimmed();
	QString strPole = QString::fromStdString(m_CurrentTicketConfig.m_strPoleNumber).trimmed();
	QString strPhase = ui.comboBox->currentText();
	QString strDir = QDir::cleanPath(strRoot + "/" + strTicket + "_" + strPole + "/" + strPhase);
	QDir().mkpath(strDir);

	// 序号:同一相每完成一次测量追加一对内/外侧数据,当前侧序号 = 已有对数 + 1（重测后序号自动回退）
	QString strSide = ui.comboBox_2->currentText();
	int nSeq = GetMearDataArray(m_mapTicketMearData, strSide, strPhase).size() / 2 + 1;
	QString strSideName = bInside ? QStringLiteral("内测") : QStringLiteral("外侧");

	// 文件名:内测/外侧_序号_时间（含毫秒防重名）
	return QString("%1/%2_%3_%4.png").arg(strDir, strSideName).arg(nSeq)
		.arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz"));
}

void Insulator_Zero_Value_Detection_Robot::On_SetFileName_Click()
{
	// 此处弹出对话框选择一个文件夹
	QString filePath = QFileDialog::getExistingDirectory(this, "选择文件夹");
	if (!filePath.isEmpty()) {
		m_strFileName = filePath;
		if (m_pDeviceLog)
			m_pDeviceLog->Write("设置截图保存目录:" + filePath.toStdString());
		return;
	}
	QMessageBox::information(this, "提示", "文件保存失败");
}

void Insulator_Zero_Value_Detection_Robot::On_NewTicket_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("打开新建工单对话框");
	newTicketDialog->show();
}

void Insulator_Zero_Value_Detection_Robot::On_NewReport_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("新建报告,工单ID:" + m_CurrentTicketConfig.m_strTicketId);
	//获取当前工单的ID
	std::string strTicketId = m_CurrentTicketConfig.m_strTicketId;
	// 判断是否存在当前工单
	if(strTicketId.empty())
	{
		QMessageBox::information(this, "提示", "请先加载工单");
		return;
	}
	if (m_CurrentTicketConfig.m_bGenerateReport)
	{
		// 已存在报告,是否需要重新生成？
		QMessageBox::StandardButton reply = QMessageBox::question(this, "提示", "已存在报告,是否需要重新生成？", QMessageBox::Yes | QMessageBox::No);
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
		QMessageBox::StandardButton reply = QMessageBox::question(this, QStringLiteral("确认删除"), QStringLiteral("是否确认删除选中的工单？"), QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes) {
			// 获取被删除工单的配置信息
			CNewTicketConfig deletedTicket = ui.tableWidget_2->item(row, 0)->data(Qt::UserRole).value<CNewTicketConfig>();
			
			if (m_pDeviceLog)
				m_pDeviceLog->WriteFormat("删除工单:第 %d 行, ID:%s", row + 1, deletedTicket.m_strTicketId.c_str());
			
			// 从UI表格中移除该行
			ui.tableWidget_2->removeRow(row);
			
			// 从配置管理器中移除对应的工单配置并保存
			auto& vecTickets = m_pConfig->m_vecNewTicketConfig;
			for (auto it = vecTickets.begin(); it != vecTickets.end(); ++it)
			{
				if (it->m_strTicketId == deletedTicket.m_strTicketId)
				{
					vecTickets.erase(it);
					break;
				}
			}
			m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
		}
	}
}

void Insulator_Zero_Value_Detection_Robot::On_ChangeTicket_Click()
{
	int row = ui.tableWidget_2->currentRow();
	if (row < 0) {
		return;
	}

	if (m_pDeviceLog)
		m_pDeviceLog->WriteFormat("修改工单:第 %d 行", row + 1);
	CNewTicketConfig m_memNewTicketConfig = ui.tableWidget_2->item(row, 0)->data(Qt::UserRole).value<CNewTicketConfig>();
	newTicketDialog->SetTicket(m_memNewTicketConfig);
	newTicketDialog->show();
}

void Insulator_Zero_Value_Detection_Robot::On_LoadTicket_Click()
{
	int row = ui.tableWidget_2->currentRow();
	if (row < 0)return;
	m_CurrentTicketConfig = ui.tableWidget_2->item(row, 0)->data(Qt::UserRole).value<CNewTicketConfig>();

	m_mapTicketMearData = m_CurrentTicketConfig.m_mapTicketMearData;

	if (m_pDeviceLog)
		m_pDeviceLog->Write("加载工单:" + m_CurrentTicketConfig.m_strLineName + "_" + m_CurrentTicketConfig.m_strPoleNumber + " ID:" + m_CurrentTicketConfig.m_strTicketId);

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

	On_combobox_currentIndexChanged(0);

	// 表格列随comboBox的item,行随片数；双联时每相拆为内侧/外侧两列,每相各占一片数的行
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
	{
		m_pModelDataWidget->setTableLayout(tableHeaders, m_CurrentTicketConfig.m_wInsulatorSliceNum);
		bool bDouble = (m_CurrentTicketConfig.m_eBunchType == CNewTicketConfig::BunchType::eDouble);
		for (auto itSide = m_CurrentTicketConfig.m_mapTicketMearData.constBegin(); itSide != m_CurrentTicketConfig.m_mapTicketMearData.constEnd(); ++itSide)
		{
			QJsonObject objDira = itSide.value().toObject();
			for (auto itDira = objDira.constBegin(); itDira != objDira.constEnd(); ++itDira)
			{
				const QString strDira = itDira.key();
				const QJsonArray vecData = itDira.value().toArray();
				for (int i = 0; i < vecData.size(); ++i)
				{
					QString strHeader = strDira;
					if (bDouble)
						strHeader += (i % 2 == 0) ? QStringLiteral("内侧") : QStringLiteral("外侧");
					m_pModelDataWidget->appendValue(strHeader, vecData[i].toDouble());
				}
			}
		}
	}

	ui.comboBox->setEnabled(true);
	ui.comboBox_2->setEnabled(true);
	ui.pBTest->setEnabled(true);
	ui.pBRetest->setEnabled(true);
}

void Insulator_Zero_Value_Detection_Robot::On_DeleteReport_Click()
{
	int row = ui.tableWidget_3->currentRow();
	if (row >= 0) {
		QMessageBox::StandardButton reply = QMessageBox::question(this, QStringLiteral("确认删除"), QStringLiteral("是否确认删除选中的报告？"), QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes) {
			// 获取被删除报告的配置信息
			CNewReportConfig deletedReport = ui.tableWidget_3->item(row, 0)->data(Qt::UserRole).value<CNewReportConfig>();
			
			if (m_pDeviceLog)
				m_pDeviceLog->WriteFormat("删除报告:第 %d 行, ID:%s", row + 1, deletedReport.m_strReportId.c_str());
			
			// 从UI表格中移除该行
			ui.tableWidget_3->removeRow(row);
			
			// 从配置管理器中移除对应的报告配置并保存
			auto& vecReports = m_pConfig->m_vecNewReportConfig;
			for (auto it = vecReports.begin(); it != vecReports.end(); ++it)
			{
				if (it->m_strReportId == deletedReport.m_strReportId)
				{
					vecReports.erase(it);
					break;
				}
			}
			m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
		}
	}
}
void Insulator_Zero_Value_Detection_Robot::On_Test_Click()
{
	if (m_CurrentTicketConfig.m_strTicketId == "")
	{
		QMessageBox::information(this, "提示", "请加载一个工单");
		return;
	}
	// 上一次测量流程未结束,忽略重复触发（兼顾手柄等外部触发）
	if (m_nMeasureStep != 0)
		return;
	QString strDira = ui.comboBox->currentText();
	QString strSide = ui.comboBox_2->currentText();
	QJsonArray vecData = GetMearDataArray(m_mapTicketMearData, strSide, strDira);
	if (vecData.size() >= m_CurrentTicketConfig.m_wInsulatorSliceNum)
	{
		QMessageBox::information(this, "提示", "请 换相 或换 号侧 ！");
		return;
	}

	// 第一步:探针指向内测,按钮禁用并弹出不可关闭的等待窗,等待4s到位后执行第一次测量
	if (m_pDeviceLog)
		m_pDeviceLog->Write("开始测量:侧别=" + strSide.toStdString() + " 相别=" + strDira.toStdString());
	SetMeasureUiEnabled(false);
	ShowMeasureWaitDialog(QStringLiteral("探针指向内测,等待到位..."));

	auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
		m_pConfig->m_memControlBoardConfig.m_cUpAngle, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
	m_pComDevice->Write(cmds.data(), cmds.size());

	QTimer::singleShot(4000, this, [this]() {
		UpdateMeasureWaitDialog(QStringLiteral("正在执行第一次测量（内测）,等待结果..."));
		captureCurrentWindow(true);	// 到位后先截图保存（内测）,再执行第一次测量
		auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 1, 0);
		m_pComDevice->Write(cmds.data(), cmds.size());
		m_nMeasureStep = 1;
		if (m_pDeviceLog)
			m_pDeviceLog->Write("测量流程:发送第一次测量指令（内测）");
		});
}

void Insulator_Zero_Value_Detection_Robot::OnMeasureResult(int nStep)
{
	if(m_pDeviceLog)
		m_pDeviceLog->Write("测量结果:第" + std::to_string(nStep) + "步");
	if (nStep == 1)
	{
		bool bDouble = (m_CurrentTicketConfig.m_eBunchType == CNewTicketConfig::BunchType::eDouble);
		if (!bDouble)
		{
			// 单联:只测内侧一次,结果收到即探针复原,关闭等待窗并恢复按钮,测量结束
			if (m_pDeviceLog)
				m_pDeviceLog->Write("测量流程:单联内测结果已收到,探针复原,本次测量结束");
			UpdateMeasureWaitDialog(QStringLiteral("探针复原中..."));
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
				m_pConfig->m_memControlBoardConfig.m_cDownAngle, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
			m_pComDevice->Write(cmds.data(), cmds.size());

			m_nMeasureStep = 0;
			HideMeasureWaitDialog();
			SetMeasureUiEnabled(true);
			return;
		}

		// 双联:第一次（内测）结果已收到:探针打到外侧,等待4s到位后执行第二次测量
		if (m_pDeviceLog)
			m_pDeviceLog->Write("测量流程:内测结果已收到,探针切换到外侧");
		UpdateMeasureWaitDialog(QStringLiteral("探针切换到外侧,等待到位..."));
		auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
			m_pConfig->m_memControlBoardConfig.m_cUpAngle2, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
		m_pComDevice->Write(cmds.data(), cmds.size());

		QTimer::singleShot(4000, this, [this]() {
			UpdateMeasureWaitDialog(QStringLiteral("正在执行第二次测量（外侧）,等待结果..."));
			captureCurrentWindow(false);	// 到位后先截图保存（外侧）,再执行第二次测量
			auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 1, 0);
			m_pComDevice->Write(cmds.data(), cmds.size());
			m_nMeasureStep = 2;
			if (m_pDeviceLog)
				m_pDeviceLog->Write("测量流程:发送第二次测量指令（外侧）");
			});
	}
	else if (nStep == 2)
	{
		// 第二次（外侧）结果已收到:探针复原,关闭等待窗并恢复按钮,测量结束
		if (m_pDeviceLog)
			m_pDeviceLog->Write("测量流程:外侧结果已收到,探针复原,本次测量结束");
		UpdateMeasureWaitDialog(QStringLiteral("探针复原中..."));
		auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
			m_pConfig->m_memControlBoardConfig.m_cDownAngle, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
		m_pComDevice->Write(cmds.data(), cmds.size());

		m_nMeasureStep = 0;
		HideMeasureWaitDialog();
		SetMeasureUiEnabled(true);
	}
}

void Insulator_Zero_Value_Detection_Robot::SetMeasureUiEnabled(bool bEnable)
{
	ui.pBTest->setEnabled(bEnable);
	ui.pBRetest->setEnabled(bEnable);
	ui.comboBox->setEnabled(bEnable);
	ui.comboBox_2->setEnabled(bEnable);
	// 行走/探针/测量等手动操作按钮
	ui.pushButton_14->setEnabled(bEnable);
	ui.pushButton_15->setEnabled(bEnable);
	ui.pushButton_26->setEnabled(bEnable);
	ui.pushButton_17->setEnabled(bEnable);
	ui.pushButton_25->setEnabled(bEnable);
	ui.pushButton_27->setEnabled(bEnable);
	ui.pushButton_28->setEnabled(bEnable);
}

void Insulator_Zero_Value_Detection_Robot::ShowMeasureWaitDialog(const QString& strText)
{
	if (m_pMeasureWaitDialog == nullptr)
	{
		m_pMeasureWaitDialog = new QDialog(this);
		m_pMeasureWaitDialog->setWindowTitle(QStringLiteral("测量中"));
		// 无关闭按钮,并拦截Esc/关闭事件,测量结束前不可关闭
		m_pMeasureWaitDialog->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
		//m_pMeasureWaitDialog->setModal(true);
		m_pMeasureWaitDialog->installEventFilter(this);

		m_pMeasureWaitLabel = new QLabel(m_pMeasureWaitDialog);
		m_pMeasureWaitLabel->setAlignment(Qt::AlignCenter);
		m_pMeasureWaitLabel->setMinimumSize(320, 40);
		m_pMeasureWaitLabel->setStyleSheet("font-size:18px;");
		auto pLayout = new QVBoxLayout(m_pMeasureWaitDialog);
		pLayout->addWidget(m_pMeasureWaitLabel);
	}
	m_pMeasureWaitLabel->setText(strText);
	m_pMeasureWaitDialog->adjustSize();
	// 显示在主窗口最上方居中位置（模态弹窗置顶,屏蔽下层界面操作）
	m_pMeasureWaitDialog->move(mapToGlobal(QPoint(
		(width() - m_pMeasureWaitDialog->width()) / 2, 20)));
	m_pMeasureWaitDialog->show();
	m_pMeasureWaitDialog->raise();
}

void Insulator_Zero_Value_Detection_Robot::UpdateMeasureWaitDialog(const QString& strText)
{
	if (m_pMeasureWaitDialog == nullptr || !m_pMeasureWaitDialog->isVisible())
		return;
	m_pMeasureWaitLabel->setText(strText);
	m_pMeasureWaitDialog->adjustSize();
	m_pMeasureWaitDialog->move(mapToGlobal(QPoint(
		(width() - m_pMeasureWaitDialog->width()) / 2, 20)));
}

void Insulator_Zero_Value_Detection_Robot::HideMeasureWaitDialog()
{
	if (m_pMeasureWaitDialog != nullptr)
		m_pMeasureWaitDialog->hide();
}

bool Insulator_Zero_Value_Detection_Robot::eventFilter(QObject* obj, QEvent* event)
{
	if (m_pMeasureWaitDialog != nullptr && obj == m_pMeasureWaitDialog)
	{
		// 屏蔽Esc与关闭事件,弹窗只能由测量流程关闭
		if (event->type() == QEvent::Close)
			return true;
		if (event->type() == QEvent::KeyPress &&
			static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
			return true;
	}
	return QMainWindow::eventFilter(obj, event);
}

void Insulator_Zero_Value_Detection_Robot::keyPressEvent(QKeyEvent* event)
{
	// 键盘控制（与手柄操作对应,忽略长按自动重复）:
	// Q-探针向内  E-探针向外  R-探针复原  A/←-行走左  D/→-行走右  S-停止  空格-开始测量
	if (!event->isAutoRepeat())
	{
		switch (event->key())
		{
		case Qt::Key_Q:	// 探针向内（内测）
		{
			if (m_pDeviceLog)
				m_pDeviceLog->Write("键盘控制:探针向内（内测）");
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
				m_pConfig->m_memControlBoardConfig.m_cUpAngle, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("内上");
			return;
		}
		case Qt::Key_E:	// 探针向外（外侧）
		{
			if (m_pDeviceLog)
				m_pDeviceLog->Write("键盘控制:探针向外（外侧）");
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
				m_pConfig->m_memControlBoardConfig.m_cUpAngle2, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("外上");
			return;
		}
		case Qt::Key_R:	// 探针复原
		{
			if (m_pDeviceLog)
				m_pDeviceLog->Write("键盘控制:探针复原");
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
				m_pConfig->m_memControlBoardConfig.m_cDownAngle, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("复原");
			return;
		}
		case Qt::Key_A:
		case Qt::Key_Left:	// 行走向左
		{
			if (m_pDeviceLog)
				m_pDeviceLog->Write("键盘控制:行走向左");
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x01, 0b11, 0x02,
				m_pConfig->m_memControlBoardConfig.m_cWalkMotorSpeed);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("左");
			return;
		}
		case Qt::Key_D:
		case Qt::Key_Right:	// 行走向右
		{
			if (m_pDeviceLog)
				m_pDeviceLog->Write("键盘控制:行走向右");
			auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x01, 0b11, 0x01,
				m_pConfig->m_memControlBoardConfig.m_cWalkMotorSpeed);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("右");
			return;
		}
		case Qt::Key_S:	// 停止行走
		{
			if (m_pDeviceLog)
				m_pDeviceLog->Write("键盘控制:停止行走");
			On_stop_Click();
			ui.label_29->setText("");
			return;
		}
		case Qt::Key_Space:	// 开始测量（流程内部已防重复触发）
		{
			if (m_pDeviceLog)
				m_pDeviceLog->Write("键盘控制:开始测量");
			On_Test_Click();
			return;
		}
		default:
		{
			break;
		}
		}
	}
	QMainWindow::keyPressEvent(event);
}

void Insulator_Zero_Value_Detection_Robot::keyReleaseEvent(QKeyEvent* event)
{
	// 松开行走键时停止行走电机（与手柄松开方向键行为一致）
	if (!event->isAutoRepeat())
	{
		switch (event->key())
		{
		case Qt::Key_A:
		case Qt::Key_D:
		case Qt::Key_Left:
		case Qt::Key_Right:
		{
			if (m_pDeviceLog)
				m_pDeviceLog->Write("键盘控制:松开行走键,停止行走");
			auto cmds = CWHSDControlBoardProtocol::DeviceStop(0x01);
			m_pComDevice->Write(cmds.data(), cmds.size());
			ui.label_29->setText("");
			return;
		}
		default:
		{
			break;
		}
		}
	}
	QMainWindow::keyReleaseEvent(event);
}

void Insulator_Zero_Value_Detection_Robot::On_Retest_Click()
{
	if (m_CurrentTicketConfig.m_strTicketId == "")
	{
		QMessageBox::information(this, "提示", "请加载一个工单");
		return;
	}
	// 上一次测量/重测流程未结束,忽略重复触发（兼顾手柄等外部触发）
	if (m_nMeasureStep != 0)
		return;

	QString strDira = ui.comboBox->currentText();
	QString strSide = ui.comboBox_2->currentText();
	QJsonArray vecData = GetMearDataArray(m_mapTicketMearData, strSide, strDira);
	if (vecData.isEmpty())return;

	bool bDouble = (m_CurrentTicketConfig.m_eBunchType == CNewTicketConfig::BunchType::eDouble);

	if (bDouble)
	{
		// 双联复测:去除最近一对内/外侧测量值,再重测两次（内一次,外一次）
		// 偶数个时最近一对完整（内+外）,各删一个;奇数个时最近外侧缺失（流程中断）,只删最近内侧,复测后仍保持成对
		bool bRemoveOutside = (vecData.size() % 2 == 0);
		if (m_pDeviceLog)
			m_pDeviceLog->Write(std::string("重测（双联）:删除最近") + (bRemoveOutside ? "一对内/外侧测量值" : "一个内侧测量值")
				+ ",侧别=" + strSide.toStdString() + " 相别=" + strDira.toStdString() + ",重测内外侧各一次");

		// 同步删除表格/曲线中最近的内/外侧测量值,等待复测值回填
		if (m_pModelDataWidget)
		{
			m_pModelDataWidget->removeLastValue(strDira + QStringLiteral("内侧"));
			if (bRemoveOutside)
				m_pModelDataWidget->removeLastValue(strDira + QStringLiteral("外侧"));
		}
		for (int i = 0; i < (bRemoveOutside ? 2 : 1) && !vecData.isEmpty(); i++)
			vecData.removeLast();
		// 回写删除后的测量值数组
		SetMearDataArray(m_mapTicketMearData, strSide, strDira, vecData);

		// 重新执行完整双联测量流程:探针指向内测,等待4s到位后执行第一次测量,后续由OnMeasureResult推进
		SetMeasureUiEnabled(false);
		ShowMeasureWaitDialog(QStringLiteral("重测:探针指向内测,等待到位..."));
		auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
			m_pConfig->m_memControlBoardConfig.m_cUpAngle, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
		m_pComDevice->Write(cmds.data(), cmds.size());

		QTimer::singleShot(4000, this, [this]() {
			UpdateMeasureWaitDialog(QStringLiteral("正在执行重测第一次测量（内测）,等待结果..."));
			captureCurrentWindow(true);	// 到位后先截图保存（内测）,再执行第一次测量
			auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 1, 0);
			m_pComDevice->Write(cmds.data(), cmds.size());
			m_nMeasureStep = 1;
			if (m_pDeviceLog)
				m_pDeviceLog->Write("测量流程:发送重测第一次测量指令（内测）");
			});
	}
	else
	{
		// 单联复测:去除最近一个（内侧）测量值,只重测一次（内测）
		if (m_pDeviceLog)
			m_pDeviceLog->Write("重测（单联）:删除最近一个测量值,侧别=" + strSide.toStdString() + " 相别=" + strDira.toStdString() + ",重测一次（内测）");

		// 同步删除表格/曲线中最近一个测量值,等待复测值回填（单联表头无内/外侧后缀）
		if (m_pModelDataWidget)
			m_pModelDataWidget->removeLastValue(strDira);
		vecData.removeLast();
		// 回写删除后的测量值数组
		SetMearDataArray(m_mapTicketMearData, strSide, strDira, vecData);

		// 探针指向内测,等待4s到位后执行一次测量,后续由OnMeasureResult推进（单联收到结果即结束）
		SetMeasureUiEnabled(false);
		ShowMeasureWaitDialog(QStringLiteral("重测:探针指向内测,等待到位..."));
		auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
			m_pConfig->m_memControlBoardConfig.m_cUpAngle, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
		m_pComDevice->Write(cmds.data(), cmds.size());

		QTimer::singleShot(4000, this, [this]() {
			UpdateMeasureWaitDialog(QStringLiteral("正在执行重测测量（内测）,等待结果..."));
			captureCurrentWindow(true);	// 到位后先截图保存（内测）,再执行重测测量
			auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 1, 0);
			m_pComDevice->Write(cmds.data(), cmds.size());
			m_nMeasureStep = 1;
			if (m_pDeviceLog)
				m_pDeviceLog->Write("测量流程:发送重测测量指令（内测）");
			});
	}
}

void Insulator_Zero_Value_Detection_Robot::On_SaveMotorSpeed_Click()
{
	m_pConfig->m_memControlBoardConfig.m_cWalkMotorSpeed = ui.comboBox_3->currentIndex();	// 控制电机速度
	if (m_pDeviceLog)
		m_pDeviceLog->WriteFormat("保存电机速度:%d", (int)m_pConfig->m_memControlBoardConfig.m_cWalkMotorSpeed);
	m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
	QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("参数已保存"));
}

void Insulator_Zero_Value_Detection_Robot::On_SaveServoSpeed_Click()
{
    m_pConfig->m_memControlBoardConfig.m_cServoSpeed = ui.comboBox_4->currentIndex();
    if (m_pDeviceLog)
        m_pDeviceLog->WriteFormat("保存舵机速度:%d", (int)m_pConfig->m_memControlBoardConfig.m_cServoSpeed);
    m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
    QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("参数已保存"));
}

void Insulator_Zero_Value_Detection_Robot::On_SaveRobotIp_Click()
{
	m_pConfig->m_memControlBoardConfig.m_strIp = ui.lineEdit_9->text().toStdString();		// 设备IP
	if (m_pDeviceLog)
		m_pDeviceLog->Write("保存机器人IP:" + m_pConfig->m_memControlBoardConfig.m_strIp);
	m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
	m_pComDevice->SetParam(m_pConfig->m_memControlBoardConfig.m_strIp.c_str(),
		m_pConfig->m_memControlBoardConfig.m_wPort);
	QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("参数已保存"));
}

void Insulator_Zero_Value_Detection_Robot::On_SaveCameraIp_Click()
{
	// 记录修改前的摄像头IP,用于判断是否需要重启
	std::string oldLeftIp = m_pConfig->m_memCCameraConfig.m_strLeftIp;
	m_pConfig->m_memCCameraConfig.m_strLeftIp = ui.lineEdit_10->text().toStdString();		// 左摄像头IP
	if (m_pDeviceLog)
		m_pDeviceLog->Write("保存摄像头IP:" + m_pConfig->m_memCCameraConfig.m_strLeftIp);
	m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
	// 摄像头IP修改后需要重启软件才能生效
	if (m_pConfig->m_memCCameraConfig.m_strLeftIp != oldLeftIp)
	{
		QMessageBox::information(this, QStringLiteral("重启提示"),
			QStringLiteral("摄像头IP已修改,需要重启软件后才能生效。"));
		QProcess::startDetached(QApplication::applicationFilePath(), QStringList());
		QApplication::exit();
	}
}

void Insulator_Zero_Value_Detection_Robot::On_SaveProbeAngle_Click()
{
	m_pConfig->m_memControlBoardConfig.m_cUpAngle = ui.lineEdit_7->text().toInt();			// 探针向内的角度
	m_pConfig->m_memControlBoardConfig.m_cDownAngle = ui.lineEdit_14->text().toInt();		// 探针复原的角度
	m_pConfig->m_memControlBoardConfig.m_cUpAngle2 = ui.lineEdit_13->text().toInt();		// 探针向外的角度
	if (m_pDeviceLog)
		m_pDeviceLog->WriteFormat("保存探针角度:向内=%d 复原=%d 向外=%d",
			(int)m_pConfig->m_memControlBoardConfig.m_cUpAngle,
			(int)m_pConfig->m_memControlBoardConfig.m_cDownAngle,
			(int)m_pConfig->m_memControlBoardConfig.m_cUpAngle2);
	m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
	QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("参数已保存"));
}

void Insulator_Zero_Value_Detection_Robot::On_SaveInsuThreshold_Click()
{
	m_pConfig->m_memControlBoardConfig.m_wInsuThreshold = ui.lineEdit_15->text().toInt();	// 绝缘阈值
	if (m_pDeviceLog)
		m_pDeviceLog->WriteFormat("保存绝缘阈值:%d", (int)m_pConfig->m_memControlBoardConfig.m_wInsuThreshold);
	m_pConfig->Write(WHSD_Tools::GetAbsolutePath("Config.xml"));
	QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("参数已保存"));
}

void Insulator_Zero_Value_Detection_Robot::On_forword_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("按钮操作:行走向右（前进）");
	auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x01, 0b11, 0x01, 0x00);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_backward_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("按钮操作:行走向左（后退）");
	auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x01, 0b11, 0x02, 0x00);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_neddle1_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("按钮操作:探针向内（内测）");
	auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
		m_pConfig->m_memControlBoardConfig.m_cUpAngle, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_neddle2_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("按钮操作:探针向外（外侧）");
	auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
		m_pConfig->m_memControlBoardConfig.m_cUpAngle2, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_neddle3_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("按钮操作:探针复原");
	auto cmds = CWHSDControlBoardProtocol::DeviceRun(0x05, 0b11, 0x01,
		m_pConfig->m_memControlBoardConfig.m_cDownAngle, (m_pConfig->m_memControlBoardConfig.m_cServoSpeed + 1) * 25);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_stop_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("按钮操作:停止");
	auto cmds = CWHSDControlBoardProtocol::DeviceStop(0x01);
	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_mear_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("按钮操作:单次测量");
	auto cmds = CWHSDControlBoardProtocol::SensorCmd(0, 1, 0);

	m_pComDevice->Write(cmds.data(), cmds.size());
}

void Insulator_Zero_Value_Detection_Robot::On_WriteReport_Click()
{
	QHash<QString, QString> data;
	data["name"] = "张三";
	data["dept"] = "研发部 <嵌入式> & 测试";  // 特殊字符自动转义
	data["phone"] = "138-0000-0000";

	QString temPath = "D:\\xz\\template.docx";
	QString output = "D:\\xz\\output.docx";
	CWriteReports::FillDocxTemplate(temPath, output, data);
}

void Insulator_Zero_Value_Detection_Robot::On_combobox_currentIndexChanged(int index)
{
	QString strDira = ui.comboBox->currentText();
	QString strSide = ui.comboBox_2->currentText();
	if (m_pDeviceLog)
		m_pDeviceLog->Write("切换相别/侧别:侧别=" + strSide.toStdString() + " 相别=" + strDira.toStdString());
	QJsonArray vecData = GetMearDataArray(m_mapTicketMearData, strSide, strDira);
	bool visible = (m_CurrentTicketConfig.m_eBunchType == CNewTicketConfig::BunchType::eDouble);

	for (int i = 1; i <= 60; i++)
	{
		QString strName = QString("labelInside%1").arg(i);
		QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (label)
		{
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #1A202B;\n}");
		}
		strName = QString("labelOutside%1").arg(i);
		label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
		if (label)
		{
			label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #1A202B;\n}");
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
				float valueInside = vecData[2 * i - 2].toDouble();
				if(m_pConfig->m_memControlBoardConfig.m_wInsuThreshold<=valueInside)
				{
					label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #10b981;\n}");
				}
				else
				{
					label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #f56c6c;\n}");
				}
			}
			strName = QString("labelOutside%1").arg(i);
			label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
			if (2 * i - 1 >= vecData.size())continue;
			if (label)
			{
				float valueOutside = vecData[2 * i - 1].toDouble();
				if(m_pConfig->m_memControlBoardConfig.m_wInsuThreshold<=valueOutside)
				{
					label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #10b981;\n}");
				}
				else
				{
					label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #f56c6c;\n}");
				}
			}
		}
	}
	else // 单联
	{
		for (int i = 1; i <= vecData.size(); i++)
		{
			QString strName = QString("labelInside%1").arg(i);
			QLabel* label = ui.tabWidget_2Page1->findChild<QLabel*>(strName);
			if (label)
			{
				float valueInside = vecData[i - 1].toDouble();
				if(m_pConfig->m_memControlBoardConfig.m_wInsuThreshold<=valueInside)
				{
					label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #10b981;\n}");
				}
				else
				{
					label->setStyleSheet("QLabel { border-radius: 12px;\n    /* 可选:配套底色/边框按需加 */\n    background-color: #f56c6c;\n}");
				}
			}
		}
	}

}

void Insulator_Zero_Value_Detection_Robot::On_ChangeTicketSignal(CNewTicketConfig strTicket)
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("更新工单:" + strTicket.m_strTicketId + " " + strTicket.m_strLineName + "_" + strTicket.m_strPoleNumber);
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
	if (m_pDeviceLog)
		m_pDeviceLog->Write("新增报告:" + strReport.m_strReportId);
	//strReport.m_strReportId = GenerateUniqueReportId();
	int rowCount = ui.tableWidget_3->rowCount();
	ui.tableWidget_3->insertRow(rowCount);
	//strReport.m_mapTicketMearData = m_mapTicketMearData;

	ui.tableWidget_3->setItem(rowCount, 0, new QTableWidgetItem(QString::number(rowCount + 1)));
	ui.tableWidget_3->item(rowCount, 0)->setData(Qt::UserRole, QVariant::fromValue(strReport));
	ui.tableWidget_3->setItem(rowCount, 1, new QTableWidgetItem(QString::fromStdString(strReport.m_strReportId)));
	ui.tableWidget_3->setItem(rowCount, 2, new QTableWidgetItem(QString::fromStdString(strReport.m_strDetectionUnit)));
	ui.tableWidget_3->setItem(rowCount, 3, new QTableWidgetItem(QString::fromStdString(strReport.m_strDetectionPerson)));
	ui.tableWidget_3->setItem(rowCount, 4, new QTableWidgetItem(QString::fromStdString(strReport.m_strWorkPlace)));
	FilterReportTable();

	//更新m_bGenerateReport标志
	m_CurrentTicketConfig.m_bGenerateReport = true;
	for (auto& ticket : m_pConfig->m_vecNewTicketConfig)
	{
		if (ticket.m_strTicketId == m_CurrentTicketConfig.m_strTicketId)
		{
			ticket.m_bGenerateReport = true;
			break;
		}
	}

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
		//stream << "\xEF\xBB\xBF"; // UTF-8 BOM,防止Excel打开中文乱码
		//stream << "侧别,方向,序号,测量值\n";

		// 第一层:侧别（QJsonObject按key有序）
		for (auto itSide = m_mapTicketMearData.constBegin(); itSide != m_mapTicketMearData.constEnd(); ++itSide)
		{
			// 第二层:方向（QJsonObject按key有序）
			QJsonObject objDira = itSide.value().toObject();
			for (auto itDira = objDira.constBegin(); itDira != objDira.constEnd(); ++itDira)
			{
				stream << itSide.key() << ","
					<< itDira.key() << ",";
				// 数组:按存入顺序依次导出
				const QJsonArray vecData = itDira.value().toArray();
				for (int i = 0; i < vecData.size(); ++i)
				{

					stream << QString::number(vecData[i].toDouble(), 'f', 3) << ",";
				}
				stream << "\n";
			}
		}
		csvFile.close();
		if (m_pDeviceLog)
			m_pDeviceLog->Write("测量数据CSV保存成功:" + strCsvPath);
	}
	else
	{
		if (m_pDeviceLog)
			m_pDeviceLog->Write("测量数据CSV保存失败:" + strCsvPath);
		QMessageBox::warning(this, "错误", QString("测量数据CSV保存失败:\n%1").arg(strCsvPath));
	}
}

void Insulator_Zero_Value_Detection_Robot::On_ChangeReportSignal(CNewReportConfig strReport)
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("更新报告:" + strReport.m_strReportId);
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
	if (m_pDeviceLog)
		m_pDeviceLog->Write("新增工单:" + config.m_strTicketId + " " + config.m_strLineName + "_" + config.m_strPoleNumber);
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
		QMessageBox::warning(this, "错误", "截图失败,像素数据为空");
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
			if (m_pDeviceLog)
				m_pDeviceLog->Write("截图保存成功:" + filePath.toStdString());
			QMessageBox::information(this, "成功", QString("截图已保存到:\n%1").arg(filePath));
		}
		else {
			if (m_pDeviceLog)
				m_pDeviceLog->Write("截图保存失败:" + filePath.toStdString());
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
	// lineEdit:线路名称（列1）；lineEdit_3:检测人员（列9）,模糊匹配,留空则不参与过滤
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
	// lineEdit_4:线路名称（匹配线路信息列4）；lineEdit_6:检测人员（列3）,模糊匹配,留空则不参与过滤
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
	if (m_pDeviceLog)
		m_pDeviceLog->Write("重置工单查询条件");
	// 清空查询条件并恢复全部显示（textChanged会自动触发过滤）
	ui.lineEdit->clear();
	ui.lineEdit_2->clear();
	ui.lineEdit_3->clear();
	FilterTicketTable();
}

void Insulator_Zero_Value_Detection_Robot::On_ResetReport_Click()
{
	if (m_pDeviceLog)
		m_pDeviceLog->Write("重置报告查询条件");
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
