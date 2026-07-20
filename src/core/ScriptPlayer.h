#ifndef SCRIPTPLAYER_H
#define SCRIPTPLAYER_H

#include <QObject>
#include <QThread>
#include <atomic>
#include "ScriptDocument.h"
#include "MonitorInfo.h"

class WindowsInputSimulator;
class MonitorManager;

// ============================================================================
// 脚本回放器
// 在工作线程中按时间戳顺序播放脚本事件，支持多显示器坐标解析
// ============================================================================
class ScriptPlayer : public QObject
{
    Q_OBJECT

public:
    explicit ScriptPlayer(WindowsInputSimulator* simulator,
                           MonitorManager* monitorMgr,
                           QObject* parent = nullptr);
    ~ScriptPlayer() override;

    // 设置要回放的脚本
    void setDocument(const ScriptDocument& doc);
    ScriptDocument document() const { return m_document; }

    // 设置回放参数
    void setPlaybackSettings(const PlaybackSettings& settings);
    PlaybackSettings playbackSettings() const { return m_playbackSettings; }

    // 控制
    void start();
    void stop();
    void requestStop();
    void pause();
    void resume();
    bool isRunning() const { return m_running.load(); }
    bool isPaused() const { return m_paused.load(); }

    // 进度
    int currentEventIndex() const { return m_currentEventIndex.load(); }
    int totalEvents() const { return m_document.eventCount(); }
    int currentRound() const { return m_currentRound.load(); }
    TaskState state() const { return m_state; }

signals:
    void started();
    void stopped();
    void paused();
    void resumed();
    void progressChanged(int currentEvent, int totalEvents, int currentRound);
    void stateChanged(TaskState state);
    void errorOccurred(const QString& message);
    void finished();

private:
    void runLoop();
    bool playEvent(const ScriptEvent& ev);
    bool playMouseEvent(const ScriptEvent& ev);
    bool playKeyboardEvent(const ScriptEvent& ev);
    bool resolvePlaybackCoordinate(const ScriptEvent& ev, QPoint& outVirtualPt);
    int64_t calculateWaitTime(int currentEventIndex, int64_t startTimeMs,
                               int64_t roundStartOffset);

    WindowsInputSimulator* m_simulator;
    MonitorManager* m_monitorMgr;
    ScriptDocument m_document;
    PlaybackSettings m_playbackSettings;

    std::atomic_bool m_stopRequested{false};
    std::atomic_bool m_running{false};
    std::atomic_bool m_paused{false};
    std::atomic_int  m_currentEventIndex{0};
    std::atomic_int  m_currentRound{0};
    TaskState m_state = TaskState::Idle;

    QPoint m_playbackStartCursor;
};

#endif // SCRIPTPLAYER_H
