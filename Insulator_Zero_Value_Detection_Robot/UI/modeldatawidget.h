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
    // 直接在对应表头列的指定行写入测量值（覆盖已有值/空位回填），并同步曲线点
    void setValueAt(const QString &header, int row, double value);
    // 清空对应表头列指定行的测量值与告警，并移除该行曲线点（删除点位）
    void clearValueAt(const QString &header, int row);
    // 读取表格当前选中的单元格：返回表头文本与行号，未选中返回false
    bool getSelectedCell(QString &strHeader, int &nRow) const;
    // 在对应表头列的指定行记录测量告警（探针超时/测量超时/数据异常等）
    void setAlarm(const QString &header, int row, const QString &alarm);
    // 重测：删除对应表头列最近一个测量值及其曲线点
    void removeLastValue(const QString &header);

private:
    void applyTableWidth();
    void resizeEvent(QResizeEvent *event) override;
    // 从所有曲线点重新计算纵轴范围（删除/覆盖后保持范围准确）
    void updateAxes();
    // 更新指定列中x坐标对应点的纵轴与曲线数据
    void updateSeriesPointAt(int col, int row, double value);

    ModelDataModel *m_model = nullptr;
    QTableView *m_tableView = nullptr;
    QSplitter *m_splitter = nullptr;
    int m_nTableIdealWidth = 0;
    // 曲线图最小宽度：列太多时表格占满后曲线图保底不被挤成一条缝
    int m_nChartMinWidth = 500;
    QChart *m_chart = nullptr;
    QVector<QLineSeries *> m_series;
    QValueAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;
    double m_yMin = 0;
    double m_yMax = 0;
    bool m_hasValue = false;
};

#endif
