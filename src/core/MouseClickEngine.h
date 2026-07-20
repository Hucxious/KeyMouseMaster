#ifndef MOUSECLICKENGINE_H
#define MOUSECLICKENGINE_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <atomic>
#include "AppTypes.h"
#include "MonitorInfo.h"

class WindowsInputSimulator;
class MonitorManager;

// ============================================================================
// 鼠标连点引擎
// 在工作线程中运行，支持多种点击模式和坐标模式
// ============================================================================
class MouseClickEngine : public QObject
{
    Q_OBJECT

public:
    // 配置参数
    struct Config {
        MouseButton button = MouseButton::Left;
        ClickMode   clickMode = ClickMode::Single;

        // 坐标
        CoordinateMode coordMode = CoordinateMode::CurrentCursor;
        QPoint  fixedVirtualPos;     // 固定虚拟桌面坐标
        QString monitorDeviceName;   // 目标显示器
        QPoint  monitorInternalPos;   // 显示器内部坐标
        QPointF monitorRatioPos;      // 显示器比例坐标

        // 时间参数
        int intervalMs = 100;
        int pressDurationMs = 50;
        int doubleClickIntervalMs = 100;
        int startDelayMs = 0;

        // 执行参数
        int  repeatCount = 1;
        bool infiniteRepeat = false;
        bool restoreCursor = false;
    };

    explicit MouseClickEngine(WindowsInputSimulator* simulator,
                               MonitorManager* monitorMgr,
                               QObject* parent = nullptr);
    ~MouseClickEngine() override;

    void setConfig(const Config& config);
    Config config() const { return m_config; }

    // 控制
    void start();
    void stop();
    void requestStop();
    bool isRunning() const { return m_running.load(); }

    // 统计
    int currentCount() const { return m_currentCount.load(); }
    int totalCount() const;
    TaskState state() const { return m_state; }

signals:
    void started();
    void stopped();
    void countChanged(int current, int total);
    void stateChanged(TaskState state);
    void errorOccurred(const QString& message);
    void finished();

private slots:
    void executeClick();

private:
    void runLoop();
    bool resolveTargetPos(QPoint& outPos);

    WindowsInputSimulator* m_simulator;
    MonitorManager* m_monitorMgr;
    Config m_config;

    std::atomic_bool m_stopRequested{false};
    std::atomic_bool m_running{false};
    std::atomic_int  m_currentCount{0};
    TaskState m_state = TaskState::Idle;

    QThread* m_workerThread = nullptr;
    QPoint m_originalCursorPos;  // 执行前光标位置
};

#endif // MOUSECLICKENGINE_H
