#pragma once
#include <mutex>
#include <atomic>
#include <QDateTime>
#include <QJsonObject>
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

class QDialog;
class QKeyEvent;

class Insulator_Zero_Value_Detection_Robot : public QMainWindow
{
	Q_OBJECT

public:
	Insulator_Zero_Value_Detection_Robot(QWidget* parent = nullptr);
	~Insulator_Zero_Value_Detection_Robot();
	// 在头文件中声明函数
	static QImage Mat2QImage(const cv::Mat& mat);

signals:
	// ===== X值单点定标：等待下位机回测结果的信号 =====
	// 0x0F测量结果回报。回调运行在协议线程，用队列连接切到UI线程再处理
	void CalibMeasureValueSignal(double dValue);
	// 0x17校准命令应答（子命令/结果/原因/参数1/参数2，参数为毫值）
	void CalibAnswerSignal(quint8 cSubCmd, quint8 cResult, quint8 cReason, qint32 nValue1, qint32 nValue2);

private slots:
	void On_timer_timeout();
	void On_timerInput_timeout();
	void On_TurnOnAll_Click(bool bState);
	// 截图按钮
	void On_Screenshot_Click();
	void On_RefeshTable_Click();
	void On_Close_Click();
	void On_Inspection_Click();
	void On_Ticket_Click();
	void On_pBSetting_Click();
	void On_Report_Click();
	void On_ZeroTest_Click();
	// 录屏按钮
	void On_Record_Click(bool bState);
	//void On_Setting_Click();
	void captureCurrentWindow(bool bInside = true);
	void On_SetFileName_Click();
	void On_NewTicket_Click();
	void On_NewReport_Click();
	void On_DeleteTicket_Click();
	void On_ChangeTicket_Click();
	void On_LoadTicket_Click();
	void On_DeleteReport_Click();
	void On_Test_Click();
	void On_Retest_Click();


	// 保存电机速度
	void On_SaveMotorSpeed_Click();
	// 保存舵机的速度
	void On_SaveServoSpeed_Click();
	// 保存机器人IP
	void On_SaveRobotIp_Click();
	// 保存摄像头IP
	void On_SaveCameraIp_Click();
	// 保存探针角度
	void On_SaveProbeAngle_Click();
	// 保存绝缘阈值
	void On_SaveInsuThreshold_Click();

	// 工单查询重置
	void On_ResetTicket_Click();
	// 报告查询重置
	void On_ResetReport_Click();

	void On_forword_Click();
    void On_backward_Click();
	void On_neddle1_Click();
	void On_neddle2_Click();
	void On_neddle3_Click();
	void On_stop_Click();
	void On_mear_Click();
	void On_WriteReport_Click();
	void On_combobox_currentIndexChanged(int index);

	// ===== X值单点定标（groupBox_3），步骤自上而下顺序执行 =====
	// ① 恢复默认（0x17/0x05）：清空旧校准，测量通道直通
	void On_CalibReset_Click();
	// ② 测原始值（0x11触发 → 等0x0F回报，连测3次取平均）
	void On_CalibMeasure_Click();
	// ③ 下发定标（0x17/0x0E）：标准值 + 原始值平均值
	void On_CalibDo_Click();
	// ④ 验证：触发实测，对比0x0F回报的修正值与标准值
	void On_CalibVerify_Click();
	// ④ 验证：不挂电阻的离线抽查（0x17/0x0A）
	void On_CalibCheck_Click();
	// 辅助：读回Flash中已存系数（0x17/0x0C）
	void On_CalibReadCoef_Click();

	// 等待回测结果的槽：收到一次0x0F测量值（已切到UI线程）
	void On_CalibMeasureValue(double dValue);
	// 等待回测结果的槽：收到一次0x17校准应答（已切到UI线程）
	void On_CalibAnswer(quint8 cSubCmd, quint8 cResult, quint8 cReason, qint32 nValue1, qint32 nValue2);
	// 等待超时：0x0F约3.2秒回报、0x0E写Flash需1~2秒，超时结束本轮等待
	void On_CalibTimeout();

public slots:
	void On_NewTicketSignal(CNewTicketConfig strTicket);
	void On_ChangeTicketSignal(CNewTicketConfig strTicket);
    void On_NewReportSignal(CNewReportConfig strReport);
	void On_ChangeReportSignal(CNewReportConfig strReport);

private:
	Ui::Insulator_Zero_Value_Detection_RobotClass ui;

	void Callback_DeviceHeartBeat(const CDeviceHeartBeat& b, int nComdeviceIndex);

	void InitUI();

	void InitParam();

	void BindAction();

	void ComDeviceConnectionChanged(const bool connected, int guid, int index);

	void CallBack_ControllerState(int t, const ControllerState* p);

	//void RefreshControllerState(const ControllerState* p);

	void CallBack_SensorValue(CSensorData* p);

	void CallBack_ZeroValue(float* p);

	// 协议线程回调：0x17校准命令应答，转成信号切到UI线程
	void CallBack_CalibAnswer(const CCalibAnswer& answer);

	void savePixmap(const QPixmap& pixmap);

	// 测量流程中截图保存，文件名含内测/外侧标记、序号与时间（仅UI线程调用）
	QString GetMeasureImageFileName(bool bInside);

	std::string GenerateUniqueTicketId();
	std::string GenerateUniqueReportId();

	//自定义显示label
	void SetVisibles(bool bVisible,int nSliceNum);

	// 填充tableWidget_2指定行的工单数据（全部列）
	void SetTicketRow(int row, const CNewTicketConfig& ticket);

	// 测量流程推进，nStep为结果对应的步骤（1=内测 2=外侧;单联时步骤1收到结果即结束,复测复用同一流程）
	void OnMeasureResult(int nStep);

