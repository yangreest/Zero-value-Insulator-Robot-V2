// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef MODELDATAWIDGET_H
#define MODELDATAWIDGET_H

#include "contentwidget.h"

#include <QStringList>
#include <QVector>

QT_FORWARD_DECLARE_CLASS(QChart)
QT_FORWARD_DECLARE_CLASS(QLineSeries)
QT_FORWARD_DECLARE_CLASS(QValueAxis)
QT_FORWARD_DECLARE_CLASS(QTableView)
QT_FORWARD_DECLARE_CLASS(QSplitter)
QT_FORWARD_DECLARE_CLASS(QResizeEvent)
class ModelDataModel;

class ModelDataWidget : public ContentWidget
{
    Q_OBJECT

public:
    ModelDataWidget(QWidget *parent = nullptr);

    // 按comboBox的item建列（表头即item内容），按片数建行，并清空已有数据与曲线
    void setTableLayout(const QStringList &headers, int rowCount);
    // 每获得一个测量值，填充到对应表头列的下一个空单元格，并绘制曲线点
    void appendValue(const QString &header, double value);
    // 重测：删除对应表头列最近一个测量值及其曲线点
    void removeLastValue(const QString &header);

private:
    void applyTableWidth();
    void resizeEvent(QResizeEvent *event) override;

    ModelDataModel *m_model = nullptr;
    QTableView *m_tableView = nullptr;
    QSplitter *m_splitter = nullptr;
    int m_nTableIdealWidth = 0;
    QChart *m_chart = nullptr;
    QVector<QLineSeries *> m_series;
    QValueAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;
    double m_yMin = 0;
    double m_yMax = 0;
    bool m_hasValue = false;
};

#endif
