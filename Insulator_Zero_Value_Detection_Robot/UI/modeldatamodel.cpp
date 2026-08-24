// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "modeldatamodel.h"

#include <QColor>
#include <QList>
#include <QRect>

#include <cmath>
#include <limits>

ModelDataModel::ModelDataModel(QObject *parent) :
    QAbstractTableModel(parent)
{
    m_columnCount = 0;
    m_rowCount = 0;
}

void ModelDataModel::setTableLayout(const QStringList &headers, int rowCount)
{
    beginResetModel();

    qDeleteAll(m_data);
    m_data.clear();

    m_headers = headers;
    m_columnCount = headers.size();
    m_rowCount = rowCount;

    // 初始化为空单元格（NaN），测量值到达后逐个填充
    const qreal empty = std::numeric_limits<qreal>::quiet_NaN();
    for (int i = 0; i < m_rowCount; i++) {
        m_data.append(new QList<qreal>(m_columnCount, empty));
    }

    endResetModel();
}

int ModelDataModel::columnIndex(const QString &header) const
{
    return m_headers.indexOf(header);
}

int ModelDataModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_data.count();
}

int ModelDataModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_columnCount;
}

QVariant ModelDataModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        // 表头与comboBox的item内容一致
        if (section >= 0 && section < m_headers.size())
            return m_headers.at(section);
        return QVariant();
    } else {
        return QString("%1").arg(section + 1);
    }
}

QVariant ModelDataModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        qreal value = m_data[index.row()]->at(index.column());
        // 尚未测量的单元格显示为空
        if (std::isnan(value))
            return QVariant();
        return value;
    } else if (role == Qt::BackgroundRole) {
        for (const QRect &rect : m_mapping) {
            if (rect.contains(index.column(), index.row()))
                return QColor(m_mapping.key(rect));
        }
        // cell not mapped return white color
        return QColor(Qt::white);
    }
    return QVariant();
}

bool ModelDataModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::EditRole) {
        m_data[index.row()]->replace(index.column(), value.toDouble());
        emit dataChanged(index, index);
        return true;
    }
    return false;
}

Qt::ItemFlags ModelDataModel::flags(const QModelIndex &index) const
{
    return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
}

void ModelDataModel::addMapping(const QString &color, const QRect &area)
{
    m_mapping.insert(color, area);
}
