#pragma once
#ifndef NEWTICKETDIALOG_H
#define NEWTICKETDIALOG_H

#include <QDialog>
#include "ui_newticketdialog.h"


class NewTicketDialog : public QDialog
{
	Q_OBJECT

public:
	explicit NewTicketDialog(QWidget* parent = nullptr);
	~NewTicketDialog();

private slots:
	void on_buttonBox_accepted();
	void on_buttonBox_rejected();

private:
	Ui::NewTicketDialogClass* ui;
};

#endif // NEWTICKETDIALOG_H