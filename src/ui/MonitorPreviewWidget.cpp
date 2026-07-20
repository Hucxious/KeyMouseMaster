#include "MonitorPreviewWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QtMath>

MonitorPreviewWidget::MonitorPreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 120);
}

void MonitorPreviewWidget::setMonitors(const QVector<MonitorInfo>& monitors)
{
    m_monitors = monitors;

    // 计算虚拟桌面范围
    if (monitors.isEmpty()) {
        m_virtualBounds = QRect();
    } else {
        int left = 0, top = 0, right = 0, bottom = 0;
        for (const auto& m : monitors) {
            left = qMin(left, m.desktopRect.left());
            top = qMin(top, m.desktopRect.top());
            right = qMax(right, m.desktopRect.right());
            bottom = qMax(bottom, m.desktopRect.bottom());
        }
        m_virtualBounds = QRect(left, top, right - left, bottom - top);
    }

    update();
}

void MonitorPreviewWidget::setHighlightedMonitor(const QString& deviceName)
{
    m_highlightedDevice = deviceName;
    update();
}

void MonitorPreviewWidget::setCurrentCursorPos(const QPoint& virtualPt)
{
    m_cursorPos = virtualPt;
    update();
}

void MonitorPreviewWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 背景
    painter.fillRect(rect(), QColor(45, 45, 48));

    if (m_monitors.isEmpty()) {
        painter.setPen(QColor(160, 160, 160));
        painter.drawText(rect(), Qt::AlignCenter, "未检测到显示器");
        return;
    }

    QSize widgetSize = size();

    // 为每个显示器绘制矩形
    for (const auto& monitor : m_monitors) {
        QPointF topLeft = mapVirtualToWidget(monitor.desktopRect.topLeft(), widgetSize);
        QPointF bottomRight = mapVirtualToWidget(monitor.desktopRect.bottomRight(), widgetSize);

        QRectF monitorRect(topLeft, bottomRight);
        monitorRect = monitorRect.normalized();

        // 确保最小可见尺寸
        if (monitorRect.width() < 20) monitorRect.setWidth(20);
        if (monitorRect.height() < 20) monitorRect.setHeight(20);

        // 是否为高亮显示器
        bool isHighlighted = (monitor.deviceName == m_highlightedDevice);

        // 填充
        QColor fillColor = isHighlighted ? QColor(33, 150, 243, 100) : QColor(60, 60, 65, 100);
        painter.fillRect(monitorRect, fillColor);

        // 边框
        QPen pen(isHighlighted ? QColor(33, 150, 243) : QColor(100, 100, 105), 2);
        painter.setPen(pen);
        painter.drawRect(monitorRect);

        // 显示器名称
        painter.setPen(QColor(220, 220, 220));
        QFont font = painter.font();
        font.setPointSize(8);
        painter.setFont(font);

        QString label;
        if (monitor.isPrimary)
            label = QString("屏%1 (主)").arg(monitor.index);
        else
            label = QString("屏%1").arg(monitor.index);

        painter.drawText(monitorRect.adjusted(4, 4, -4, -4),
                         Qt::AlignLeft | Qt::AlignTop, label);

        // 分辨率
        QString resText = QString("%1x%2")
            .arg(monitor.desktopRect.width())
            .arg(monitor.desktopRect.height());
        painter.drawText(monitorRect.adjusted(4, 4, -4, -4),
                         Qt::AlignRight | Qt::AlignBottom, resText);

        // 缩放
        if (monitor.scaleFactor != 1.0) {
            QString scaleText = QString("@%1%").arg(qRound(monitor.scaleFactor * 100));
            painter.drawText(monitorRect.adjusted(4, 4, -4, -4),
                             Qt::AlignLeft | Qt::AlignBottom, scaleText);
        }
    }
}

QPointF MonitorPreviewWidget::mapVirtualToWidget(const QPoint& virtualPt,
                                                    const QSize& widgetSize) const
{
    if (m_virtualBounds.isEmpty())
        return QPointF(0, 0);

    // 留边距
    const int margin = 15;
    int drawW = widgetSize.width() - margin * 2;
    int drawH = widgetSize.height() - margin * 2;

    double scaleX = static_cast<double>(drawW) / m_virtualBounds.width();
    double scaleY = static_cast<double>(drawH) / m_virtualBounds.height();
    double scale = qMin(scaleX, scaleY);

    // 居中
    double offsetX = (widgetSize.width() - m_virtualBounds.width() * scale) / 2.0;
    double offsetY = (widgetSize.height() - m_virtualBounds.height() * scale) / 2.0;

    double x = (virtualPt.x() - m_virtualBounds.left()) * scale + offsetX;
    double y = (virtualPt.y() - m_virtualBounds.top()) * scale + offsetY;

    return QPointF(x, y);
}
