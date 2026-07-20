#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>
#include <QSize>
#include <QPoint>
#include "AppTypes.h"

// ============================================================================
// 应用配置管理器 (基于 QSettings)
// ============================================================================
class SettingsManager : public QObject
{
    Q_OBJECT

public:
    explicit SettingsManager(QObject* parent = nullptr);

    // 窗口状态
    QSize windowSize() const;
    void setWindowSize(const QSize& size);
    QPoint windowPosition() const;
    void setWindowPosition(const QPoint& pos);
    int currentTabIndex() const;
    void setCurrentTabIndex(int index);

    // 鼠标连点参数
    int mouseClickInterval() const;
    void setMouseClickInterval(int ms);
    int mouseClickIntervalUnit() const; // 0=ms, 1=s
    void setMouseClickIntervalUnit(int unit);
    int mousePressDuration() const;
    void setMousePressDuration(int ms);
    int mouseDoubleClickInterval() const;
    void setMouseDoubleClickInterval(int ms);
    int mouseStartDelay() const;
    void setMouseStartDelay(int ms);
    int mouseRepeatCount() const;
    void setMouseRepeatCount(int count);
    bool mouseInfinite() const;
    void setMouseInfinite(bool infinite);
    bool mouseRestoreCursor() const;
    void setMouseRestoreCursor(bool restore);
    int mouseButton() const; // MouseButton enum
    void setMouseButton(int btn);
    int mouseClickMode() const; // ClickMode enum
    void setMouseClickMode(int mode);
    CoordinateMode mouseCoordinateMode() const;
    void setMouseCoordinateMode(CoordinateMode mode);
    QPoint mouseFixedPos() const;
    void setMouseFixedPos(const QPoint& pos);
    QString mouseMonitorDevice() const;
    void setMouseMonitorDevice(const QString& device);

    // 键盘连点参数
    int keyboardInterval() const;
    void setKeyboardInterval(int ms);
    int keyboardPressDuration() const;
    void setKeyboardPressDuration(int ms);
    int keyboardStartDelay() const;
    void setKeyboardStartDelay(int ms);
    int keyboardRepeatCount() const;
    void setKeyboardRepeatCount(int count);
    bool keyboardInfinite() const;
    void setKeyboardInfinite(bool infinite);
    int keyboardInputMode() const;
    void setKeyboardInputMode(int mode);
    int keyboardQtKey() const;
    void setKeyboardQtKey(int key);
    QString keyboardKeyDisplay() const;
    void setKeyboardKeyDisplay(const QString& display);

    // 快捷键
    HotkeyInfo mouseStartHotkey() const;
    void setMouseStartHotkey(const HotkeyInfo& hk);
    HotkeyInfo mouseStopHotkey() const;
    void setMouseStopHotkey(const HotkeyInfo& hk);
    HotkeyInfo keyboardStartHotkey() const;
    void setKeyboardStartHotkey(const HotkeyInfo& hk);
    HotkeyInfo keyboardStopHotkey() const;
    void setKeyboardStopHotkey(const HotkeyInfo& hk);

    // 脚本
    QString scriptDefaultDir() const;
    void setScriptDefaultDir(const QString& dir);
    QStringList recentScripts() const;
    void setRecentScripts(const QStringList& scripts);
    void addRecentScript(const QString& path);
    double scriptPlaybackSpeed() const;
    void setScriptPlaybackSpeed(double speed);
    int scriptRepeatCount() const;
    void setScriptRepeatCount(int count);

    // 系统托盘
    int trayCloseBehavior() const;
    void setTrayCloseBehavior(int behavior);
    bool minimizeToTray() const;
    void setMinimizeToTray(bool enable);

    // 通用
    CoordinateMode defaultCoordinateMode() const;
    void setDefaultCoordinateMode(CoordinateMode mode);

    // 同步写入
    void sync();

signals:
    void settingsChanged();

private:
    QSettings m_settings;

    // 序列化辅助
    static QByteArray hotkeyToBytes(const HotkeyInfo& hk);
    static HotkeyInfo hotkeyFromBytes(const QByteArray& data);
};

#endif // SETTINGSMANAGER_H
