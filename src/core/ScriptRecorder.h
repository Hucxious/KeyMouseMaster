#ifndef SCRIPTRECORDER_H
#define SCRIPTRECORDER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <atomic>
#include "ScriptDocument.h"
#include "MonitorInfo.h"

class WindowsHookManager;
class MonitorManager;

// ============================================================================
// 脚本录制器
// 管理录制生命周期，将钩子捕获的事件处理后组装成 ScriptDocument
// ============================================================================
class ScriptRecorder : public QObject
{
    Q_OBJECT

public:
    explicit ScriptRecorder(WindowsHookManager* hookManager,
                             MonitorManager* monitorMgr,
                             QObject* parent = nullptr);
    ~ScriptRecorder() override;

    // 开始/停止录制
    bool startRecording(const RecordingSettings& settings);
    void stopRecording();
    bool isRecording() const { return m_recording.load(); }

    // 获取录制结果
    ScriptDocument takeDocument();

    // 是否录制了事件
    bool hasEvents() const;

    TaskState state() const { return m_state; }

signals:
    void recordingStarted();
    void recordingStopped();
    void stateChanged(TaskState state);
    void eventCountChanged(int count);
    void errorOccurred(const QString& message);

private slots:
    void onHookRecordingStopped();
    void processRecordedEvents();

private:
    void enrichEventsWithMonitorInfo(QVector<ScriptEvent>& events);
    void removeTrailingHotkeyEvents(QVector<ScriptEvent>& events);

    WindowsHookManager* m_hookManager;
    MonitorManager* m_monitorMgr;

    std::atomic_bool m_recording{false};
    TaskState m_state = TaskState::Idle;
    RecordingSettings m_settings;
    ScriptDocument m_document;
    QMutex m_documentMutex;
};

#endif // SCRIPTRECORDER_H
