#include "newreportdialog.h"


NewReportDialog::NewReportDialog(QWidget* parent) :
	QDialog(parent)/*, m_bIsNewReport(true)*/
{
	ui.setupUi(this);
	// 界面置于最前
	setWindowFlags(Qt::WindowStaysOnTopHint);
    connect(ui.pushButton, &QPushButton::clicked, this, &NewReportDialog::on_buttonBox_accepted);

}

NewReportDialog::~NewReportDialog()
{
	//delete ui;
}

void NewReportDialog::on_buttonBox_accepted()
{
	CNewReportConfig strReport = m_strReport;
    strReport.m_strReportId = ui.lineEdit->text().toStdString();
    strReport.m_strDetectionUnit = ui.lineEdit_2->text().toStdString();
    strReport.m_strDetectionPerson = ui.lineEdit_3->text().toStdString();
    strReport.m_strWorkPlace = ui.lineEdit_4->text().toStdString();
	//if(m_bIsNewReport)
		emit NewReportSignal(strReport);
	//else
	//{
       // emit ChangeReportSignal(strReport);
		//m_bIsNewReport = true;
	//}
	close();
}

void NewReportDialog::SetReport(CNewReportConfig strReport)
{
	m_strReport = strReport;
    ui.lineEdit->setText(QString::fromStdString(strReport.m_strReportId));
    ui.lineEdit_2->setText(QString::fromStdString(strReport.m_strDetectionUnit));
    ui.lineEdit_3->setText(QString::fromStdString(strReport.m_strDetectionPerson));
    ui.lineEdit_4->setText(QString::fromStdString(strReport.m_strWorkPlace));
	//m_bIsNewReport = false;
}

void NewReportDialog::on_buttonBox_rejected()
{
	close();
}