	// 测量期间禁用/恢复相关按钮与下拉框（弹窗模态已挡界面，此处兼顾手柄等外部触发）
	void SetMeasureUiEnabled(bool bEnable);

	// 显示/更新/关闭不可关闭的测量等待弹窗（显示在主窗口最上方）
	void ShowMeasureWaitDialog(const QString& strText);
	void UpdateMeasureWaitDialog(const QString& strText);
	void HideMeasureWaitDialog();

protected:
	// 拦截等待弹窗的Esc/关闭事件，保证测量结束前不可关闭
	bool eventFilter(QObject* obj, QEvent* event) override;

	// 键盘控制：探针位置/行走/停止/开始测量（与手柄操作对应）
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;

private:

	// 根据lineEdit、lineEdit_3过滤tableWidget_2
	void FilterTicketTable();

	// 根据lineEdit_4、lineEdit_6过滤tableWidget_3
	void FilterReportTable();

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

	// 录像状态标志（UI线程写，取流线程读）
	std::atomic<bool> m_bRecording{ false };
	// 录像帧缓存：录制期间取流线程逐帧追加，停止后统一转成视频文件；
	// 写帧用clone避免grab复用缓冲区，互斥锁保护跨线程读写。
	std::mutex m_mutexRecordBuf;
	std::vector<cv::Mat> m_vecRecordFrames;
	// 录像开始时间，停止时结合帧数反推帧率（仅UI线程读写）
	QDateTime m_timeRecordStart;

	NewReportDialog* newReportDialog;
	NewTicketDialog* newTicketDialog;

	ContentWidget* m_activeWidget = nullptr;

	// 电阻值表格/曲线控件（列随comboBox、行随片数）
	ModelDataWidget* m_pModelDataWidget = nullptr;

	// 测量数据（JSON格式）:第一层key为侧别,第二层key为相别/方向,值为该相测量值数组(QJsonArray)
	QJsonObject m_mapTicketMearData;

	CNewTicketConfig m_CurrentTicketConfig;

	// 测量流程状态：0=空闲 1=等待第一次（内测）结果 2=等待第二次（外侧）结果，仅UI线程读写
	int m_nMeasureStep = 0;

	// 测量等待弹窗及其提示文本（懒创建，复用）
	QDialog* m_pMeasureWaitDialog = nullptr;
	QLabel* m_pMeasureWaitLabel = nullptr;

	// ===== X值单点定标流程状态 =====
	// 步骤自上而下顺序执行，每步发出命令后进入等待态，靠信号槽收结果推进
	enum ECalibStep
	{
		eCalibIdle = 0,		// 空闲，可发起步骤①
		eCalibWaitReset,		// 等0x05恢复默认应答
		eCalibWaitMeasure,	// 等0x0F原始值回报（连测3次）
		eCalibWaitDo,			// 等0x0E定标应答
		eCalibWaitVerify,		// 等0x0F修正值回报（验证）
		eCalibWaitAux			// 等0x0C读回系数/0x0A离线抽查应答
	};

	// 定标流程日志追加到textEdit_CalibLog（仅UI线程调用）
	void CalibAppendLog(const QString& strText);

	// 刷新步骤状态标签：nType 0=未执行/灰 1=进行中/蓝 2=成功/绿 3=失败/红
	void CalibSetStepState(QLabel* pLabel, const QString& strText, int nType);

	// 定标期间禁用/恢复groupBox_3内的按钮与输入框，避免重入
	void CalibSetUiEnabled(bool bEnable);

	// 进入等待态：记录步骤、启动单次超时定时器、禁用按钮
	// 工单测量流程正在进行时返回false（定标会改道0x0F结果，两者互斥）
	bool CalibStartWait(ECalibStep eStep, int nTimeoutMs);

	// 结束等待态：停定时器、回到空闲
	void CalibStopWait();

	// 发0x11触发一次检测（0x0F约3.2秒后主动上报，超时按5秒设）
	void CalibTriggerMeasure();

	// 发送一帧并记录HEX日志（便于与协议文档示例帧逐字节比对），未连接返回false
	bool CalibSendFrame(const std::vector<uint8_t>& vecData);

	// 读取标准电阻值输入框并转成int32毫值，非法返回false
	bool CalibGetStdMilli(qint32& nStdMilli);

	// 定标失败原因码转文字（0x01原始值超时 0x04Flash写失败 0x05参数长度错 0x09参数非法）
	static QString CalibReasonText(quint8 cReason);

	ECalibStep m_eCalibStep = eCalibIdle;

	// 等待下位机应答/回报的单次超时定时器
	QTimer* m_pCalibTimeoutTimer = nullptr;

	// 已完成的原值测量次数（0~3），满3次取平均
	int m_nCalibMeasureIndex = 0;

	// 三次原始值与其平均值（MΩ）
	double m_arCalibRaw[3] = { 0.0, 0.0, 0.0 };
	double m_dCalibRawAvg = 0.0;

	// 定标测量中标志：UI线程写、协议线程读，置位时0x0F结果走定标流程而非工单记录
	std::atomic<bool> m_bCalibMeasuring{ false };

	// 步骤完成标志：保证自上而下顺序执行，上一步未完成不放开下一步
	bool m_bCalibResetDone = false;	// ①已恢复默认
	bool m_bCalibRawDone = false;	// ②已取得原始值平均
	bool m_bCalibDoDone = false;		// ③系数已写入Flash

public:
	// 摄像头
	void CameraConnect();
	void NewCameraConnect();
	ICameraBase* m_pC1;
	ICameraBase* m_pC2;

	std::string m_strLeftIp;
	std::string m_strRightIp;
};
