#ifndef COORDINATEMAPPER_H
#define COORDINATEMAPPER_H

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include "AppTypes.h"
#include "MonitorInfo.h"

// ============================================================================
// 坐标映射器
// 统一管理所有坐标系统之间的转换
// 坐标系统: Qt逻辑坐标、Windows物理坐标、虚拟桌面坐标、显示器内部坐标、比例坐标
// ============================================================================
class CoordinateMapper : public QObject
{
    Q_OBJECT

public:
    explicit CoordinateMapper(QObject* parent = nullptr);

    // ---- 虚拟桌面 <-> Windows绝对坐标 (0~65535) ----
    // 将虚拟桌面坐标映射到Windows SendInput绝对坐标
    static void virtualToWindowsAbsolute(const QPoint& virtualPt,
                                          const QRect& virtualDesktopBounds,
                                          int& outAbsX, int& outAbsY);

    // ---- 虚拟桌面 <-> 显示器内部 ----
    static QPoint virtualToMonitorInternal(const QPoint& virtualPt,
                                            const MonitorInfo& monitor);
    static QPoint monitorInternalToVirtual(const QPoint& internalPt,
                                            const MonitorInfo& monitor);

    // ---- 显示器内部 <-> 比例坐标 [0,1] ----
    static QPointF internalToRatio(const QPoint& internalPt,
                                    const MonitorInfo& monitor);
    static QPoint ratioToInternal(const QPointF& ratioPt,
                                   const MonitorInfo& monitor);

    // ---- 虚拟桌面 <-> 比例坐标 (需要知道目标显示器) ----
    static QPointF virtualToRatio(const QPoint& virtualPt,
                                   const MonitorInfo& monitor);
    static QPoint ratioToVirtual(const QPointF& ratioPt,
                                  const MonitorInfo& monitor);

    // ---- 根据坐标模式解析目标虚拟桌面坐标 ----
    static bool resolveTargetCoordinate(
        CoordinateMode mode,
        const QPoint& virtualDesktopAbs,
        const QString& monitorDeviceName,
        const QPoint& monitorInternal,
        const QPointF& monitorRatio,
        const QVector<MonitorInfo>& monitors,
        QPoint& outVirtualPt,
        QString* errorMsg = nullptr);

    // ---- 工具方法 ----
    // 检查坐标是否在虚拟桌面范围内
    static bool isInVirtualDesktop(const QPoint& virtualPt,
                                    const QRect& virtualDesktopBounds);

    // 边界裁剪
    static QPoint clampToVirtualDesktop(const QPoint& virtualPt,
                                         const QRect& virtualDesktopBounds);

private:
    // 验证显示器设备名是否在列表中
    static const MonitorInfo* findMonitor(const QString& deviceName,
                                           const QVector<MonitorInfo>& monitors);
};

#endif // COORDINATEMAPPER_H
