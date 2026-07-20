#ifndef MONITORINFO_H
#define MONITORINFO_H

#include <QString>
#include <QRect>
#include <QPoint>
#include <QSize>
#include <cstdint>

// ============================================================================
// 显示器信息结构
// 包含Windows虚拟桌面坐标、显示器内部坐标、DPI等完整信息
// ============================================================================
struct MonitorInfo
{
    QString  deviceName;        // 设备名称，如 "\\.\DISPLAY1"
    QString  friendlyName;      // 友好名称
    bool     isPrimary = false;

    // 虚拟桌面坐标系的区域 (可能包含负值)
    QRect    desktopRect;       // 显示器在虚拟桌面的完整区域
    QRect    workAreaRect;      // 可用区域(不含任务栏)

    // 显示器内部坐标 (左上角始终为0,0)
    QSize    resolution;        // 物理分辨率
    int      dpi = 96;
    qreal    scaleFactor = 1.0; // 缩放比例

    // 相对虚拟桌面的位置
    QPoint   desktopOffset;     // 等同于 desktopRect.topLeft()

    // 显示器序号 (用于用户识别)
    int      index = 0;

    // 唯一标识哈希 (用于脚本中的显示器匹配)
    // 第一版使用 "设备名+分辨率+位置" 组合
    QString  uniqueId() const;

    // 检查一个虚拟桌面坐标是否在此显示器内
    bool containsVirtualPoint(const QPoint& virtualPt) const;

    // 将虚拟桌面坐标转换为显示器内部坐标
    QPoint virtualToInternal(const QPoint& virtualPt) const;

    // 将显示器内部坐标转换为虚拟桌面坐标
    QPoint internalToVirtual(const QPoint& internalPt) const;

    // 将显示器内部坐标转换为比例坐标 [0.0, 1.0]
    QPointF internalToRatio(const QPoint& internalPt) const;

    // 将比例坐标转换为显示器内部坐标
    QPoint ratioToInternal(const QPointF& ratioPt) const;

    // 用于匹配断开的显示器
    bool matches(const MonitorInfo& other) const;
};

#endif // MONITORINFO_H
