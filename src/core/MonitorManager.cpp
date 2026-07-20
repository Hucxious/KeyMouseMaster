#include "MonitorManager.h"
#include "Logger.h"
#include <QGuiApplication>
#include <QScreen>
#include <QSet>

MonitorManager::MonitorManager(QObject* parent)
    : QObject(parent)
{
}

void MonitorManager::initialize()
{
    refresh();
    connectQtScreenSignals();
    LOG_INFO(QString("显示器管理器初始化: %1 个显示器, 虚拟桌面 %2")
        .arg(m_monitors.size())
        .arg(QString("%1x%2+(%3,%4)")
            .arg(m_virtualDesktopBounds.width())
            .arg(m_virtualDesktopBounds.height())
            .arg(m_virtualDesktopBounds.x())
            .arg(m_virtualDesktopBounds.y())));
}

void MonitorManager::refresh()
{
    QVector<MonitorInfo> oldMonitors = m_monitors;

    m_monitors = m_backend.enumerateMonitors();
    m_virtualDesktopBounds = m_backend.getVirtualDesktopBounds();

    // 检测新增和移除的显示器
    QSet<QString> oldNames, newNames;
    for (const auto& m : oldMonitors) oldNames.insert(m.deviceName);
    for (const auto& m : m_monitors)   newNames.insert(m.deviceName);

    for (const auto& name : oldNames) {
        if (!newNames.contains(name))
            emit monitorRemoved(name);
    }
    for (const auto& name : newNames) {
        if (!oldNames.contains(name)) {
            auto it = std::find_if(m_monitors.begin(), m_monitors.end(),
                                    [&](const MonitorInfo& m) {
                                        return m.deviceName == name;
                                    });
            if (it != m_monitors.end())
                emit monitorAdded(*it);
        }
    }

    if (oldMonitors.size() != m_monitors.size()
        || oldNames != newNames) {
        emit monitorsChanged();
        LOG_INFO("显示器配置已更新");
    }
}

MonitorInfo MonitorManager::primaryMonitor() const
{
    for (const auto& m : m_monitors) {
        if (m.isPrimary) return m;
    }
    return m_monitors.isEmpty() ? MonitorInfo() : m_monitors.first();
}

MonitorInfo MonitorManager::monitorAtPoint(const QPoint& virtualPt) const
{
    return m_backend.monitorAtPoint(virtualPt, m_monitors);
}

MonitorInfo MonitorManager::monitorByName(const QString& deviceName) const
{
    return m_backend.monitorByDeviceName(deviceName, m_monitors);
}

MonitorInfo MonitorManager::currentCursorMonitor() const
{
    return m_backend.getCurrentCursorMonitor();
}

QPoint MonitorManager::currentCursorPos() const
{
    return m_backend.getCurrentCursorPos();
}

bool MonitorManager::validateScriptMonitors(const QVector<MonitorInfo>& scriptMonitors,
                                              QString* errorMsg) const
{
    // 收集脚本中引用的所有显示器
    for (const auto& sm : scriptMonitors) {
        bool found = false;
        for (const auto& cm : m_monitors) {
            if (cm.matches(sm)) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (errorMsg)
                *errorMsg = QString("脚本引用的显示器 '%1' (%2x%3 偏移(%4,%5)) "
                                   "在当前系统中未找到匹配")
                    .arg(sm.deviceName)
                    .arg(sm.desktopRect.width()).arg(sm.desktopRect.height())
                    .arg(sm.desktopRect.x()).arg(sm.desktopRect.y());
            return false;
        }
    }
    return true;
}

bool MonitorManager::matchMonitor(const MonitorInfo& scriptMonitor,
                                    MonitorInfo& outCurrentMonitor) const
{
    for (const auto& cm : m_monitors) {
        if (cm.matches(scriptMonitor)) {
            outCurrentMonitor = cm;
            return true;
        }
    }
    return false;
}

// ============================================================================
// Qt 屏幕信号连接
// ============================================================================
void MonitorManager::connectQtScreenSignals()
{
    QGuiApplication* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (!app) return;

    connect(app, &QGuiApplication::screenAdded,
            this, &MonitorManager::onQtScreenAdded);
    connect(app, &QGuiApplication::screenRemoved,
            this, &MonitorManager::onQtScreenRemoved);

    for (QScreen* screen : QGuiApplication::screens()) {
        connect(screen, &QScreen::geometryChanged,
                this, &MonitorManager::onQtScreenGeometryChanged);
        connect(screen, &QScreen::logicalDotsPerInchChanged,
                this, [this](qreal dpi) {
                    Q_UNUSED(dpi)
                    refresh();
                });
    }
}

void MonitorManager::disconnectQtScreenSignals()
{
    QGuiApplication* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if (app) {
        disconnect(app, &QGuiApplication::screenAdded,
                   this, &MonitorManager::onQtScreenAdded);
        disconnect(app, &QGuiApplication::screenRemoved,
                    this, &MonitorManager::onQtScreenRemoved);
    }
}

void MonitorManager::onQtScreenAdded(QScreen* screen)
{
    Q_UNUSED(screen)
    refresh();
}

void MonitorManager::onQtScreenRemoved(QScreen* screen)
{
    Q_UNUSED(screen)
    refresh();
}

void MonitorManager::onQtScreenGeometryChanged(const QRect& geometry)
{
    Q_UNUSED(geometry)
    refresh();
}
