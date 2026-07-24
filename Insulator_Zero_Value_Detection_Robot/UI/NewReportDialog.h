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

private slots:
	void on_buttonBox_accepted();
	void on_buttonBox_rejected();

private:
	Ui::NewReportDialogClass ui;
};

#endif // NEWREPORTDIALOG_H