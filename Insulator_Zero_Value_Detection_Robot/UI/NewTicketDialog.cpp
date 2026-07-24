#include "newticketdialog.h"
#include <QFileDialog>

NewTicketDialog::NewTicketDialog(QWidget* parent)
	:QWidget(parent), m_bIsNewTicket(true)
{
	ui.setupUi(this);

	connect(ui.pushButton, &QPushButton::clicked, this, &NewTicketDialog::on_buttonBox_accepted);
	connect(ui.pushButton_2, &QPushButton::clicked, this, &NewTicketDialog::on_buttonBox_rejected);
}

NewTicketDialog::~NewTicketDialog()
{
	/*delete ui;*/
}

void NewTicketDialog::on_buttonBox_accepted()
{
	CNewTicketConfig m_memNewTicketConfig;
	m_memNewTicketConfig.m_strLineName = ui.lineEdit->text().toStdString();
	m_memNewTicketConfig.m_strPoleNumber = ui.lineEdit_2->text().toStdString();
	m_memNewTicketConfig.m_eBunchType = (CNewTicketConfig::BunchType)ui.comboBox->currentIndex();
	m_memNewTicketConfig.m_wInsulatorSliceNum = ui.lineEdit_3->text().toInt();
	m_memNewTicketConfig.m_eLoopType = (CNewTicketConfig::LoopType)ui.comboBox_2->currentIndex();
	m_memNewTicketConfig.m_strDetectionUnit = ui.lineEdit_4->text().toStdString();
	m_memNewTicketConfig.m_strRemark = ui.lineEdit_5->text().toStdString();
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
	ui.lineEdit->setText(QString::fromStdString(strTicket.m_strLineName));
	ui.lineEdit_2->setText(QString::fromStdString(strTicket.m_strPoleNumber));
	ui.comboBox->setCurrentIndex((int)strTicket.m_eBunchType);
	ui.lineEdit_3->setText(QString::number(strTicket.m_wInsulatorSliceNum));
	ui.comboBox_2->setCurrentIndex((int)strTicket.m_eLoopType);
	ui.lineEdit_4->setText(QString::fromStdString(strTicket.m_strDetectionUnit));
	ui.lineEdit_5->setText(QString::fromStdString(strTicket.m_strRemark));

	m_bIsNewTicket = false;
}

void NewTicketDialog::on_buttonBox_rejected()
{
	close();
}