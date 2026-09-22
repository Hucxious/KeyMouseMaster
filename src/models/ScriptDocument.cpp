#include "ScriptDocument.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <cmath>

namespace {
bool readCoordinateMode(const QJsonValue& value, CoordinateMode& mode)
{
    if (value.isUndefined()) { mode = CoordinateMode::MonitorRelative; return true; }
    const QString text = value.toString();
    const char* names[] = {"CurrentCursor", "VirtualDesktopAbsolute", "MonitorRelative", "MonitorRatio"};
    for (int i = 0; i < 4; ++i) {
        const auto candidate = static_cast<CoordinateMode>(i);
        // 兼容现有 v1 文件的中文值和早期文档给出的英文值，不改变保存格式。
        if (text == names[i] || text == QString::fromUtf8(coordinateModeToString(candidate))) {
            mode = candidate; return true;
        }
    }
    return false;
}
}


ScriptDocument::ScriptDocument()
{
    createdAt = QDateTime::currentDateTime();
    modifiedAt = createdAt;
}

QJsonObject ScriptDocument::toJson() const
{
    QJsonObject obj;
    obj["format"] = AppConstants::SCRIPT_FORMAT;
    obj["version"] = AppConstants::SCRIPT_VERSION;
    obj["name"] = name;
    obj["description"] = description;
    obj["created_at"] = createdAt.toString(Qt::ISODate);
    obj["modified_at"] = modifiedAt.toString(Qt::ISODate);
    obj["coordinate_mode"] = coordinateModeToString(coordinateMode);

    // 桌面范围
    QJsonObject desktop;
    desktop["x"] = virtualDesktopBounds.x();
    desktop["y"] = virtualDesktopBounds.y();
    desktop["width"] = virtualDesktopBounds.width();
    desktop["height"] = virtualDesktopBounds.height();
    obj["desktop"] = desktop;

    // 显示器列表
    QJsonArray monitorsArr;
    for (const auto& m : monitors) {
        QJsonObject mo;
        mo["device_name"] = m.deviceName;
        mo["friendly_name"] = m.friendlyName;
        mo["is_primary"] = m.isPrimary;
        mo["desktop_x"] = m.desktopRect.x();
        mo["desktop_y"] = m.desktopRect.y();
        mo["width"] = m.desktopRect.width();
        mo["height"] = m.desktopRect.height();
        mo["resolution_w"] = m.resolution.width();
        mo["resolution_h"] = m.resolution.height();
        mo["dpi"] = m.dpi;
        mo["scale_factor"] = m.scaleFactor;
        mo["index"] = m.index;
        monitorsArr.append(mo);
    }
    obj["monitors"] = monitorsArr;

    // 回放设置
    QJsonObject pb;
    pb["start_delay_ms"] = playbackSettings.startDelayMs;
    pb["repeat_count"] = playbackSettings.repeatCount;
    pb["infinite_repeat"] = playbackSettings.infiniteRepeat;
    pb["round_interval_ms"] = playbackSettings.roundIntervalMs;
    pb["speed_factor"] = playbackSettings.speedFactor;
    pb["restore_cursor"] = playbackSettings.restoreCursor;
    pb["skip_disabled"] = playbackSettings.skipDisabledEvents;
    pb["coordinate_mode"] = coordinateModeToString(playbackSettings.coordinateMode);
    obj["playback"] = pb;

    // 事件数组
    QJsonArray eventsArr;
    for (const auto& ev : events) {
        eventsArr.append(ev.toJson());
    }
    obj["events"] = eventsArr;

    return obj;
}

bool ScriptDocument::fromJson(const QJsonObject& obj, QString* errorMsg)
{
    // 解析失败保留当前文档，避免导入坏文件破坏未保存的编辑内容。
    ScriptDocument parsed;
    if (!parsed.parseJson(obj, errorMsg)) return false;
    if (!parsed.events.isEmpty() && !parsed.isValid(errorMsg)) return false;
    *this = parsed;
    return true;
}

