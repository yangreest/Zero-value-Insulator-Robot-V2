// ===== 环境配置 =====
// qmake:  QT += core
//         加入 Qt 私有头文件:  QT += core-private
//         （若仍找不到头文件:  INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtCore/$$[QT_VERSION]/QtCore/private）
// CMake:  find_package(Qt6 REQUIRED COMPONENTS Core)
//         target_link_libraries(app PRIVATE Qt6::CorePrivate)
// 跨平台，无需安装 Word。

#include <QCoreApplication>
#include <QStringList>
#include <QHash>
#include <QRegularExpression>
#include <QDebug>
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>

// XML 文本转义（数据含 < > & 时 Word 才能正常显示）
static QString xmlEscape(const QString &s)
{
    QString r = s;
    r.replace('&', "&amp;");
    r.replace('<', "&lt;");
    r.replace('>', "&gt;");
    return r;
}

// 合并被 Word 拆开的 run，使 ${xxx} 变成连续文本
static QString mergeAdjacentRuns(const QString &xml)
{
    QString r = xml;
    QRegularExpression re1("</w:t></w:r><w:r>(?:<w:rPr>.*?</w:rPr>)?<w:t>",
                           QRegularExpression::DotMatchesEverythingOption);
    r.replace(re1, "</w:t><w:t>");   // 跨 run 合并
    r.remove("</w:t><w:t>");         // 同 run 内多个 w:t 合并
    return r;
}

static QString fillPlaceholders(const QString &xml, const QHash<QString, QString> &data)
{
    QString out = mergeAdjacentRuns(xml);
    for (auto it = data.constBegin(); it != data.constEnd(); ++it)
        out.replace("${" + it.key() + "}", xmlEscape(it.value()));
    return out;
}

// 基于模板 docx 生成填充后的 docx
bool fillDocxTemplate(const QString &templatePath, const QString &outputPath,
                      const QHash<QString, QString> &data)
{
    QZipReader reader(templatePath);
    if (!reader.exists()) { qWarning() << "模板打开失败:" << templatePath; return false; }

    const QList<QZipReader::FileInfo> infos = reader.fileInfoList();

    QByteArray docXml = reader.fileData("word/document.xml");
    if (docXml.isEmpty()) { qWarning() << "word/document.xml 为空"; return false; }
    QByteArray newDocXml = fillPlaceholders(QString::fromUtf8(docXml), data).toUtf8();

    QZipWriter writer(outputPath);
    for (const QZipReader::FileInfo &fi : infos) {
        if (fi.isDir) {
            writer.addDirectory(fi.filePath);
        } else {
            QByteArray content = (fi.filePath == "word/document.xml")
                    ? newDocXml : reader.fileData(fi.filePath);
            writer.addFile(fi.filePath, content);   // 其余部件原样保留
        }
    }
    writer.close();
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QHash<QString, QString> data;
    data["name"]  = "张三";
    data["dept"]  = "研发部 <嵌入式> & 测试";   // 含特殊字符，会自动转义
    data["phone"] = "138-0000-0000";

    if (fillDocxTemplate("template.docx", "output.docx", data))
        qDebug() << "生成成功: output.docx";
    return 0;
}
