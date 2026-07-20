#include "CoordinateMapper.h"
#include <QtMath>
#include <QDebug>

CoordinateMapper::CoordinateMapper(QObject* parent)
    : QObject(parent)
{
}

// ============================================================================
// 虚拟桌面坐标 -> Windows SendInput 绝对坐标 (0~65535)
// 考虑虚拟桌面左上角不为(0,0)的情况和负坐标
// ============================================================================
void CoordinateMapper::virtualToWindowsAbsolute(const QPoint& virtualPt,
                                                  const QRect& virtualDesktopBounds,
                                                  int& outAbsX, int& outAbsY)
{
    int virtW = virtualDesktopBounds.width();
    int virtH = virtualDesktopBounds.height();

    // 保护: 宽高不能为0或1，否则除零
    if (virtW <= 1) virtW = 2;
    if (virtH <= 1) virtH = 2;

    int virtLeft = virtualDesktopBounds.left();
    int virtTop  = virtualDesktopBounds.top();

    // 映射公式: normalized = (pt - virtOrigin) * 65535 / (virtSize - 1)
    // 使用 int64_t 防止溢出
    int64_t normX = (static_cast<int64_t>(virtualPt.x()) - virtLeft) * 65535 / (virtW - 1);
    int64_t normY = (static_cast<int64_t>(virtualPt.y()) - virtTop)  * 65535 / (virtH - 1);

    // 裁剪到 [0, 65535]
    outAbsX = static_cast<int>(qBound<int64_t>(INT64_C(0), normX, INT64_C(65535)));
    outAbsY = static_cast<int>(qBound<int64_t>(INT64_C(0), normY, INT64_C(65535)));
}

// ============================================================================
// 虚拟桌面 <-> 显示器内部
// ============================================================================
QPoint CoordinateMapper::virtualToMonitorInternal(const QPoint& virtualPt,
                                                    const MonitorInfo& monitor)
{
    return QPoint(virtualPt.x() - monitor.desktopRect.x(),
                  virtualPt.y() - monitor.desktopRect.y());
}

QPoint CoordinateMapper::monitorInternalToVirtual(const QPoint& internalPt,
                                                    const MonitorInfo& monitor)
{
    return QPoint(internalPt.x() + monitor.desktopRect.x(),
                  internalPt.y() + monitor.desktopRect.y());
}

// ============================================================================
// 显示器内部 <-> 比例坐标 [0,1]
// ============================================================================
QPointF CoordinateMapper::internalToRatio(const QPoint& internalPt,
                                            const MonitorInfo& monitor)
{
    int w = monitor.desktopRect.width();
    int h = monitor.desktopRect.height();
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    return QPointF(static_cast<qreal>(internalPt.x()) / w,
                   static_cast<qreal>(internalPt.y()) / h);
}

QPoint CoordinateMapper::ratioToInternal(const QPointF& ratioPt,
                                           const MonitorInfo& monitor)
{
    int w = monitor.desktopRect.width();
    int h = monitor.desktopRect.height();

    return QPoint(qRound(ratioPt.x() * w),
                  qRound(ratioPt.y() * h));
}

// ============================================================================
// 虚拟桌面 <-> 比例坐标
// ============================================================================
QPointF CoordinateMapper::virtualToRatio(const QPoint& virtualPt,
                                           const MonitorInfo& monitor)
{
    QPoint internal = virtualToMonitorInternal(virtualPt, monitor);
    return internalToRatio(internal, monitor);
}

QPoint CoordinateMapper::ratioToVirtual(const QPointF& ratioPt,
                                          const MonitorInfo& monitor)
{
    QPoint internal = ratioToInternal(ratioPt, monitor);
    return monitorInternalToVirtual(internal, monitor);
}

// ============================================================================
// 根据坐标模式解析出目标虚拟桌面坐标
// ============================================================================
bool CoordinateMapper::resolveTargetCoordinate(
    CoordinateMode mode,
    const QPoint& virtualDesktopAbs,
    const QString& monitorDeviceName,
    const QPoint& monitorInternal,
    const QPointF& monitorRatio,
    const QVector<MonitorInfo>& monitors,
    QPoint& outVirtualPt,
    QString* errorMsg)
{
    switch (mode) {
    case CoordinateMode::CurrentCursor:
        // 此模式不在预处理中解析，由调用方读取当前光标位置
        outVirtualPt = QPoint();
        return true;

    case CoordinateMode::VirtualDesktopAbsolute:
        outVirtualPt = virtualDesktopAbs;
        return true;

    case CoordinateMode::MonitorRelative: {
        const MonitorInfo* monitor = findMonitor(monitorDeviceName, monitors);
        if (!monitor) {
            if (errorMsg)
                *errorMsg = QString("找不到目标显示器: %1").arg(monitorDeviceName);
            return false;
        }
        outVirtualPt = monitor->internalToVirtual(monitorInternal);
        return true;
    }

    case CoordinateMode::MonitorRatio: {
        const MonitorInfo* monitor = findMonitor(monitorDeviceName, monitors);
        if (!monitor) {
            if (errorMsg)
                *errorMsg = QString("找不到目标显示器: %1").arg(monitorDeviceName);
            return false;
        }
        outVirtualPt = ratioToVirtual(monitorRatio, *monitor);
        return true;
    }
    }

    return false;
}

// ============================================================================
// 工具方法
// ============================================================================
bool CoordinateMapper::isInVirtualDesktop(const QPoint& virtualPt,
                                            const QRect& virtualDesktopBounds)
{
    return virtualDesktopBounds.contains(virtualPt);
}

QPoint CoordinateMapper::clampToVirtualDesktop(const QPoint& virtualPt,
                                                 const QRect& virtualDesktopBounds)
{
    int x = qBound(virtualDesktopBounds.left(), virtualPt.x(),
                   virtualDesktopBounds.right());
    int y = qBound(virtualDesktopBounds.top(), virtualPt.y(),
                   virtualDesktopBounds.bottom());
    return QPoint(x, y);
}

const MonitorInfo* CoordinateMapper::findMonitor(const QString& deviceName,
                                                   const QVector<MonitorInfo>& monitors)
{
    for (const auto& m : monitors) {
        if (m.deviceName == deviceName)
            return &m;
    }
    return nullptr;
}
