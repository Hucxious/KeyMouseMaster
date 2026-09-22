#include "ScriptEvent.h"
#include <QJsonArray>
#include <cmath>
#include <limits>

// ============================================================================
// ScriptEvent 序列化到 JSON
// ============================================================================
QJsonObject ScriptEvent::toJson() const
{
    QJsonObject obj;
    obj["type"] = static_cast<int>(type);
    obj["timestamp_ms"] = static_cast<qint64>(timestampMs);
    obj["index"] = eventIndex;
    obj["enabled"] = enabled;

    obj["mouse_button"] = static_cast<int>(mouseButton);
    obj["virtual_x"] = virtualDesktopPos.x();
    obj["virtual_y"] = virtualDesktopPos.y();
    obj["monitor_device"] = monitorDeviceName;
    obj["monitor_internal_x"] = monitorInternalPos.x();
    obj["monitor_internal_y"] = monitorInternalPos.y();
    obj["monitor_ratio_x"] = monitorRatioPos.x();
    obj["monitor_ratio_y"] = monitorRatioPos.y();
    obj["wheel_delta"] = wheelDelta;

    obj["win_vk"] = static_cast<int>(winVk);
    obj["scan_code"] = static_cast<int>(scanCode);
    obj["is_extended"] = isExtendedKey;
    obj["has_ctrl"] = hasCtrl;
    obj["has_shift"] = hasShift;
    obj["has_alt"] = hasAlt;
    obj["has_win"] = hasWin;
    obj["key_display"] = keyDisplayName;
    obj["modifiers_at_press"] = static_cast<int>(modifiersAtPress);

    return obj;
}

// ============================================================================
// 从 JSON 反序列化 ScriptEvent
// ============================================================================
ScriptEvent ScriptEvent::fromJson(const QJsonObject& obj, bool* ok)
{
    ScriptEvent ev;
    bool valid = true;

    auto getInt = [&](const QString& key, int defaultVal = 0) -> int {
        if (!obj.contains(key)) return defaultVal;
        const auto value = obj[key];
        const double number = value.toDouble();
        if (!value.isDouble() || !std::isfinite(number) || std::floor(number) != number
            || number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
            valid = false; return defaultVal;
        }
        return static_cast<int>(number);
    };
    auto getStr = [&](const QString& key, const QString& defaultVal = {}) -> QString {
        if (obj.contains(key) && obj[key].isString())
            return obj[key].toString();
        return defaultVal;
    };
    auto getBool = [&](const QString& key, bool defaultVal = false) -> bool {
        if (obj.contains(key) && obj[key].isBool())
            return obj[key].toBool();
        return defaultVal;
    };

    int typeInt = getInt("type", -1);
    if (typeInt < 0 || typeInt > 6) valid = false;
    ev.type = static_cast<ScriptEventType>(typeInt);

    const double stamp = obj["timestamp_ms"].toDouble(-1);
    if (!std::isfinite(stamp) || stamp < 0 || stamp > 9007199254740991.0 || std::floor(stamp) != stamp)
        valid = false;
    ev.timestampMs = valid ? static_cast<int64_t>(stamp) : -1;
    if (ev.timestampMs < 0) valid = false;

    ev.eventIndex = getInt("index", 0);
    ev.enabled = getBool("enabled", true);

    int btn = getInt("mouse_button", 0);
    ev.mouseButton = static_cast<MouseButton>(btn);

    ev.virtualDesktopPos = QPoint(getInt("virtual_x"), getInt("virtual_y"));
    ev.monitorDeviceName = getStr("monitor_device");
    ev.monitorInternalPos = QPoint(getInt("monitor_internal_x"), getInt("monitor_internal_y"));
    ev.monitorRatioPos = QPointF(
        obj.contains("monitor_ratio_x") ? obj["monitor_ratio_x"].toDouble() : 0.0,
        obj.contains("monitor_ratio_y") ? obj["monitor_ratio_y"].toDouble() : 0.0
    );
    ev.wheelDelta = getInt("wheel_delta", 0);

    ev.winVk = static_cast<uint32_t>(getInt("win_vk", 0));
    ev.scanCode = static_cast<uint32_t>(getInt("scan_code", 0));
    ev.isExtendedKey = getBool("is_extended", false);
    ev.hasCtrl  = getBool("has_ctrl", false);
    ev.hasShift = getBool("has_shift", false);
    ev.hasAlt   = getBool("has_alt", false);
    ev.hasWin   = getBool("has_win", false);
    ev.keyDisplayName = getStr("key_display");
    ev.modifiersAtPress = static_cast<uint32_t>(getInt("modifiers_at_press", 0));

    if (ok) *ok = valid && ev.isValid();
    return ev;
}

bool ScriptEvent::isValid() const
{
    return validationError().isEmpty();
}

QString ScriptEvent::validationError() const
{
    if (type < ScriptEventType::MouseMove || type > ScriptEventType::KeyUp)
        return QString("无效的事件类型: %1").arg(static_cast<int>(type));
    if (timestampMs < 0)
        return "时间戳不能为负数";
    if (timestampMs > 9007199254740991LL) return "时间戳超出 JSON 精确整数范围";
    if (isMouseEvent()) {
        if (mouseButton < MouseButton::Left || mouseButton > MouseButton::XButton2)
            return "无效鼠标按钮";
        if (!std::isfinite(monitorRatioPos.x()) || !std::isfinite(monitorRatioPos.y())
            || monitorRatioPos.x() < 0 || monitorRatioPos.x() > 1
            || monitorRatioPos.y() < 0 || monitorRatioPos.y() > 1)
            return "无效显示器比例坐标";
    }
    if (isKeyboardEvent()) {
        if (winVk == 0 || winVk > 254)
            return "键盘事件缺少虚拟键码";
    }
    return {}; // 空字符串表示无错误
}

bool ScriptEvent::isMouseEvent() const
{
    return type >= ScriptEventType::MouseMove && type <= ScriptEventType::MouseHWheel;
}

bool ScriptEvent::isKeyboardEvent() const
{
    return type == ScriptEventType::KeyDown || type == ScriptEventType::KeyUp;
}

QString ScriptEvent::eventSummary() const
{
    switch (type) {
    case ScriptEventType::MouseMove:
        return QString("鼠标移动到 (%1, %2) [%3]")
            .arg(virtualDesktopPos.x()).arg(virtualDesktopPos.y())
            .arg(monitorDeviceName);
    case ScriptEventType::MouseDown:
        return QString("按下 %1").arg(mouseButtonToString(mouseButton));
    case ScriptEventType::MouseUp:
        return QString("释放 %1").arg(mouseButtonToString(mouseButton));
    case ScriptEventType::MouseWheel:
        return QString("滚轮 %1").arg(wheelDelta);
    case ScriptEventType::MouseHWheel:
        return QString("水平滚轮 %1").arg(wheelDelta);
    case ScriptEventType::KeyDown:
        return QString("按下 %1").arg(keyDisplayName);
    case ScriptEventType::KeyUp:
        return QString("释放 %1").arg(keyDisplayName);
    }
    return "未知事件";
}
