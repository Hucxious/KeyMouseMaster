#ifndef WINDOWSHOOKMANAGER_H
#define WINDOWSHOOKMANAGER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QPoint>
#include <atomic>
#include <memory>

#include "ScriptEvent.h"
#include "AppTypes.h"
#include "WinCompat.h"

// ============================================================================
// Windows 全局钩子管理器
// 使用 WH_MOUSE_LL 和 WH_KEYBOARD_LL 录制键鼠事件
// 钩子回调只负责轻量数据提取，放入线程安全队列由工作线程处理
// ============================================================================

// 原始钩子事件 (轻量，在回调中创建)
struct RawHookEvent
{
    enum Type { Mouse, Keyboard };

    Type   rawType;
    int64_t timestampMs;

    // 鼠标
    int    mouseX = 0;
    int    mouseY = 0;
    DWORD  mouseFlags = 0;
    DWORD  mouseData = 0;
    ULONG_PTR extraInfo = 0;

    // 键盘
    DWORD  vkCode = 0;
    DWORD  scanCode = 0;
    DWORD  keyFlags = 0;  // 0 = down, 0x80 = up
    ULONG_PTR keyExtraInfo = 0;
};

class WindowsHookManager : public QObject
{
    Q_OBJECT

public:
    explicit WindowsHookManager(QObject* parent = nullptr);
    ~WindowsHookManager() override;

    // 安装/卸载钩子
    bool installMouseHook();
    bool installKeyboardHook();
    void uninstallMouseHook();
    void uninstallKeyboardHook();
    void uninstallAllHooks();
    bool isMouseHookInstalled() const { return m_mouseHookInstalled; }
    bool isKeyboardHookInstalled() const { return m_keyboardHookInstalled; }

    // 录制控制
    void startRecording(const RecordingSettings& settings);
    void stopRecording();
    bool isRecording() const { return m_recording.load(); }

    // 获取录制的事件列表 (在工作线程中处理后的结果)
    QVector<ScriptEvent> takeRecordedEvents();

    // 设置忽略本软件窗口的HWND
    void setOwnWindowHandle(void* hwnd);
    void setIgnoreSimulatedInput(bool ignore);

signals:
    void recordingStarted();
    void recordingStopped();
    void hookError(const QString& message);
    void rawEventReady();  // 通知工作线程有新的原始事件

private:
    // Windows 钩子回调 (静态函数，桥接到实例)
#ifdef Q_OS_WIN
    static LRESULT CALLBACK mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
#endif

    // 事件处理工作线程
    Q_INVOKABLE void processRawEvents();
    ScriptEvent convertMouseEvent(const RawHookEvent& raw);
    ScriptEvent convertKeyboardEvent(const RawHookEvent& raw);
    bool shouldRecordMouseMove(const ScriptEvent& last, const ScriptEvent& current) const;

    // 钩子句柄
#ifdef Q_OS_WIN
    HHOOK m_mouseHook = nullptr;
    HHOOK m_keyboardHook = nullptr;
#else
    void* m_mouseHook = nullptr;
    void* m_keyboardHook = nullptr;
#endif
    bool m_mouseHookInstalled = false;
    bool m_keyboardHookInstalled = false;

    // 录制状态
    std::atomic_bool m_recording{false};
    std::atomic_bool m_stopProcessing{false};
    RecordingSettings m_settings;
    int64_t m_recordingStartTime = 0;

    // 原始事件队列 (线程安全)
    QMutex m_rawQueueMutex;
    QQueue<RawHookEvent> m_rawEventQueue;
    QWaitCondition m_rawEventCondition;

    // 处理后的事件列表
    QMutex m_processedMutex;
    QVector<ScriptEvent> m_processedEvents;

    // 工作线程
    QThread* m_workerThread = nullptr;

    // 本软件窗口句柄
#ifdef Q_OS_WIN
    HWND m_ownHwnd = nullptr;
#else
    void* m_ownHwnd = nullptr;
#endif
    bool m_ignoreSimulated = true;

    // 上一次鼠标位置 (用于压缩)
    QPoint m_lastRecordedMousePos;
    int64_t m_lastMouseMoveTime = 0;

    // 配置中的鼠标按下时记录的修饰键状态
    uint32_t m_currentModifiers = 0;

    // 全局实例指针 (用于静态回调)
    static WindowsHookManager* s_instance;
};

#endif // WINDOWSHOOKMANAGER_H
