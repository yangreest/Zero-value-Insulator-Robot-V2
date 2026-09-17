#include "newticketdialog.h"
#include <QFileDialog>
#include <QDateTime>
#include <QMessageBox>

NewTicketDialog::NewTicketDialog(QWidget* parent)
	:QWidget(parent), m_bIsNewTicket(true)
{
	ui.setupUi(this);
	// 界面置于最前
    setWindowFlags(Qt::WindowStaysOnTopHint);

	// 开始/结束时间为检测任务真实起止时间,由检测流程自动打点,不允许手动编辑
	ui.dateTimeEdit->setDateTime(QDateTime::currentDateTime());
	ui.dateTimeEdit_2->setDateTime(QDateTime::currentDateTime());
	ui.dateTimeEdit->setReadOnly(true);
	ui.dateTimeEdit_2->setReadOnly(true);
	ui.dateTimeEdit->setEnabled(false);
	ui.dateTimeEdit_2->setEnabled(false);

	connect(ui.pushButton, &QPushButton::clicked, this, &NewTicketDialog::on_buttonBox_accepted);
	connect(ui.pushButton_2, &QPushButton::clicked, this, &NewTicketDialog::on_buttonBox_rejected);
}

NewTicketDialog::~NewTicketDialog()
{
	/*delete ui;*/
}

void NewTicketDialog::on_buttonBox_accepted()
{
	// 校验名称不能为空
	if (ui.lineEdit->text().trimmed().isEmpty())
	{
		QMessageBox::warning(this, "提示", "名称不能为空");
		return;
	}

	// 校验数量要在1到60之间
	int sliceNum = ui.lineEdit_3->text().toInt();
	if (sliceNum < 1 || sliceNum > 60)
	{
		QMessageBox::warning(this, "提示", "数量要在1到60之间");
		return;
	}

	CNewTicketConfig m_memNewTicketConfig = m_strTicket;
	m_memNewTicketConfig.m_strLineName = ui.lineEdit->text().toStdString();
	m_memNewTicketConfig.m_strPoleNumber = ui.lineEdit_2->text().toStdString();
	m_memNewTicketConfig.m_eBunchType = (CNewTicketConfig::BunchType)ui.comboBox->currentIndex();
	m_memNewTicketConfig.m_wInsulatorSliceNum = sliceNum;
	m_memNewTicketConfig.m_eLoopType = (CNewTicketConfig::LoopType)ui.comboBox_2->currentIndex();
	m_memNewTicketConfig.m_strDetectionUnit = ui.lineEdit_4->text().toStdString();
	m_memNewTicketConfig.m_strRemark = ui.lineEdit_5->text().toStdString();
	m_memNewTicketConfig.m_eCurrentType = (CNewTicketConfig::CurrentType)ui.comboBox_3->currentIndex();
	// 开始/结束时间为检测真实起止,由检测流程自动打点,此处保留 m_strTicket 中已有值,不从控件写入
	m_memNewTicketConfig.m_strDetectionPerson = ui.lineEdit_6->text().toStdString();
	if (m_bIsNewTicket)
		emit NewTicketSignal(m_memNewTicketConfig);
	else
	{
		emit ChangeTicketSignal(m_memNewTicketConfig);
		m_bIsNewTicket = true;
	}
	close();
}

void NewTicketDialog::SetTicket(CNewTicketConfig strTicket)
{
	m_strTicket = strTicket;
	ui.lineEdit->setText(QString::fromStdString(strTicket.m_strLineName));
	ui.lineEdit_2->setText(QString::fromStdString(strTicket.m_strPoleNumber));
	ui.comboBox->setCurrentIndex((int)strTicket.m_eBunchType);
	ui.lineEdit_3->setText(QString::number(strTicket.m_wInsulatorSliceNum));
	ui.comboBox_2->setCurrentIndex((int)strTicket.m_eLoopType);
	ui.lineEdit_4->setText(QString::fromStdString(strTicket.m_strDetectionUnit));
	ui.lineEdit_5->setText(QString::fromStdString(strTicket.m_strRemark));
	ui.comboBox_3->setCurrentIndex((int)strTicket.m_eCurrentType);
	ui.dateTimeEdit->setDateTime(QDateTime::fromString(QString::fromStdString(strTicket.m_strStartTime), "yyyy-MM-dd HH:mm"));
	ui.dateTimeEdit_2->setDateTime(QDateTime::fromString(QString::fromStdString(strTicket.m_strEndTime), "yyyy-MM-dd HH:mm"));
	ui.lineEdit_6->setText(QString::fromStdString(strTicket.m_strDetectionPerson));

	m_bIsNewTicket = false;
}

void NewTicketDialog::ResetForNew()
{
	// 清空内部缓存的工单配置(含历史测量数据),恢复为新建态
	m_strTicket = CNewTicketConfig();
	m_bIsNewTicket = true;

	// 清空全部输入框并复位下拉框
	ui.lineEdit->clear();
	ui.lineEdit_2->clear();
	ui.lineEdit_3->clear();
	ui.lineEdit_4->clear();
	ui.lineEdit_5->clear();
	ui.lineEdit_6->clear();
	ui.comboBox->setCurrentIndex(0);
	ui.comboBox_2->setCurrentIndex(0);
	ui.comboBox_3->setCurrentIndex(0);
	// 起止时间只读,新建时尚未开始检测,置当前时间仅作占位显示
	ui.dateTimeEdit->setDateTime(QDateTime::currentDateTime());
	ui.dateTimeEdit_2->setDateTime(QDateTime::currentDateTime());
}

void NewTicketDialog::on_buttonBox_rejected()
{
	close();
}