bool ScriptDocument::parseJson(const QJsonObject& obj, QString* errorMsg)
{
    if (!validateJsonStructure(obj, errorMsg))
        return false;

    name = obj["name"].toString();
    description = obj["description"].toString();
    createdAt = QDateTime::fromString(obj["created_at"].toString(), Qt::ISODate);
    modifiedAt = QDateTime::fromString(obj["modified_at"].toString(), Qt::ISODate);

    if (!readCoordinateMode(obj["coordinate_mode"], coordinateMode)) {
        if (errorMsg) *errorMsg = "无效的坐标模式";
        return false;
    }

    // 桌面范围
    QJsonObject desktop = obj["desktop"].toObject();
    virtualDesktopBounds = QRect(
        desktop["x"].toInt(), desktop["y"].toInt(),
        desktop["width"].toInt(), desktop["height"].toInt()
    );

    // 显示器
    monitors.clear();
    QJsonArray monitorsArr = obj["monitors"].toArray();
    for (const auto& mv : monitorsArr) {
        QJsonObject mo = mv.toObject();
        MonitorInfo mi;
        mi.deviceName   = mo["device_name"].toString();
        mi.friendlyName = mo["friendly_name"].toString();
        mi.isPrimary    = mo["is_primary"].toBool();
        mi.desktopRect  = QRect(mo["desktop_x"].toInt(), mo["desktop_y"].toInt(),
                                mo["width"].toInt(), mo["height"].toInt());
        mi.resolution   = QSize(mo["resolution_w"].toInt(), mo["resolution_h"].toInt());
        mi.dpi          = mo["dpi"].toInt();
        mi.scaleFactor  = mo["scale_factor"].toDouble();
        mi.index        = mo["index"].toInt();
        mi.desktopOffset = mi.desktopRect.topLeft();
        mi.workAreaRect  = mi.desktopRect;
        monitors.append(mi);
    }

    // 回放设置
    QJsonObject pb = obj["playback"].toObject();
    playbackSettings.startDelayMs    = pb["start_delay_ms"].toInt();
    playbackSettings.repeatCount     = pb["repeat_count"].toInt(1);
    playbackSettings.infiniteRepeat  = pb["infinite_repeat"].toBool();
    playbackSettings.roundIntervalMs = pb["round_interval_ms"].toInt(1000);
    playbackSettings.speedFactor     = pb["speed_factor"].toDouble(1.0);
    playbackSettings.restoreCursor   = pb["restore_cursor"].toBool(true);
    playbackSettings.skipDisabledEvents = pb["skip_disabled"].toBool(true);

    if (!readCoordinateMode(pb["coordinate_mode"], playbackSettings.coordinateMode)
        || !std::isfinite(playbackSettings.speedFactor) || playbackSettings.speedFactor < 0.1
        || playbackSettings.speedFactor > 10 || playbackSettings.startDelayMs < 0
        || playbackSettings.startDelayMs > 60000 || playbackSettings.repeatCount < 1
        || playbackSettings.repeatCount > AppConstants::MAX_REPEAT_COUNT
        || playbackSettings.roundIntervalMs < 0 || playbackSettings.roundIntervalMs > 3600000) {
        if (errorMsg) *errorMsg = "无效的回放参数";
        return false;
    }
    // 事件
    events.clear();
    QJsonArray eventsArr = obj["events"].toArray();
    for (int i = 0; i < eventsArr.size(); ++i) {
        bool ok = false;
        ScriptEvent ev = ScriptEvent::fromJson(eventsArr[i].toObject(), &ok);
        if (ok) {
            ev.eventIndex = i;
            events.append(ev);
        } else {
            if (errorMsg) *errorMsg = QString("事件 %1 数据无效").arg(i);
            return false;
        }
    }

    return true;
}

bool ScriptDocument::saveToFile(const QString& path, QString* errorMsg)
{
    QJsonObject doc = toJson();
    QJsonDocument jsonDoc(doc);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg)
            *errorMsg = QString("无法打开文件: %1").arg(file.errorString());
        return false;
    }

    const QByteArray bytes = jsonDoc.toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        if (errorMsg) *errorMsg = QString("写入文件失败: %1").arg(file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (errorMsg)
            *errorMsg = QString("写入文件失败: %1").arg(file.errorString());
        return false;
    }

    this->filePath = path;
    markModified();
    return true;
}

bool ScriptDocument::loadFromFile(const QString& path, QString* errorMsg)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMsg)
            *errorMsg = QString("无法打开文件: %1").arg(file.errorString());
        return false;
    }

    if (file.size() > 128 * 1024 * 1024) {
        if (errorMsg) *errorMsg = "脚本文件超过 128 MiB 上限";
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMsg)
            *errorMsg = QString("JSON解析错误: %1").arg(parseError.errorString());
        return false;
    }

    if (!jsonDoc.isObject()) {
        if (errorMsg)
            *errorMsg = "脚本文件根节点必须是JSON对象";
        return false;
    }

    if (!fromJson(jsonDoc.object(), errorMsg))
        return false;

    this->filePath = path;
    return true;
}

bool ScriptDocument::isValid(QString* errorMsg) const
{
    if (events.isEmpty()) {
        if (errorMsg) *errorMsg = "脚本事件列表为空";
        return false;
    }
    if (events.size() > AppConstants::MAX_SCRIPT_EVENTS) {
        if (errorMsg) *errorMsg = QString("事件数量(%1)超过上限(%2)")
            .arg(events.size()).arg(AppConstants::MAX_SCRIPT_EVENTS);
        return false;
    }
    for (int i = 0; i < events.size(); ++i) {
        if (i > 0 && events[i].timestampMs < events[i - 1].timestampMs) {
            if (errorMsg) *errorMsg = "事件时间戳必须按行非递减";
            return false;
        }
        QString evErr = events[i].validationError();
        if (!evErr.isEmpty()) {
            if (errorMsg) *errorMsg = QString("事件 %1: %2").arg(i).arg(evErr);
            return false;
        }
    }
    return true;
}

