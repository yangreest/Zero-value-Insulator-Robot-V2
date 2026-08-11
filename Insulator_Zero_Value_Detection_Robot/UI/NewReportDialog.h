#pragma once
#ifndef NEWREPORTDIALOG_H
#define NEWREPORTDIALOG_H

#include <QDialog>
#include "ui_newreportdialog.h"
#include "Config/ConfigManager.h"


class NewReportDialog : public QDialog
{
	Q_OBJECT

public:
	explicit NewReportDialog(QWidget* parent = nullptr);
	~NewReportDialog();
signals:
	void NewReportSignal(CNewReportConfig strReport);
	void ChangeReportSignal(CNewReportConfig strReport);
public:
	void SetReport(CNewReportConfig strReport);
private slots:
	void on_buttonBox_accepted();
	void on_buttonBox_rejected();

private:
	Ui::NewReportDialogClass ui;

	//bool m_bIsNewReport;
	CNewReportConfig m_strReport;
};

#endif // NEWREPORTDIALOG_H