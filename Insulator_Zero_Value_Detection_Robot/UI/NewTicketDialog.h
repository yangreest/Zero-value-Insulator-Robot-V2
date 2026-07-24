#pragma once
#ifndef NEWTICKETDIALOG_H
#define NEWTICKETDIALOG_H

#include <QDialog>
#include "ui_newticketdialog.h"
#include "Config/ConfigManager.h"


class NewTicketDialog : public QWidget
{
	Q_OBJECT

public:
	explicit NewTicketDialog(QWidget* parent = nullptr);
	~NewTicketDialog();

signals:
	void NewTicketSignal(CNewTicketConfig strTicket);
	void ChangeTicketSignal(CNewTicketConfig strTicket);

public:
	void SetTicket(CNewTicketConfig strTicket);
private slots:
	void on_buttonBox_accepted();
	void on_buttonBox_rejected();

private:
	Ui::NewTicketDialogClass ui;

	bool m_bIsNewTicket;
};

#endif // NEWTICKETDIALOG_H