#pragma once
#include <QMap>
#include <QDataStream>
#include <QByteArray>
#include <QFile>
#include <QSettings>
#include <QDebug>

/// 容器 → 二进制字节数组
template<typename TContainer>
QByteArray toBinaryData(const TContainer& container)
{
    QByteArray arr;
    QDataStream stream(&arr, QIODevice::WriteOnly);
    // 固定序列化版本，防止Qt版本升级导致无法解析
    stream.setVersion(QDataStream::Qt_6_10);
    stream << container;
    return arr;
}

/// 二进制字节数组 → 容器
template<typename TContainer>
bool fromBinaryData(const QByteArray& data, TContainer& outContainer)
{
    if (data.isEmpty())
        return false;

    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_6_10);
    stream >> outContainer;
    return stream.status() == QDataStream::Ok;
}

/// 二进制字节 → Base64字符串（文本配置存储用，ini/xml只能存字符串）
inline QString binaryToBase64(const QByteArray& binData)
{
    return QString::fromLatin1(binData.toBase64());
}

/// Base64字符串 → 原始二进制字节
inline QByteArray base64ToBinary(const QString& base64Str)
{
    return QByteArray::fromBase64(base64Str.toLatin1());
}


// 保存Map到ini
//void saveMapToIniBinary(const std::map<std::string, int>& map, const QString& iniPath)
//{
//    QByteArray bin = toBinaryData(map);
//    QString base64Str = binaryToBase64(bin);
//
//    QSettings settings(iniPath, QSettings::IniFormat);
//    settings.setIniCodec("UTF-8");
//    settings.beginGroup("BinaryStorage");
//    settings.setValue("map_data", base64Str);
//    settings.endGroup();
//}

// INI读取字符串，还原QMap
//QMap<QString, int> loadMapFromIniBinary(const QString& iniPath)
//{
//    QMap<QString, int> map;
//    QSettings settings(iniPath, QSettings::IniFormat);
//    settings.setIniCodec("UTF-8");
//    settings.beginGroup("BinaryStorage");
//    QString base64Str = settings.value("map_data", "").toString();
//    settings.endGroup();
//
//    QByteArray bin = base64ToBinary(base64Str);
//    fromBinaryData(bin, map);
//    return map;
//}