void ScriptDocument::clear()
{
    playbackSettings = PlaybackSettings();
    recordingSettings = RecordingSettings();
    coordinateMode = CoordinateMode::MonitorRelative;
    virtualDesktopBounds = QRect();
    events.clear();
    name.clear();
    description.clear();
    filePath.clear();
    monitors.clear();
    createdAt = QDateTime::currentDateTime();
    markModified();
}

void ScriptDocument::addEvent(const ScriptEvent& ev)
{
    events.append(ev);
    markModified();
}

void ScriptDocument::removeEvent(int index)
{
    if (index >= 0 && index < events.size()) {
        events.removeAt(index);
        // 重新编号
        for (int i = 0; i < events.size(); ++i)
            events[i].eventIndex = i;
        markModified();
    }
}

void ScriptDocument::moveEventUp(int index)
{
    if (index > 0 && index < events.size()) {
        std::swap(events[index].timestampMs, events[index - 1].timestampMs);
        events.swapItemsAt(index, index - 1);
        events[index].eventIndex = index;
        events[index - 1].eventIndex = index - 1;
        markModified();
    }
}

void ScriptDocument::moveEventDown(int index)
{
    if (index >= 0 && index < events.size() - 1) {
        std::swap(events[index].timestampMs, events[index + 1].timestampMs);
        events.swapItemsAt(index, index + 1);
        events[index].eventIndex = index;
        events[index + 1].eventIndex = index + 1;
        markModified();
    }
}

void ScriptDocument::toggleEventEnabled(int index)
{
    if (index >= 0 && index < events.size()) {
        events[index].enabled = !events[index].enabled;
        markModified();
    }
}

bool ScriptDocument::validateMonitors(const QVector<MonitorInfo>& currentMonitors,
                                       QString* errorMsg) const
{
    // 收集脚本中引用的显示器设备名
    QSet<QString> usedMonitors;
    for (const auto& ev : events) {
        if (ev.enabled && ev.isMouseEvent() && !ev.monitorDeviceName.isEmpty())
            usedMonitors.insert(ev.monitorDeviceName);
    }

    // 检查每个引用的显示器是否能在当前系统中找到匹配
    for (const auto& usedName : usedMonitors) {
        bool found = false;
        // 首先尝试在脚本保存的显示器列表中查找
        MonitorInfo scriptMonitor;
        for (const auto& sm : monitors) {
            if (sm.deviceName == usedName) {
                scriptMonitor = sm;
                break;
            }
        }

        // 在当前显示器中查找匹配
        for (const auto& cm : currentMonitors) {
            if (cm.deviceName == usedName) {
                found = true;
                break;
            }
        }

        if (!found) {
            if (errorMsg)
                *errorMsg = QString("脚本引用的显示器 '%1' 在当前系统中未找到匹配，"
                                   "请确认显示器连接后重试").arg(usedName);
            return false;
        }
    }
    return true;
}

void ScriptDocument::markModified()
{
    modifiedAt = QDateTime::currentDateTime();
}

bool ScriptDocument::validateJsonStructure(const QJsonObject& obj, QString* errorMsg) const
{
    // 检查format
    if (!obj.contains("format") || obj["format"].toString() != AppConstants::SCRIPT_FORMAT) {
        if (errorMsg)
            *errorMsg = QString("无效的脚本格式，期望 '%1'").arg(AppConstants::SCRIPT_FORMAT);
        return false;
    }

    // 检查版本
    int version = obj["version"].toInt(-1);
    if (version < 1 || version > AppConstants::SCRIPT_VERSION) {
        if (errorMsg)
            *errorMsg = QString("不支持的脚本版本: %1 (当前支持版本 %2)")
                .arg(version).arg(AppConstants::SCRIPT_VERSION);
        return false;
    }

    // 检查事件数组
    if (!obj.contains("events") || !obj["events"].isArray()) {
        if (errorMsg)
            *errorMsg = "脚本缺少events数组";
        return false;
    }

    QJsonArray eventsArr = obj["events"].toArray();
    if (eventsArr.size() > AppConstants::MAX_SCRIPT_EVENTS) {
        if (errorMsg)
            *errorMsg = QString("事件数量(%1)超过上限(%2)")
                .arg(eventsArr.size()).arg(AppConstants::MAX_SCRIPT_EVENTS);
        return false;
    }

    return true;
}
