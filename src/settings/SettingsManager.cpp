#include "SettingsManager.h"
#include <QStandardPaths>
#include <QDir>

SettingsManager::SettingsManager(QObject* parent)
    : QObject(parent)
    , m_settings("KeyMouseMaster", "KeyMouseMaster")
{
}

// ============ 窗口状态 ============
QSize SettingsManager::windowSize() const
{
    return m_settings.value("window/size", QSize(900, 650)).toSize();
}

void SettingsManager::setWindowSize(const QSize& size)
{
    m_settings.setValue("window/size", size);
}

QPoint SettingsManager::windowPosition() const
{
    return m_settings.value("window/position", QPoint(-1, -1)).toPoint();
}

void SettingsManager::setWindowPosition(const QPoint& pos)
{
    m_settings.setValue("window/position", pos);
}

int SettingsManager::currentTabIndex() const
{
    return m_settings.value("window/tabIndex", 0).toInt();
}

void SettingsManager::setCurrentTabIndex(int index)
{
    m_settings.setValue("window/tabIndex", index);
}

// ============ 鼠标连点参数 ============
int SettingsManager::mouseClickInterval() const
{
    return m_settings.value("mouse/intervalMs", 100).toInt();
}

void SettingsManager::setMouseClickInterval(int ms)
{
    m_settings.setValue("mouse/intervalMs", ms);
}

int SettingsManager::mouseClickIntervalUnit() const
{
    return m_settings.value("mouse/intervalUnit", 0).toInt();
}

void SettingsManager::setMouseClickIntervalUnit(int unit)
{
    m_settings.setValue("mouse/intervalUnit", unit);
}

int SettingsManager::mousePressDuration() const
{
    return m_settings.value("mouse/pressDurationMs", 50).toInt();
}

void SettingsManager::setMousePressDuration(int ms)
{
    m_settings.setValue("mouse/pressDurationMs", ms);
}

int SettingsManager::mouseDoubleClickInterval() const
{
    return m_settings.value("mouse/doubleClickIntervalMs", 100).toInt();
}

void SettingsManager::setMouseDoubleClickInterval(int ms)
{
    m_settings.setValue("mouse/doubleClickIntervalMs", ms);
}

int SettingsManager::mouseStartDelay() const
{
    return m_settings.value("mouse/startDelayMs", 0).toInt();
}

void SettingsManager::setMouseStartDelay(int ms)
{
    m_settings.setValue("mouse/startDelayMs", ms);
}

int SettingsManager::mouseRepeatCount() const
{
    return m_settings.value("mouse/repeatCount", 1).toInt();
}

void SettingsManager::setMouseRepeatCount(int count)
{
    m_settings.setValue("mouse/repeatCount", count);
}

bool SettingsManager::mouseInfinite() const
{
    return m_settings.value("mouse/infinite", false).toBool();
}

void SettingsManager::setMouseInfinite(bool infinite)
{
    m_settings.setValue("mouse/infinite", infinite);
}

bool SettingsManager::mouseRestoreCursor() const
{
    return m_settings.value("mouse/restoreCursor", false).toBool();
}

void SettingsManager::setMouseRestoreCursor(bool restore)
{
    m_settings.setValue("mouse/restoreCursor", restore);
}

int SettingsManager::mouseButton() const
{
    return m_settings.value("mouse/button", static_cast<int>(MouseButton::Left)).toInt();
}

void SettingsManager::setMouseButton(int btn)
{
    m_settings.setValue("mouse/button", btn);
}

int SettingsManager::mouseClickMode() const
{
    return m_settings.value("mouse/clickMode", static_cast<int>(ClickMode::Single)).toInt();
}

void SettingsManager::setMouseClickMode(int mode)
{
    m_settings.setValue("mouse/clickMode", mode);
}

CoordinateMode SettingsManager::mouseCoordinateMode() const
{
    int v = m_settings.value("mouse/coordMode", static_cast<int>(CoordinateMode::CurrentCursor)).toInt();
    return static_cast<CoordinateMode>(v);
}

