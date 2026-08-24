#include "newticketdialog.h"
#include <QFileDialog>
#include <QDateTime>

NewTicketDialog::NewTicketDialog(QWidget* parent)
	:QWidget(parent), m_bIsNewTicket(true)
{
	ui.setupUi(this);
	// 界面置于最前
    setWindowFlags(Qt::WindowStaysOnTopHint);

	// 开始/结束时间默认当前时间，并保证结束时间不早于开始时间
	ui.dateTimeEdit->setDateTime(QDateTime::currentDateTime());
	ui.dateTimeEdit_2->setDateTime(QDateTime::currentDateTime());

	connect(ui.pushButton, &QPushButton::clicked, this, &NewTicketDialog::on_buttonBox_accepted);
	connect(ui.pushButton_2, &QPushButton::clicked, this, &NewTicketDialog::on_buttonBox_rejected);
}

NewTicketDialog::~NewTicketDialog()
{
	/*delete ui;*/
}

void NewTicketDialog::on_buttonBox_accepted()
{
	CNewTicketConfig m_memNewTicketConfig = m_strTicket;
	m_memNewTicketConfig.m_strLineName = ui.lineEdit->text().toStdString();
	m_memNewTicketConfig.m_strPoleNumber = ui.lineEdit_2->text().toStdString();
	m_memNewTicketConfig.m_eBunchType = (CNewTicketConfig::BunchType)ui.comboBox->currentIndex();
	m_memNewTicketConfig.m_wInsulatorSliceNum = ui.lineEdit_3->text().toInt();
	m_memNewTicketConfig.m_eLoopType = (CNewTicketConfig::LoopType)ui.comboBox_2->currentIndex();
	m_memNewTicketConfig.m_strDetectionUnit = ui.lineEdit_4->text().toStdString();
	m_memNewTicketConfig.m_strRemark = ui.lineEdit_5->text().toStdString();
	m_memNewTicketConfig.m_eCurrentType = (CNewTicketConfig::CurrentType)ui.comboBox_3->currentIndex();
	m_memNewTicketConfig.m_strStartTime = ui.dateTimeEdit->dateTime().toString("yyyy-MM-dd HH:mm").toStdString();
	m_memNewTicketConfig.m_strEndTime = ui.dateTimeEdit_2->dateTime().toString("yyyy-MM-dd HH:mm").toStdString();
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

void NewTicketDialog::on_buttonBox_rejected()
{
	close();
}