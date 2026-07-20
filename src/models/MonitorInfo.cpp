#include "MonitorInfo.h"
#include <QCryptographicHash>

QString MonitorInfo::uniqueId() const
{
    // 第一版使用 "设备名+分辨率+位置" 组合
    QString data = QString("%1_%2x%3_(%4,%5)")
        .arg(deviceName)
        .arg(resolution.width()).arg(resolution.height())
        .arg(desktopRect.x()).arg(desktopRect.y());

    return QString::fromUtf8(
        QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Md5).toHex());
}

bool MonitorInfo::containsVirtualPoint(const QPoint& virtualPt) const
{
    return desktopRect.contains(virtualPt);
}

QPoint MonitorInfo::virtualToInternal(const QPoint& virtualPt) const
{
    return QPoint(virtualPt.x() - desktopRect.x(),
                  virtualPt.y() - desktopRect.y());
}

QPoint MonitorInfo::internalToVirtual(const QPoint& internalPt) const
{
    return QPoint(internalPt.x() + desktopRect.x(),
                  internalPt.y() + desktopRect.y());
}

QPointF MonitorInfo::internalToRatio(const QPoint& internalPt) const
{
    int w = desktopRect.width();
    int h = desktopRect.height();
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    return QPointF(static_cast<qreal>(internalPt.x()) / w,
                   static_cast<qreal>(internalPt.y()) / h);
}

QPoint MonitorInfo::ratioToInternal(const QPointF& ratioPt) const
{
    int w = desktopRect.width();
    int h = desktopRect.height();

    return QPoint(qRound(ratioPt.x() * w),
                  qRound(ratioPt.y() * h));
}

bool MonitorInfo::matches(const MonitorInfo& other) const
{
    // 第一版: 比较设备名 + 分辨率 + 相对位置
    if (deviceName == other.deviceName) return true;

    // 如果设备名不同，但分辨率和位置匹配，也认为是同一个显示器
    if (resolution == other.resolution
        && desktopRect == other.desktopRect)
        return true;

    return false;
}
