#include "WriteReports.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

// XML 文本转义（数据含 < > & 时 Word 才能正常显示）
QString CWriteReports::XmlEscape(const QString& strText)
{
	QString strResult = strText;
	strResult.replace('&', "&amp;");
	strResult.replace('<', "&lt;");
	strResult.replace('>', "&gt;");
	return strResult;
}

// 合并被 Word 拆开的 run，使 ${xxx} 变成连续文本
QString CWriteReports::MergeAdjacentRuns(const QString& strXml)
{
	QString strResult = strXml;
	QRegularExpression re1("</w:t></w:r><w:r>(?:<w:rPr>.*?</w:rPr>)?<w:t>",
		QRegularExpression::DotMatchesEverythingOption);
	strResult.replace(re1, "</w:t><w:t>"); // 跨 run 合并
	strResult.remove("</w:t><w:t>");       // 同 run 内多个 w:t 合并
	return strResult;
}

// 替换 word/document.xml 中的全部占位符
QString CWriteReports::FillPlaceholders(const QString& strXml, const QHash<QString, QString>& mapData)
{
	QString strResult = MergeAdjacentRuns(strXml);
	for (auto it = mapData.constBegin(); it != mapData.constEnd(); ++it)
		strResult.replace("${" + it.key() + "}", XmlEscape(it.value()));
	return strResult;
}

// 填充 docx 模板，生成报告
bool CWriteReports::FillDocxTemplate(
	const QString& strTemplatePath,
	const QString& strOutputPath,
	const QHash<QString, QString>& mapData)
{
	// 校验路径
	if (strTemplatePath.isEmpty() || strOutputPath.isEmpty())
	{
		qWarning() << "模板或输出路径为空";
		return false;
	}

	if (QFileInfo(strTemplatePath).absoluteFilePath() == QFileInfo(strOutputPath).absoluteFilePath())
	{
		qWarning() << "模板与输出路径相同:" << strTemplatePath;
		return false;
	}

	// 打开模板 docx
	QZipReader reader(strTemplatePath);
	if (!reader.exists())
	{
		qWarning() << "模板打开失败:" << strTemplatePath;
		return false;
	}

	const QList<QZipReader::FileInfo> infos = reader.fileInfoList();

	// 取出正文 XML 并填充占位符
	QByteArray docXml = reader.fileData("word/document.xml");
	if (docXml.isEmpty())
	{
		qWarning() << "word/document.xml 为空";
		return false;
	}
	QByteArray newDocXml = FillPlaceholders(QString::fromUtf8(docXml), mapData).toUtf8();

	// 输出目录不存在时自动创建
	QString strOutputDir = QFileInfo(strOutputPath).absolutePath();
	if (!strOutputDir.isEmpty() && !QDir().mkpath(strOutputDir))
	{
		qWarning() << "创建输出目录失败:" << strOutputDir;
		return false;
	}

	// 逐部件写入输出 docx：document.xml 用填充后的内容，其余部件原样保留
	QZipWriter writer(strOutputPath);
	for (const QZipReader::FileInfo& fi : infos)
	{
		if (fi.isDir)
		{
			writer.addDirectory(fi.filePath);
		}
		else
		{
			QByteArray content = (fi.filePath == "word/document.xml")
				? newDocXml : reader.fileData(fi.filePath);
			writer.addFile(fi.filePath, content);
		}
	}
	writer.close();
	reader.close();

	return true;
}

// std::string 重载
bool CWriteReports::FillDocxTemplate(
	const std::string& strTemplatePath,
	const std::string& strOutputPath,
	const QHash<QString, QString>& mapData)
{
	return FillDocxTemplate(
		QString::fromStdString(strTemplatePath),
		QString::fromStdString(strOutputPath),
		mapData);
}
