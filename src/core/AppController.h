#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QThread>
#include <memory>

#include "AppTypes.h"
#include "ScriptDocument.h"

// Forward declarations
class WindowsInputSimulator;
class WindowsHookManager;
class WindowsHotkeyManager;
class WindowsMonitorBackend;
class MonitorManager;
class CoordinateMapper;
class InputStateManager;
class TaskManager;
class MouseClickEngine;
class KeyboardClickEngine;
class ScriptRecorder;
class ScriptPlayer;
class ScriptSerializer;
class SettingsManager;
class Logger;

// ============================================================================
// 应用控制器
// 协调所有子系统，是应用的核心枢纽
// ============================================================================
class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    // 初始化所有子系统
    bool initialize();

    // 子系统访问器
    WindowsInputSimulator* inputSimulator()    const { return m_inputSimulator; }
    WindowsHookManager*    hookManager()       const { return m_hookManager; }
    WindowsHotkeyManager*  hotkeyManager()     const { return m_hotkeyManager; }
    MonitorManager*        monitorManager()    const { return m_monitorManager; }
    CoordinateMapper*      coordinateMapper()  const { return m_coordinateMapper; }
    InputStateManager*     inputStateManager() const { return m_inputStateManager; }
    TaskManager*           taskManager()       const { return m_taskManager; }
    MouseClickEngine*      mouseClickEngine()  const { return m_mouseClickEngine; }
    KeyboardClickEngine*   keyboardClickEngine() const { return m_keyboardClickEngine; }
    ScriptRecorder*        scriptRecorder()    const { return m_scriptRecorder; }
    ScriptPlayer*          scriptPlayer()      const { return m_scriptPlayer; }
    ScriptSerializer*      scriptSerializer()  const { return m_scriptSerializer; }
    SettingsManager*       settingsManager()   const { return m_settingsManager; }

    // 全局快捷键ID常量
    enum HotkeyId {
        HK_EMERGENCY_STOP = 1,
        HK_MOUSE_START,
        HK_MOUSE_STOP,
        HK_KEYBOARD_START,
        HK_KEYBOARD_STOP,
        HK_RECORDING_START,
        HK_RECORDING_STOP,
        HK_PLAYBACK_START,
        HK_PLAYBACK_STOP
    };

public slots:
    // 全局操作
    void emergencyStop();
    void resetInputState();
    void registerAllHotkeys();
    void unregisterAllHotkeys();

signals:
    void initialized();
    void initializationError(const QString& message);
    void emergencyStopTriggered();
    void inputStateReset();
    void statusMessage(const QString& message);

private slots:
    void onGlobalHotkeyPressed(int id);
    void onMonitorsChanged();
    void onTaskStateChanged(TaskState state);

private:
    void createSubsystems();
    void connectSignals();

    // 子系统指针 (由Qt父子对象管理生命周期)
    WindowsInputSimulator* m_inputSimulator = nullptr;
    WindowsHookManager*    m_hookManager = nullptr;
    WindowsHotkeyManager*  m_hotkeyManager = nullptr;
    MonitorManager*        m_monitorManager = nullptr;
    CoordinateMapper*      m_coordinateMapper = nullptr;
    InputStateManager*     m_inputStateManager = nullptr;
    TaskManager*           m_taskManager = nullptr;
    MouseClickEngine*      m_mouseClickEngine = nullptr;
    KeyboardClickEngine*   m_keyboardClickEngine = nullptr;
    ScriptRecorder*        m_scriptRecorder = nullptr;
    ScriptPlayer*          m_scriptPlayer = nullptr;
    ScriptSerializer*      m_scriptSerializer = nullptr;
    SettingsManager*       m_settingsManager = nullptr;

    // 已注册的热键ID列表
    QVector<int> m_registeredHotkeyIds;
};

#endif // APPCONTROLLER_H
