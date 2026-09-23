#include "PreviewReportDialog.h"
#include "Report/WriteReports.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFont>


PreviewReportDialog::PreviewReportDialog(QWidget* parent) :
	QDialog(parent)
{
	ui.setupUi(this);
	// 界面置于最前
	setWindowFlags(Qt::WindowStaysOnTopHint);
	// 中文默认字体，保证富文本预览中文正常显示
	ui.reportView->setFont(QFont(QStringLiteral("Microsoft YaHei"), 10));

	connect(ui.pBExport, &QPushButton::clicked, this, &PreviewReportDialog::On_Export_Click);
	connect(ui.pBClose, &QPushButton::clicked, this, &PreviewReportDialog::On_Close_Click);
}

PreviewReportDialog::~PreviewReportDialog()
{
}

void PreviewReportDialog::SetReportHtml(const QString& strHtml, const QString& strDefaultPdfName)
{
	m_strHtml = strHtml;
	m_strDefaultPdfName = strDefaultPdfName;
	ui.reportView->setHtml(strHtml);
}

void PreviewReportDialog::On_Export_Click()
{
	if (m_strHtml.isEmpty())
	{
		QMessageBox::information(this, "提示", "当前没有可导出的报告内容");
		return;
	}

	// 默认文件名: <线路>_<杆塔>_<报告编号>.pdf
	const QString strDefault = m_strDefaultPdfName.isEmpty()
		? QStringLiteral("检测报告") : m_strDefaultPdfName;
	QString filePath = QFileDialog::getSaveFileName(this, "导出报告",
		strDefault + ".pdf", "PDF 文件 (*.pdf)");
	if (filePath.isEmpty())
		return;

	if (CWriteReports::ExportHtmlToPdf(m_strHtml, filePath))
	{
		QMessageBox::information(this, "提示", "报告导出成功:\n" + filePath);
	}
	else
	{
		QMessageBox::warning(this, "错误", "报告导出失败:\n" + filePath);
	}
}

void PreviewReportDialog::On_Close_Click()
{
	close();
}
