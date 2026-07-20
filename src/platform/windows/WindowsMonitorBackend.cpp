#include "WindowsMonitorBackend.h"
#include "Logger.h"
#include <QGuiApplication>
#include <QScreen>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winuser.h>

// 在函数体外定义，以便 monitorEnumProc 中使用 reinterpret_cast
struct MonitorEnumContext {
    QVector<MonitorInfo>* monitors;
    int index;
};
#endif

WindowsMonitorBackend::WindowsMonitorBackend(QObject* parent)
    : QObject(parent)
{
}

QVector<MonitorInfo> WindowsMonitorBackend::enumerateMonitors()
{
    QVector<MonitorInfo> monitors;

#ifdef Q_OS_WIN
    MonitorEnumContext ctx = {&monitors, 0};

    EnumDisplayMonitors(nullptr, nullptr, monitorEnumProc, reinterpret_cast<LPARAM>(&ctx));
#endif

    // 补充Qt的屏幕信息 (DPI、缩放等)
    for (auto& m : monitors) {
        enrichWithQScreenInfo(m);
    }

    // 按虚拟桌面x坐标排序
    std::sort(monitors.begin(), monitors.end(),
              [](const MonitorInfo& a, const MonitorInfo& b) {
                  return a.desktopRect.x() < b.desktopRect.x();
              });

    // 重新分配序号
    for (int i = 0; i < monitors.size(); ++i) {
        monitors[i].index = i + 1;
    }

    return monitors;
}

#ifdef Q_OS_WIN
BOOL CALLBACK WindowsMonitorBackend::monitorEnumProc(HMONITOR hMonitor, HDC hdc,
                                                      LPRECT rect, LPARAM lParam)
{
    Q_UNUSED(hdc)

    auto* ctx = reinterpret_cast<MonitorEnumContext*>(lParam);
    QVector<MonitorInfo>* monitors = ctx->monitors;

    MONITORINFOEXW mi;
    mi.cbSize = sizeof(MONITORINFOEXW);
    if (!GetMonitorInfoW(hMonitor, &mi)) {
        return TRUE;  // 继续枚举
    }

    MonitorInfo info;
    info.deviceName = QString::fromWCharArray(mi.szDevice);
    info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    info.desktopRect = QRect(mi.rcMonitor.left, mi.rcMonitor.top,
                              mi.rcMonitor.right - mi.rcMonitor.left,
                              mi.rcMonitor.bottom - mi.rcMonitor.top);
    info.workAreaRect = QRect(mi.rcWork.left, mi.rcWork.top,
                               mi.rcWork.right - mi.rcWork.left,
                               mi.rcWork.bottom - mi.rcWork.top);
    info.desktopOffset = info.desktopRect.topLeft();
    info.resolution = info.desktopRect.size();
    info.index = ctx->index++;

    // 获取设备友好名称
    DISPLAY_DEVICEW dd;
    dd.cb = sizeof(DISPLAY_DEVICEW);
    if (EnumDisplayDevicesW(mi.szDevice, 0, &dd, 0)) {
        info.friendlyName = QString::fromWCharArray(dd.DeviceString);
    }

    monitors->append(info);
    return TRUE;
}
#endif

void WindowsMonitorBackend::enrichWithQScreenInfo(MonitorInfo& info)
{
    // 使用 QScreen 补充 DPI 和缩放信息
    QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        QRect screenGeo = screen->geometry();
        // 匹配: Qt使用逻辑坐标，但geometry返回的是屏幕在虚拟桌面的位置(物理像素)
        // 这里做近似匹配
        if (screenGeo.topLeft() == info.desktopRect.topLeft()
            || screen->name() == info.deviceName
            || (qAbs(screenGeo.x() - info.desktopRect.x()) < 10
                && qAbs(screenGeo.y() - info.desktopRect.y()) < 10)) {
            info.dpi = static_cast<int>(screen->logicalDotsPerInch());
            info.scaleFactor = screen->devicePixelRatio();
            info.resolution = screen->size() * screen->devicePixelRatio();
            break;
        }
    }
}

MonitorInfo WindowsMonitorBackend::monitorAtPoint(const QPoint& virtualPt,
                                                    const QVector<MonitorInfo>& monitors) const
{
    for (const auto& m : monitors) {
        if (m.containsVirtualPoint(virtualPt))
            return m;
    }
    return {}; // 未找到
}

MonitorInfo WindowsMonitorBackend::monitorByDeviceName(const QString& deviceName,
                                                         const QVector<MonitorInfo>& monitors) const
{
    for (const auto& m : monitors) {
        if (m.deviceName == deviceName)
            return m;
    }
    return {};
}

QRect WindowsMonitorBackend::getVirtualDesktopBounds()
{
#ifdef Q_OS_WIN
    int left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return QRect(left, top, width, height);
#else
    // Fallback: use QScreen aggregation
    int left = 0, top = 0, right = 0, bottom = 0;
    for (QScreen* screen : QGuiApplication::screens()) {
        QRect geo = screen->geometry();
        left = qMin(left, geo.left());
        top = qMin(top, geo.top());
        right = qMax(right, geo.right());
        bottom = qMax(bottom, geo.bottom());
    }
    return QRect(left, top, right - left, bottom - top);
#endif
}

MonitorInfo WindowsMonitorBackend::getCurrentCursorMonitor()
{
#ifdef Q_OS_WIN
    POINT pt;
    GetCursorPos(&pt);
    QPoint cursorPos(pt.x, pt.y);
    auto monitors = enumerateMonitors();
    return monitorAtPoint(cursorPos, monitors);
#else
    return {};
#endif
}

QPoint WindowsMonitorBackend::getCurrentCursorPos()
{
#ifdef Q_OS_WIN
    POINT pt;
    GetCursorPos(&pt);
    return QPoint(pt.x, pt.y);
#else
    return QPoint();
#endif
}
