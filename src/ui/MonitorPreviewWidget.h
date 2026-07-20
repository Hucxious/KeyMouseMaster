#ifndef MONITORPREVIEWWIDGET_H
#define MONITORPREVIEWWIDGET_H

#include <QWidget>
#include <QVector>
#include "MonitorInfo.h"

// ============================================================================
// 显示器预览控件
// 以可视化方式显示多显示器布局，高亮当前选中的显示器
// ============================================================================
class MonitorPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorPreviewWidget(QWidget* parent = nullptr);

    void setMonitors(const QVector<MonitorInfo>& monitors);
    void setHighlightedMonitor(const QString& deviceName);
    void setCurrentCursorPos(const QPoint& virtualPt);

    QSize minimumSizeHint() const override { return QSize(200, 120); }
    QSize sizeHint() const override { return QSize(400, 200); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<MonitorInfo> m_monitors;
    QString m_highlightedDevice;
    QPoint m_cursorPos;
    QRect m_virtualBounds;

    // 将虚拟桌面坐标映射到控件绘制坐标
    QPointF mapVirtualToWidget(const QPoint& virtualPt, const QSize& widgetSize) const;
};

#endif // MONITORPREVIEWWIDGET_H
