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
    qDeleteAll(m_alarms);
    m_alarms.clear();

    m_headers = headers;
    m_columnCount = headers.size();
    m_rowCount = rowCount;

    // 初始化为空单元格（NaN），测量值到达后逐个填充
    const qreal empty = std::numeric_limits<qreal>::quiet_NaN();
    for (int i = 0; i < m_rowCount; i++) {
        m_data.append(new QList<qreal>(m_columnCount, empty));
        m_alarms.append(new QList<QString>(m_columnCount));
    }

    endResetModel();
}

void ModelDataModel::setCellAlarm(int row, int col, const QString &alarm)
{
    if (row < 0 || row >= m_rowCount || col < 0 || col >= m_columnCount)
        return;
    m_alarms[row]->replace(col, alarm);
    const QModelIndex idx = index(row, col);
    emit dataChanged(idx, idx);
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
        if (section >= 0 && section < m_headers.size()) {
            // 显示时把分隔空格换成换行，使表头两行显示，避免整串过长把列撑宽；
            // m_headers保留原串，columnIndex查找不受影响
            QString strHeader = m_headers.at(section);
            return strHeader.replace(QLatin1Char(' '), QLatin1Char('\n'));
        }
        return QVariant();
    } else {
        return QString("%1").arg(section + 1);
    }
}

QVariant ModelDataModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const QString strAlarm = m_alarms[index.row()]->at(index.column());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        qreal value = m_data[index.row()]->at(index.column());
        // 尚未测量的单元格:有告警则显示告警文本（探针/测量超时等），否则显示为空
        if (std::isnan(value))
            return strAlarm.isEmpty() ? QVariant() : QVariant(strAlarm);
        return value;
    } else if (role == Qt::ForegroundRole) {
        // 有告警的单元格标红，与界面告警红色(#f56c6c)一致
        if (!strAlarm.isEmpty())
            return QColor(0xf5, 0x6c, 0x6c);
    } else if (role == Qt::ToolTipRole) {
        // 悬停显示告警详情
        if (!strAlarm.isEmpty())
            return strAlarm;
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
