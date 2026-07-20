#pragma once
#ifndef NEWREPORTDIALOG_H
#define NEWREPORTDIALOG_H

#include <QDialog>
#include "ui_newreportdialog.h"


class NewReportDialog : public QDialog
{
	Q_OBJECT

public:
	explicit NewReportDialog(QWidget* parent = nullptr);
	~NewReportDialog();

private slots:
	void on_buttonBox_accepted();
	void on_buttonBox_rejected();

private:
	Ui::NewReportDialogClass* ui;
};

#endif // NEWREPORTDIALOG_H