#include "newreportdialog.h"


NewReportDialog::NewReportDialog(QWidget* parent) :
	QDialog(parent)
{
	ui->setupUi(this);
}

NewReportDialog::~NewReportDialog()
{
	delete ui;
}

void NewReportDialog::on_buttonBox_accepted()
{
	// 处理确定按钮点击事件
	accept(); // 关闭对话框并返回Accepted
}

void NewReportDialog::on_buttonBox_rejected()
{
	// 处理取消按钮点击事件
	reject(); // 关闭对话框并返回Rejected
}
