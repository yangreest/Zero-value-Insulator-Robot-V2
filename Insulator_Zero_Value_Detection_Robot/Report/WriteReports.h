#pragma once

#include <string>
#include <QHash>
#include <QString>

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

private:
	// XML 文本转义（数据含 < > & 时 Word 才能正常显示）
	static QString XmlEscape(const QString& strText);

	// 合并被 Word 拆开的 run，使 ${xxx} 变成连续文本
	static QString MergeAdjacentRuns(const QString& strXml);

	// 替换 word/document.xml 中的全部占位符
	static QString FillPlaceholders(const QString& strXml, const QHash<QString, QString>& mapData);
};
