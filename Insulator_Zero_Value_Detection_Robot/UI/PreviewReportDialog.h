#pragma once
#ifndef PREVIEWREPORTDIALOG_H
#define PREVIEWREPORTDIALOG_H

#include <QDialog>
#include "ui_previewreportdialog.h"


class PreviewReportDialog : public QDialog
{
	Q_OBJECT

public:
	explicit PreviewReportDialog(QWidget* parent = nullptr);
	~PreviewReportDialog();

	// 设置报告富文本内容（HTML）与默认导出文件名（不含 .pdf 后缀）
	void SetReportHtml(const QString& strHtml, const QString& strDefaultPdfName);

private slots:
	void On_Export_Click();
	void On_Close_Click();

private:
	Ui::PreviewReportDialogClass ui;

	QString m_strHtml;
	QString m_strDefaultPdfName;
};

#endif // PREVIEWREPORTDIALOG_H
