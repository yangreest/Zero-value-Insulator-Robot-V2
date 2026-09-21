// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "modeldatawidget.h"
#include "modeldatamodel.h"

#include <QChart>
#include <QChartView>
#include <QFontMetrics>
#include <QGraphicsLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineSeries>
#include <QSplitter>
#include <QTableView>
#include <QTimer>
#include <QValueAxis>

#include <cmath>
#include <limits>

QT_USE_NAMESPACE

ModelDataWidget::ModelDataWidget(QWidget *parent)
    : ContentWidget(parent)
{
    // create simple model for storing user's data
    m_model = new ModelDataModel(this);

    // create table view and add model to it
    m_tableView = new QTableView;
    m_tableView->setModel(m_model);
    // 列宽均匀拉伸：所有列等宽铺满表格，表格整体宽度仍由表头内容决定（见setTableLayout）
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    auto chart = new QChart;
    chart->setAnimationOptions(QChart::AllAnimations); // enable animations
    m_chart = chart;

    // 横坐标为行数（片号），纵坐标为测量值
    m_axisX = new QValueAxis;
    m_axisX->setTitleText(QStringLiteral("片号"));
    m_axisX->setLabelFormat("%d");
    m_axisX->setRange(1, 1);
    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY = new QValueAxis;
    m_axisY->setTitleText(QStringLiteral("测量值"));
    m_axisY->setRange(0, 1);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    chart->layout()->setContentsMargins(0, 0, 0, 0);
    auto chartView = new QChartView(chart, this);
    chartView->setRenderHint(QPainter::Antialiasing);
    // 曲线图最小宽度：列太多表格占满时，曲线图不再被压缩到很窄
    chartView->setMinimumWidth(m_nChartMinWidth);

    // 用splitter控制表格与曲线图的宽度，曲线图占剩余空间，可拖动调节
    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->addWidget(m_tableView);
    m_splitter->addWidget(chartView);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    // create main layout
    auto mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_splitter);
    setLayout(mainLayout);
}

void ModelDataWidget::setTableLayout(const QStringList &headers, int rowCount)
{
    // 移除旧曲线
    const auto oldSeries = m_chart->series();
    for (QAbstractSeries *series : oldSeries) {
        m_chart->removeSeries(series);
        delete series;
    }
    m_series.clear();
    m_model->clearMapping();
    m_hasValue = false;

    // 列数由comboBox的item决定（表头与item内容一致），行数由片数决定
    m_model->setTableLayout(headers, rowCount);

    // 每一列绘制一组曲线
    for (int col = 0; col < headers.size(); col++) {
        auto series = new QLineSeries;
        series->setName(headers.at(col));
        m_chart->addSeries(series);
        series->attachAxis(m_axisX);
        series->attachAxis(m_axisY);
        m_series.append(series);

        // get the color of the series and use it for showing the mapped area
        QString seriesColorHex = "#" + QString::number(series->pen().color().rgb(), 16).right(6).toUpper();
        m_model->addMapping(seriesColorHex, QRect(col, 0, 1, rowCount));
    }

    // 坐标轴范围：横轴为片号1~片数，纵轴先用默认范围，随测量值动态扩展
    if (rowCount > 0)
        m_axisX->setRange(1, rowCount);
    m_axisY->setRange(0, 1);

    // 表格宽度根据表头长度自由调节：表头两行显示，列宽只需容纳最宽的一行；
    // 列均匀拉伸下每列等宽，因此表格宽度取最宽表头×列数，保证最宽表头不被截断
    const QFontMetrics fm(m_tableView->horizontalHeader()->font());
    int nVerticalHeaderWidth = m_tableView->verticalHeader()->width();
    if (nVerticalHeaderWidth <= 0)
        nVerticalHeaderWidth = m_tableView->verticalHeader()->defaultSectionSize();
    int nMaxLineWidth = 0;
    for (const QString &strHeader : headers) {
        // 表头按空格拆成多行，列宽取最宽的一行（而非整串），避免单元格过宽
        const QStringList lines = strHeader.split(QLatin1Char(' '));
        for (const QString &strLine : lines)
            nMaxLineWidth = qMax(nMaxLineWidth, fm.horizontalAdvance(strLine));
    }
    const int nColWidth = nMaxLineWidth + 20; // 文字两侧留白
    int nTableWidth = nVerticalHeaderWidth + m_tableView->frameWidth() * 2
        + nColWidth * headers.size();
    // 表头两行需要更高的表头高度（否则第二行会被截断）
    m_tableView->horizontalHeader()->setMinimumHeight(fm.height() * 2 + 8);
    m_tableView->setMinimumWidth(nTableWidth);
    m_nTableIdealWidth = nTableWidth;
    // 表格取表头所需宽度，曲线图占剩余空间（总宽精确分配，避免setSizes按比例缩放）
    applyTableWidth();
}

