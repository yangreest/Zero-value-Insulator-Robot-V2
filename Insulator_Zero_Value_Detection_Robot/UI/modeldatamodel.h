// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef MODELDATAMODEL_H
#define MODELDATAMODEL_H

#include <QAbstractTableModel>
#include <QMultiHash>
#include <QRect>
#include <QStringList>

class ModelDataModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ModelDataModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    Qt::ItemFlags flags(const QModelIndex &index) const;

    // 根据表头（列数）与行数重建表格，未测量单元格为空（NaN）
    void setTableLayout(const QStringList &headers, int rowCount);
    // 根据表头内容查找列序号，未找到返回-1
    int columnIndex(const QString &header) const;

    void addMapping(const QString &color, const QRect &area);
    void clearMapping() { m_mapping.clear(); }

private:
    QList<QList<qreal> *> m_data;
    QMultiHash<QString, QRect> m_mapping;
    QStringList m_headers;
    int m_columnCount;
    int m_rowCount;
};

#endif
