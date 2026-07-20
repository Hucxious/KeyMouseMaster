#ifndef WINDOWSMONITORBACKEND_H
#define WINDOWSMONITORBACKEND_H

#include <QObject>
#include <QVector>
#include "MonitorInfo.h"
#include "WinCompat.h"

// ============================================================================
// Windows 显示器后端
// 使用 EnumDisplayMonitors / GetMonitorInfo 等 Win32 API 获取显示器信息
// 综合 QScreen 信息，提供完整的显示器数据
// ============================================================================
class WindowsMonitorBackend : public QObject
{
    Q_OBJECT

public:
    explicit WindowsMonitorBackend(QObject* parent = nullptr);

    // 枚举所有显示器
    QVector<MonitorInfo> enumerateMonitors();

    // 获取包含指定虚拟桌面坐标的显示器
    MonitorInfo monitorAtPoint(const QPoint& virtualPt,
                                const QVector<MonitorInfo>& monitors) const;

    // 通过设备名查找显示器
    MonitorInfo monitorByDeviceName(const QString& deviceName,
                                     const QVector<MonitorInfo>& monitors) const;

    // 获取虚拟桌面范围
    QRect getVirtualDesktopBounds();

    // 获取当前鼠标所在显示器
    MonitorInfo getCurrentCursorMonitor();

    // 获取当前鼠标虚拟桌面坐标
    QPoint getCurrentCursorPos();

signals:
    void monitorsChanged();

private:
    // Win32 回调
#ifdef Q_OS_WIN
    static BOOL CALLBACK monitorEnumProc(HMONITOR hMonitor, HDC hdc, LPRECT rect, LPARAM lParam);
#endif

    // 从 QScreen 补充 DPI 和缩放信息
    void enrichWithQScreenInfo(MonitorInfo& info);
};

#endif // WINDOWSMONITORBACKEND_H
