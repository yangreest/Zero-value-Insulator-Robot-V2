#pragma once
#include <mutex>
#include <QtWidgets/QMainWindow>
#include "ui_Insulator_Zero_Value_Detection_Robot.h"
#include "Config/ConfigManager.h"
#include "DeviceCom/IDeviceCom.h"
#include "Log/ScanS_FC.h"
#include "Log/ScanS_WriteLog.h"
#include "Protocol/WHSDControlBoradProtocol.h"
#include "Tools/XInputHelper.h"
#include "Camera/CameraBase.h"
//#include "Config/XmlManagerWindow.h"
#include <opencv2/opencv.hpp>
#include <UI/NewTicketDialog.h>
#include <UI/NewReportDialog.h>

#include "UI/contentwidget.h"
#include "UI/modeldatawidget.h"

class Insulator_Zero_Value_Detection_Robot : public QMainWindow
{
	Q_OBJECT

public:
	Insulator_Zero_Value_Detection_Robot(QWidget* parent = nullptr);
	~Insulator_Zero_Value_Detection_Robot();
	// 在头文件中声明函数
	static QImage Mat2QImage(const cv::Mat& mat);

private slots:
	void On_timer_timeout();
	void On_timerInput_timeout();
	void On_TurnOnAll_Click(bool bState);
	void On_TurnOffAll_Click();
	void On_Close_Click();
	void On_Inspection_Click();
	void On_Ticket_Click();
	void On_pBSetting_Click();
	void On_Report_Click();
	void On_ZeroTest_Click();
	void On_Setting_Click();
	void captureCurrentWindow();
	void On_SetFileName_Click();
	void On_NewTicket_Click();
	void On_NewReport_Click();
	void On_DeleteTicket_Click();
	void On_ChangeTicket_Click();
	void On_LoadTicket_Click();
	void On_Test_Click();
	void On_Retest_Click();

	void On_forword_Click();
    void On_backward_Click();
	void On_neddle1_Click();
	void On_neddle2_Click();
	void On_neddle3_Click();
	void On_stop_Click();
	void On_mear_Click();

public slots:
	void On_NewTicketSignal(CNewTicketConfig strTicket);
	void On_ChangeTicketSignal(CNewTicketConfig strTicket);
    void On_NewReportSignal(CNewReportConfig strReport);

private:
	Ui::Insulator_Zero_Value_Detection_RobotClass ui;

	void Callback_DeviceHeartBeat(const CDeviceHeartBeat& b, int nComdeviceIndex);

	void InitUI();

	void InitParam();

	void BindAction();

	void ComDeviceConnectionChanged(const bool connected, int guid, int index);

	void CallBack_ControllerState(int t, const ControllerState* p);

	void RefreshControllerState(const ControllerState* p);

	void CallBack_SensorValue(CSensorData* p);

	void savePixmap(const QPixmap& pixmap);

	std::string GenerateUniqueTicketId();
	std::string GenerateUniqueReportId();

	//自定义显示label
	void SetVisibles(bool bVisible,int nSliceNum);

	ControllerState m_memControllerState;

	CXInputHelper* m_pXInputHelper;

	IDeviceCom* m_pComDevice;

	CConfigManager* m_pConfig;

	CWriteLog* m_pDeviceLog;

	CWHSDControlBoardProtocol* m_pWHSDControlBoardProtocol;

	//XmlManagerWindow* xmlManagerWindow;

	std::mutex m_mutexDeviceInfoLock;

	CDeviceHeartBeat m_memDeviceHeartBeat;

	CCFRD_Time m_time_LastHeartBeatTime;

	uint64_t m_nHeartBeatCount;

	bool m_bControlBroadConnected;

	std::mutex m_mutexXInput;

	QTimer* m_pTimer;
	QTimer* m_pTimerInput;

	int m_nLastDir;

	uint16_t m_wSensorStatus;

	uint16_t m_wSensorBat;

	uint16_t m_wSensorResult;

	uint64_t m_nTimeCount;

	bool m_bLastButton;

	QString	m_strFileName;

	bool continueStreaming;

	QLabel* overlayLabel;

	NewReportDialog* newReportDialog;
	NewTicketDialog* newTicketDialog;

	ContentWidget* m_activeWidget = nullptr;

public:
	// 摄像头
	void CameraConnect();
	void NewCameraConnect();
	ICameraBase* m_pC1;
	ICameraBase* m_pC2;

	std::string m_strLeftIp;
	std::string m_strRightIp;
};
