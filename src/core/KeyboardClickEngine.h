#ifndef KEYBOARDCLICKENGINE_H
#define KEYBOARDCLICKENGINE_H

#include <QObject>
#include <QThread>
#include <atomic>
#include "AppTypes.h"

class WindowsInputSimulator;

// ============================================================================
// 键盘连点引擎
// 在工作线程中运行，支持普通按键与长按（均可携带修饰键）
// ============================================================================
class KeyboardClickEngine : public QObject
{
    Q_OBJECT

public:
    struct Config {
        // 按键信息
        int      qtKey = 0;
        uint32_t winVk = 0;
        uint32_t scanCode = 0;
        bool     isExtended = false;
        QString  displayName;

        // 修饰键
        bool hasCtrl  = false;
        bool hasShift = false;
        bool hasAlt   = false;
        bool hasWin   = false;

        // 输入模式
        KeyInputMode inputMode = KeyInputMode::Normal;

        // 时间参数
        int intervalMs = 100;
        int pressDurationMs = 50;
        int startDelayMs = 0;

        // 执行参数
        int  repeatCount = 1;
        bool infiniteRepeat = false;
    };

    explicit KeyboardClickEngine(WindowsInputSimulator* simulator,
                                  QObject* parent = nullptr);
    ~KeyboardClickEngine() override;

    void setConfig(const Config& config);
    Config config() const { return m_config; }

    void start();
    void stop();
    void requestStop();
    bool isRunning() const { return m_running.load(); }

    int currentCount() const { return m_currentCount.load(); }
    int totalCount() const;
    TaskState state() const { return m_state.load(); }

signals:
    void started();
    void stopped();
    void countChanged(int current, int total);
    void stateChanged(TaskState state);
    void errorOccurred(const QString& message);
    void finished();

private:
    void runLoop();
    void executeKeyAction();
    void releaseAllModifiers(); // 释放所有修饰键

    WindowsInputSimulator* m_simulator;
    Config m_config;

    std::atomic_bool m_stopRequested{false};
    std::atomic_bool m_running{false};
    std::atomic_int  m_currentCount{0};
    std::atomic<TaskState> m_state{TaskState::Idle};

    // 记录的修饰键状态，用于停止时释放
    std::atomic_bool m_ctrlDown{false};
    std::atomic_bool m_shiftDown{false};
    std::atomic_bool m_altDown{false};
    std::atomic_bool m_winDown{false};

    QThread* m_workerThread = nullptr;
    quint64 m_generation = 0;
};

#endif // KEYBOARDCLICKENGINE_H
