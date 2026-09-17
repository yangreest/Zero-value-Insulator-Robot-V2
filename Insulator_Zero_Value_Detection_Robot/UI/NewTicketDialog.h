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
	// 新建工单前重置:清空全部输入与内部缓存的工单配置,避免残留上一个工单(尤其是历史检测数据)
	void ResetForNew();
private slots:
	void on_buttonBox_accepted();
	void on_buttonBox_rejected();

private:
	Ui::NewTicketDialogClass ui;

	bool m_bIsNewTicket;
	CNewTicketConfig m_strTicket;
};

#endif // NEWTICKETDIALOG_H