void SettingsManager::setMouseCoordinateMode(CoordinateMode mode)
{
    m_settings.setValue("mouse/coordMode", static_cast<int>(mode));
}

QPoint SettingsManager::mouseFixedPos() const
{
    return m_settings.value("mouse/fixedPos", QPoint(0, 0)).toPoint();
}

void SettingsManager::setMouseFixedPos(const QPoint& pos)
{
    m_settings.setValue("mouse/fixedPos", pos);
}

QString SettingsManager::mouseMonitorDevice() const
{
    return m_settings.value("mouse/monitorDevice", "").toString();
}

void SettingsManager::setMouseMonitorDevice(const QString& device)
{
    m_settings.setValue("mouse/monitorDevice", device);
}

// ============ 键盘连点参数 ============
int SettingsManager::keyboardInterval() const
{
    return m_settings.value("keyboard/intervalMs", 100).toInt();
}

void SettingsManager::setKeyboardInterval(int ms)
{
    m_settings.setValue("keyboard/intervalMs", ms);
}

int SettingsManager::keyboardPressDuration() const
{
    return m_settings.value("keyboard/pressDurationMs", 50).toInt();
}

void SettingsManager::setKeyboardPressDuration(int ms)
{
    m_settings.setValue("keyboard/pressDurationMs", ms);
}

int SettingsManager::keyboardStartDelay() const
{
    return m_settings.value("keyboard/startDelayMs", 0).toInt();
}

void SettingsManager::setKeyboardStartDelay(int ms)
{
    m_settings.setValue("keyboard/startDelayMs", ms);
}

int SettingsManager::keyboardRepeatCount() const
{
    return m_settings.value("keyboard/repeatCount", 1).toInt();
}

void SettingsManager::setKeyboardRepeatCount(int count)
{
    m_settings.setValue("keyboard/repeatCount", count);
}

bool SettingsManager::keyboardInfinite() const
{
    return m_settings.value("keyboard/infinite", false).toBool();
}

void SettingsManager::setKeyboardInfinite(bool infinite)
{
    m_settings.setValue("keyboard/infinite", infinite);
}

int SettingsManager::keyboardInputMode() const
{
    return m_settings.value("keyboard/inputMode", static_cast<int>(KeyInputMode::Normal)).toInt();
}

void SettingsManager::setKeyboardInputMode(int mode)
{
    m_settings.setValue("keyboard/inputMode", mode);
}

int SettingsManager::keyboardQtKey() const
{
    return m_settings.value("keyboard/qtKey", 0).toInt();
}

void SettingsManager::setKeyboardQtKey(int key)
{
    m_settings.setValue("keyboard/qtKey", key);
}

QString SettingsManager::keyboardKeyDisplay() const
{
    return m_settings.value("keyboard/keyDisplay", "").toString();
}

void SettingsManager::setKeyboardKeyDisplay(const QString& display)
{
    m_settings.setValue("keyboard/keyDisplay", display);
}

// ============ 快捷键 ============
QByteArray SettingsManager::hotkeyToBytes(const HotkeyInfo& hk)
{
    QByteArray data;
    data.append(reinterpret_cast<const char*>(&hk.key), sizeof(hk.key));
    data.append(reinterpret_cast<const char*>(&hk.ctrl), sizeof(hk.ctrl));
    data.append(reinterpret_cast<const char*>(&hk.shift), sizeof(hk.shift));
    data.append(reinterpret_cast<const char*>(&hk.alt), sizeof(hk.alt));
    data.append(reinterpret_cast<const char*>(&hk.win), sizeof(hk.win));
    return data;
}

HotkeyInfo SettingsManager::hotkeyFromBytes(const QByteArray& data)
{
    HotkeyInfo hk;
    if (data.size() >= static_cast<int>(sizeof(int) + 4 * sizeof(bool))) {
        const char* p = data.constData();
        memcpy(&hk.key, p, sizeof(hk.key)); p += sizeof(hk.key);
        memcpy(&hk.ctrl, p, sizeof(hk.ctrl)); p += sizeof(hk.ctrl);
        memcpy(&hk.shift, p, sizeof(hk.shift)); p += sizeof(hk.shift);
        memcpy(&hk.alt, p, sizeof(hk.alt)); p += sizeof(hk.alt);
        memcpy(&hk.win, p, sizeof(hk.win));
    }
    return hk;
}

