#include "newticketdialog.h"


NewTicketDialog::NewTicketDialog(QWidget* parent) :
	QDialog(parent)
{
	ui->setupUi(this);
}

NewTicketDialog::~NewTicketDialog()
{
	delete ui;
}

void NewTicketDialog::on_buttonBox_accepted()
{
	// 处理确定按钮点击事件
	accept(); // 关闭对话框并返回Accepted
}

void NewTicketDialog::on_buttonBox_rejected()
{
	// 处理取消按钮点击事件
	reject(); // 关闭对话框并返回Rejected
}