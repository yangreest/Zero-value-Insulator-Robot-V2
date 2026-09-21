#pragma once

#include <string>
#include <functional>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QJsonObject>

// ===== 环境配置 =====
// qmake:  QT += core
//         加入 Qt 私有头文件:  QT += core-private
//         （若仍找不到头文件:  INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtCore/$$[QT_VERSION]/QtCore/private）
// CMake:  find_package(Qt6 REQUIRED COMPONENTS Core)
//         target_link_libraries(app PRIVATE Qt6::CorePrivate)
// 跨平台，无需安装 Word。
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>

// docx 报告生成类。
// 技术路线：docx 本质是 zip 容器，正文存放在 word/document.xml 中。
// 基于模板 docx，将 word/document.xml 中的 ${xxx} 占位符替换为实际数据，
// 其余部件（样式、图片、页眉页脚等）原样复制，生成填充后的 docx 报告。
class CWriteReports
{
public:
	// 填充 docx 模板，生成报告
	// strTemplatePath: 模板 docx 路径
	// strOutputPath:   输出 docx 路径（输出目录不存在时自动创建）
	// mapData:         占位符键值对，key 为占位符名（模板中写作 ${key}）
	// 返回值: 成功返回 true，失败返回 false
	static bool FillDocxTemplate(
		const QString& strTemplatePath,
		const QString& strOutputPath,
		const QHash<QString, QString>& mapData);

	// std::string 重载，便于直接对接项目配置中的 std::string 字段
	static bool FillDocxTemplate(
		const std::string& strTemplatePath,
		const std::string& strOutputPath,
		const QHash<QString, QString>& mapData);

	// 用双联测量数据填充报告"数据表"(document.xml 中第一张表)的片数数据行。
	// 数据行按实际测量片数自适应：多于模板行则增行，少于则减行，无数据则清空数据行。
	// mapTicketMearData 结构: { 侧别: { 相别: [内1,外1,内2,外2,...] } }
	//   侧别: key 含"大号"→表左半(大号侧)，含"小号"→表右半(小号侧)
	//   相别: key 含 'A'/'B'/'C' → 对应相列组
	//   每相数组按[内侧,外侧]成对存放，第 n 片: 内侧(左列)=[2(n-1)]，外侧(右列)=[2(n-1)+1]
	//   数据行数 = 六个相列数组的最大片数 = max(ceil(size/2))
	// 数据表每数据行 14 单元格: [片号,大号A内,大号A外,大号B内,大号B外,大号C内,大号C外,
	//                            片号,小号A内,小号A外,小号B内,小号B外,小号C内,小号C外]
	static bool FillMearDataReport(
		const QString& strTemplatePath,
		const QString& strOutputPath,
		const QJsonObject& mapTicketMearData);

	// std::string 重载
	static bool FillMearDataReport(
		const std::string& strTemplatePath,
		const std::string& strOutputPath,
		const QJsonObject& mapTicketMearData);

	// ===== HTML 富文本报告（Qt QTextDocument 子集，弹窗预览与导出 PDF 通用）=====

	// 填充 HTML 模板：将 ${key} 占位符替换为实际数据（值做 HTML 转义）
	static QString FillHtmlTemplate(
		const QString& strTemplate,
		const QHash<QString, QString>& mapData);

	// 生成测量数据表 HTML。mapTicketMearData 结构: { 侧别: { 相别: [值...] } }
	// bDouble: 双联时每相数组按[内侧,外侧]成对存放，拆成内/外两列；单联每相一列
	// 行数 = 各相数组最大片数（双联 ceil(size/2)），无数据时输出"暂无测量数据"占位行
	static QString BuildMearTableHtml(const QJsonObject& mapTicketMearData, bool bDouble);

	// 将 HTML 富文本导出为 PDF（QTextDocument + QPdfWriter，A4，输出目录自动创建）
	static bool ExportHtmlToPdf(const QString& strHtml, const QString& strPdfPath);

private:
	// HTML 文本转义（数据含 < > & 时预览与 PDF 才能正常显示）
	static QString HtmlEscape(const QString& strText);
	// XML 文本转义（数据含 < > & 时 Word 才能正常显示）
	static QString XmlEscape(const QString& strText);

	// 合并被 Word 拆开的 run，使 ${xxx} 变成连续文本
	static QString MergeAdjacentRuns(const QString& strXml);

	// 替换 word/document.xml 中的全部占位符
	static QString FillPlaceholders(const QString& strXml, const QHash<QString, QString>& mapData);

	// 读取模板 docx，对 word/document.xml 应用 fnTransform，其余部件原样复制后写出到 strOutputPath
	static bool RewriteDocx(
		const QString& strTemplatePath,
		const QString& strOutputPath,
		const std::function<QString(const QString&)>& fnTransform);

	// 在 document.xml 第一张表中，用测量数据重建片数数据行（行数自适应）
	static QString FillMearDataRows(const QString& strXml, const QJsonObject& mapData);

	// 以 strRowTemplate 为样板生成一行数据行，cellValues 提供 14 个单元格文本
	// （克隆时剥离 w14:paraId/textId，避免多行 ID 重复导致 Word 报"内容有问题"）
	static QString BuildDataRow(const QString& strRowTemplate, const QStringList& cellValues);

	// 将单元格 XML 内文本替换为 strValue（写入第一个 <w:t>，其余 <w:t> 清空）
	static QString SetCellText(const QString& strCellXml, const QString& strValue);
};