HotkeyInfo SettingsManager::mouseStartHotkey() const
{
    return hotkeyFromBytes(m_settings.value("hotkey/mouseStart").toByteArray());
}

void SettingsManager::setMouseStartHotkey(const HotkeyInfo& hk)
{
    m_settings.setValue("hotkey/mouseStart", hotkeyToBytes(hk));
}

HotkeyInfo SettingsManager::mouseStopHotkey() const
{
    return hotkeyFromBytes(m_settings.value("hotkey/mouseStop").toByteArray());
}

void SettingsManager::setMouseStopHotkey(const HotkeyInfo& hk)
{
    m_settings.setValue("hotkey/mouseStop", hotkeyToBytes(hk));
}

HotkeyInfo SettingsManager::keyboardStartHotkey() const
{
    return hotkeyFromBytes(m_settings.value("hotkey/keyboardStart").toByteArray());
}

void SettingsManager::setKeyboardStartHotkey(const HotkeyInfo& hk)
{
    m_settings.setValue("hotkey/keyboardStart", hotkeyToBytes(hk));
}

HotkeyInfo SettingsManager::keyboardStopHotkey() const
{
    return hotkeyFromBytes(m_settings.value("hotkey/keyboardStop").toByteArray());
}

void SettingsManager::setKeyboardStopHotkey(const HotkeyInfo& hk)
{
    m_settings.setValue("hotkey/keyboardStop", hotkeyToBytes(hk));
}

// ============ 脚本 ============
QString SettingsManager::scriptDefaultDir() const
{
    return m_settings.value("script/defaultDir",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
}

void SettingsManager::setScriptDefaultDir(const QString& dir)
{
    m_settings.setValue("script/defaultDir", dir);
}

QStringList SettingsManager::recentScripts() const
{
    return m_settings.value("script/recentScripts").toStringList();
}

void SettingsManager::setRecentScripts(const QStringList& scripts)
{
    m_settings.setValue("script/recentScripts", scripts);
}

void SettingsManager::addRecentScript(const QString& path)
{
    QStringList scripts = recentScripts();
    scripts.removeAll(path);
    scripts.prepend(path);
    if (scripts.size() > 10) scripts = scripts.mid(0, 10);
    setRecentScripts(scripts);
}

double SettingsManager::scriptPlaybackSpeed() const
{
    return m_settings.value("script/playbackSpeed", 1.0).toDouble();
}

void SettingsManager::setScriptPlaybackSpeed(double speed)
{
    m_settings.setValue("script/playbackSpeed", speed);
}

int SettingsManager::scriptRepeatCount() const
{
    return m_settings.value("script/repeatCount", 1).toInt();
}

void SettingsManager::setScriptRepeatCount(int count)
{
    m_settings.setValue("script/repeatCount", count);
}

// ============ 系统托盘 ============
int SettingsManager::trayCloseBehavior() const
{
    return m_settings.value("tray/closeBehavior",
        static_cast<int>(TrayCloseBehavior::AskUser)).toInt();
}

void SettingsManager::setTrayCloseBehavior(int behavior)
{
    m_settings.setValue("tray/closeBehavior", behavior);
}

bool SettingsManager::minimizeToTray() const
{
    return m_settings.value("tray/minimizeToTray", true).toBool();
}

void SettingsManager::setMinimizeToTray(bool enable)
{
    m_settings.setValue("tray/minimizeToTray", enable);
}

// ============ 通用 ============
CoordinateMode SettingsManager::defaultCoordinateMode() const
{
    int v = m_settings.value("general/defaultCoordMode",
        static_cast<int>(CoordinateMode::MonitorRelative)).toInt();
    return static_cast<CoordinateMode>(v);
}

void SettingsManager::setDefaultCoordinateMode(CoordinateMode mode)
{
    m_settings.setValue("general/defaultCoordMode", static_cast<int>(mode));
}

void SettingsManager::sync()
{
    m_settings.sync();
}