void ModelDataWidget::applyTableWidth()
{
    if (m_nTableIdealWidth <= 0 || m_splitter->width() <= 0)
        return;
    // 曲线图保底最小宽度：列太多时表格取完所需宽度后，曲线图保持m_nChartMinWidth；
    // 总宽超出视口的部分由外层QScrollArea的横向滚动条承载
    int nChartWidth = qMax(m_nChartMinWidth, m_splitter->width() - m_nTableIdealWidth);
    m_splitter->setSizes({ m_nTableIdealWidth, nChartWidth });
}

void ModelDataWidget::resizeEvent(QResizeEvent *event)
{
    ContentWidget::resizeEvent(event);
    // 延迟到事件循环执行，避免在父级splitter拖拽过程中调用setSizes引起布局冲突
    QTimer::singleShot(0, this, [this]() {
        applyTableWidth();
    });
}

void ModelDataWidget::appendValue(const QString &header, double value)
{
    int col = m_model->columnIndex(header);
    if (col < 0 || col >= m_series.size())
        return;

    // 找到该列下一个无数值的单元格（空格或告警占位格）并填充；
    // 告警占位格（超时等）填值后保留告警标红，保证测量值与行号对齐不错位
    for (int row = 0; row < m_model->rowCount(); row++) {
        QModelIndex index = m_model->index(row, col);
        if (m_model->data(index, Qt::EditRole).typeId() != QMetaType::Double) {
            m_model->setData(index, value);
            // 横坐标用行数，纵坐标为对应行的值
            m_series[col]->append(row + 1, value);

            // 动态扩展纵轴范围
            if (!m_hasValue) {
                m_yMin = m_yMax = value;
                m_hasValue = true;
            } else {
                m_yMin = qMin(m_yMin, value);
                m_yMax = qMax(m_yMax, value);
            }
            double padding = qMax((m_yMax - m_yMin) * 0.1, 1.0);
            m_axisY->setRange(m_yMin - padding, m_yMax + padding);
            return;
        }
    }
}

void ModelDataWidget::setAlarm(const QString &header, int row, const QString &alarm)
{
    int col = m_model->columnIndex(header);
    if (col < 0 || row < 0 || row >= m_model->rowCount())
        return;
    m_model->setCellAlarm(row, col, alarm);
}

void ModelDataWidget::removeLastValue(const QString &header)
{
    int col = m_model->columnIndex(header);
    if (col < 0 || col >= m_series.size())
        return;

    // 找到该列最近一个已测量或含告警的单元格并清空（值+告警同时清除）
    const double empty = std::numeric_limits<double>::quiet_NaN();
    for (int row = m_model->rowCount() - 1; row >= 0; row--) {
        QModelIndex index = m_model->index(row, col);
        const QVariant varValue = m_model->data(index, Qt::EditRole);
        if (varValue.isValid()) {
            m_model->setData(index, empty);
            m_model->setCellAlarm(row, col, QString());
            // 仅数值单元格对应一个曲线点，告警占位格（超时等）无曲线点可删
            if (varValue.typeId() == QMetaType::Double) {
                const auto points = m_series[col]->points();
                if (!points.isEmpty())
                    m_series[col]->removePoints(points.size() - 1, 1);
            }
            return;
        }
    }
}
