#include "WriteReports.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>

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

// 读取模板 docx，对 word/document.xml 应用 fnTransform，其余部件原样复制后写出
bool CWriteReports::RewriteDocx(
	const QString& strTemplatePath,
	const QString& strOutputPath,
	const std::function<QString(const QString&)>& fnTransform)
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
	QByteArray newDocXml = fnTransform(QString::fromUtf8(docXml)).toUtf8();

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

// 填充 docx 模板，生成报告
bool CWriteReports::FillDocxTemplate(
	const QString& strTemplatePath,
	const QString& strOutputPath,
	const QHash<QString, QString>& mapData)
{
	return RewriteDocx(strTemplatePath, strOutputPath,
		[&mapData](const QString& strXml) { return FillPlaceholders(strXml, mapData); });
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

// ===== 测量数据填充数据表 =====

// 用双联测量数据填充报告"数据表"的片数数据行（行数自适应）
bool CWriteReports::FillMearDataReport(
	const QString& strTemplatePath,
	const QString& strOutputPath,
	const QJsonObject& mapTicketMearData)
{
	return RewriteDocx(strTemplatePath, strOutputPath,
		[&mapTicketMearData](const QString& strXml) { return FillMearDataRows(strXml, mapTicketMearData); });
}

// std::string 重载
bool CWriteReports::FillMearDataReport(
	const std::string& strTemplatePath,
	const std::string& strOutputPath,
	const QJsonObject& mapTicketMearData)
{
	return FillMearDataReport(
		QString::fromStdString(strTemplatePath),
		QString::fromStdString(strOutputPath),
		mapTicketMearData);
}

// 将单元格 XML 内文本替换为 strValue（写入第一个 <w:t>，其余 <w:t> 清空）
QString CWriteReports::SetCellText(const QString& strCellXml, const QString& strValue)
{
	static const QRegularExpression reT("<w:t(?: [^>]*)?>(.*?)</w:t>",
		QRegularExpression::DotMatchesEverythingOption);
	QString strResult;
	int nLast = 0;
	bool bFirst = true;
	QRegularExpressionMatchIterator it = reT.globalMatch(strCellXml);
	while (it.hasNext())
	{
		QRegularExpressionMatch m = it.next();
		strResult += strCellXml.mid(nLast, m.capturedStart() - nLast);
		const int nOpenLen = m.capturedStart(1) - m.capturedStart(0);
		strResult += m.captured(0).left(nOpenLen);
		strResult += bFirst ? XmlEscape(strValue) : QString();
		strResult += "</w:t>";
		nLast = m.capturedEnd();
		bFirst = false;
	}
	strResult += strCellXml.mid(nLast);
	return strResult;
}

// 以 strRowTemplate 为样板生成一行数据行，cellValues 提供 14 个单元格文本
QString CWriteReports::BuildDataRow(const QString& strRowTemplate, const QStringList& cellValues)
{
	static const QRegularExpression reId("\\s+w14:(?:paraId|textId)=\"[^\"]*\"");
	static const QRegularExpression reTc("<w:tc>.*?</w:tc>",
		QRegularExpression::DotMatchesEverythingOption);

	QString strRow = strRowTemplate;
	strRow.remove(reId); // 剥离 paraId/textId，避免多行 ID 重复导致 Word 报"内容有问题"

	QString strResult;
	int nLast = 0;
	int nCell = 0;
	QRegularExpressionMatchIterator it = reTc.globalMatch(strRow);
	while (it.hasNext())
	{
		QRegularExpressionMatch m = it.next();
		strResult += strRow.mid(nLast, m.capturedStart() - nLast);
		QString strCell = m.captured(0);
		if (nCell < cellValues.size())
			strCell = SetCellText(strCell, cellValues.at(nCell));
		strResult += strCell;
		nLast = m.capturedEnd();
		++nCell;
	}
	strResult += strRow.mid(nLast);
	return strResult;
}

// 在 document.xml 第一张表中，用测量数据重建片数数据行（行数自适应）
QString CWriteReports::FillMearDataRows(const QString& strXml, const QJsonObject& mapData)
{
	// 1. 解析测量数据，取出六列数组：大号侧 A/B/C、小号侧 A/B/C
	auto fnFindKey = [](const QJsonObject& obj, const QString& strContain) -> QString {
		for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
			if (it.key().contains(strContain))
				return it.key();
		return QString();
		};
	auto fnPhaseArray = [](const QJsonObject& objSide, QChar chPhase) -> QJsonArray {
		for (auto it = objSide.constBegin(); it != objSide.constEnd(); ++it)
			if (it.key().contains(chPhase))
				return it.value().toArray();
		return QJsonArray();
		};

	const QJsonObject objBig = mapData.value(fnFindKey(mapData, QStringLiteral("大号"))).toObject();
	const QJsonObject objSmall = mapData.value(fnFindKey(mapData, QStringLiteral("小号"))).toObject();

	// 列顺序：大号A、大号B、大号C、小号A、小号B、小号C（不可 static，否则跨调用复用首次数据）
	const QJsonArray arrCols[6] = {
		fnPhaseArray(objBig, QLatin1Char('A')), fnPhaseArray(objBig, QLatin1Char('B')), fnPhaseArray(objBig, QLatin1Char('C')),
		fnPhaseArray(objSmall, QLatin1Char('A')), fnPhaseArray(objSmall, QLatin1Char('B')), fnPhaseArray(objSmall, QLatin1Char('C'))
	};

	// 2. 行数 N = 各列最大片数（双联每片 2 值，片数 = ceil(size/2)）
	int nRows = 0;
	for (int i = 0; i < 6; ++i)
		nRows = qMax(nRows, static_cast<int>((arrCols[i].size() + 1) / 2));

	// 3. 定位第一张表
	static const QRegularExpression reTbl("<w:tbl>.*?</w:tbl>", QRegularExpression::DotMatchesEverythingOption);
	const QRegularExpressionMatch mTbl = reTbl.match(strXml);
	if (!mTbl.hasMatch())
	{
		qWarning() << "未找到数据表(<w:tbl>)，跳过测量数据填充";
		return strXml;
	}
	const QString strTbl = mTbl.captured(0);

	// 4. 找出数据行（14 单元格且首格为纯数字片号）范围，以首个数据行为样板
	static const QRegularExpression reTr("<w:tr(?:\\s[^>]*)?>.*?</w:tr>", QRegularExpression::DotMatchesEverythingOption);
	static const QRegularExpression reTc("<w:tc>.*?</w:tc>", QRegularExpression::DotMatchesEverythingOption);
	static const QRegularExpression reT("<w:t(?: [^>]*)?>(.*?)</w:t>", QRegularExpression::DotMatchesEverythingOption);
	static const QRegularExpression reNum("^\\d+$");

	int nDataStart = -1;
	int nDataEnd = -1;
	QString strRowTemplate;
	QRegularExpressionMatchIterator itTr = reTr.globalMatch(strTbl);
	while (itTr.hasNext())
	{
		const QRegularExpressionMatch mTr = itTr.next();
		const QString strRow = mTr.captured(0);

		int nCell = 0;
		QString strFirstText;
		QRegularExpressionMatchIterator itTc = reTc.globalMatch(strRow);
		while (itTc.hasNext())
		{
			const QRegularExpressionMatch mTc = itTc.next();
			if (nCell == 0)
			{
				const QRegularExpressionMatch mT = reT.match(mTc.captured(0));
				strFirstText = mT.hasMatch() ? mT.captured(1) : QString();
			}
			++nCell;
		}

		if (nCell == 14 && reNum.match(strFirstText).hasMatch())
		{
			if (nDataStart < 0)
			{
				nDataStart = mTr.capturedStart();
				strRowTemplate = strRow;
			}
			nDataEnd = mTr.capturedEnd();
		}
	}

	if (nDataStart < 0 || strRowTemplate.isEmpty())
	{
		qWarning() << "未找到可参照的数据行(14 列且首列为片号)，跳过测量数据填充";
		return strXml;
	}
	if (nRows <= 0)
		qWarning() << "测量数据为空，数据表将不含任何片号数据行";

	// 5. 以样板行生成 N 行数据
	static const int nColOfCell[14] = { -1, 0, 0, 1, 1, 2, 2, -1, 3, 3, 4, 4, 5, 5 };
	static const bool bOutsideOfCell[14] = { false, false, true, false, true, false, true, false, false, true, false, true, false, true };

	QString strNewRows;
	for (int n = 1; n <= nRows; ++n)
	{
		QStringList cellValues;
		for (int c = 0; c < 14; ++c)
		{
			if (nColOfCell[c] < 0)
			{
				cellValues << QString::number(n);   // 片号列
				continue;
			}
			const QJsonArray& arr = arrCols[nColOfCell[c]];
			const int idx = 2 * (n - 1) + (bOutsideOfCell[c] ? 1 : 0);
			cellValues << (idx < arr.size() ? QString::number(arr[idx].toDouble()) : QString());
		}
		strNewRows += BuildDataRow(strRowTemplate, cellValues);
	}

	// 6. 用新数据行替换原数据行范围，回填 document.xml
	const QString strNewTbl = strTbl.left(nDataStart) + strNewRows + strTbl.mid(nDataEnd);
	return strXml.left(mTbl.capturedStart()) + strNewTbl + strXml.mid(mTbl.capturedEnd());
}
