#ifndef MONITORMANAGER_H
#define MONITORMANAGER_H

#include <QObject>
#include <QVector>
#include <QTimer>
#include "MonitorInfo.h"
#include "platform/windows/WindowsMonitorBackend.h"

class QScreen;

// ============================================================================
// 显示器管理器
// 管理多显示器配置，监听变化，提供显示器查询接口
// ============================================================================
class MonitorManager : public QObject
{
    Q_OBJECT

public:
    explicit MonitorManager(QObject* parent = nullptr);

    // 初始化: 枚举所有显示器
    void initialize();

    // 获取当前所有显示器
    QVector<MonitorInfo> monitors() const { return m_monitors; }

    // 获取主显示器
    MonitorInfo primaryMonitor() const;

    // 获取虚拟桌面范围
    QRect virtualDesktopBounds() const { return m_virtualDesktopBounds; }

    // 通过坐标查找显示器
    MonitorInfo monitorAtPoint(const QPoint& virtualPt) const;

    // 通过设备名查找显示器
    MonitorInfo monitorByName(const QString& deviceName) const;

    // 获取当前光标所在显示器
    MonitorInfo currentCursorMonitor() const;

    // 获取当前光标位置
    QPoint currentCursorPos() const;

    // 刷新显示器列表
    void refresh();

    // 检查脚本引用的显示器是否还存在
    bool validateScriptMonitors(const QVector<MonitorInfo>& scriptMonitors,
                                 QString* errorMsg = nullptr) const;

    // 尝试匹配脚本中的显示器到当前显示器
    bool matchMonitor(const MonitorInfo& scriptMonitor,
                      MonitorInfo& outCurrentMonitor) const;

signals:
    void monitorsChanged();
    void monitorAdded(const MonitorInfo& monitor);
    void monitorRemoved(const QString& deviceName);
    void virtualDesktopChanged(const QRect& newBounds);

private slots:
    void onQtScreenAdded(QScreen* screen);
    void onQtScreenRemoved(QScreen* screen);
    void onQtScreenGeometryChanged(const QRect& geometry);

private:
    void connectQtScreenSignals();
    void disconnectQtScreenSignals();

    mutable WindowsMonitorBackend m_backend;
    QVector<MonitorInfo> m_monitors;
    QRect m_virtualDesktopBounds;
};

#endif // MONITORMANAGER_H
