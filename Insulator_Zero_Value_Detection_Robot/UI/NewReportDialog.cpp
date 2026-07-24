#include "newreportdialog.h"


NewReportDialog::NewReportDialog(QWidget* parent) :
	QDialog(parent)
{
	ui.setupUi(this);
    connect(ui.pushButton, &QPushButton::clicked, this, &NewReportDialog::on_buttonBox_accepted);

}

NewReportDialog::~NewReportDialog()
{
	//delete ui;
}

void NewReportDialog::on_buttonBox_accepted()
{
	CNewReportConfig strReport;
    strReport.m_strReportId = ui.lineEdit->text().toStdString();
    strReport.m_strDetectionUnit = ui.lineEdit_2->text().toStdString();
    strReport.m_strDetectionPerson = ui.lineEdit_3->text().toStdString();
    strReport.m_strWorkPlace = ui.lineEdit_4->text().toStdString();

	emit NewReportSignal(strReport);
	close();
}

void NewReportDialog::on_buttonBox_rejected()
{
	close();
}
