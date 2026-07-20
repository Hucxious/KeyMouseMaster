#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include <QObject>
#include <QMutex>
#include "AppTypes.h"
#include "ScriptEvent.h"
#include "ScriptDocument.h"

class MouseClickEngine;
class KeyboardClickEngine;
class ScriptPlayer;
class ScriptRecorder;

// ============================================================================
// 任务管理器
// 统一管理所有自动化任务，确保同一时间只有一个任务运行
// ============================================================================
class TaskManager : public QObject
{
    Q_OBJECT

public:
    explicit TaskManager(QObject* parent = nullptr);

    // 注册子引擎
    void setMouseClickEngine(MouseClickEngine* engine);
    void setKeyboardClickEngine(KeyboardClickEngine* engine);
    void setScriptPlayer(ScriptPlayer* player);
    void setScriptRecorder(ScriptRecorder* recorder);

    // 状态查询
    TaskState currentState() const { return m_currentState; }
    bool isAnyTaskRunning() const;
    bool canStartTask() const;

    // 启动任务 (返回是否成功，失败时设置errorMsg)
    bool requestStartMouseClick(QString* errorMsg = nullptr);
    bool requestStartKeyboardClick(QString* errorMsg = nullptr);
    bool requestStartRecording(const RecordingSettings& settings,
                                QString* errorMsg = nullptr);
    bool requestStartPlayback(const ScriptDocument& doc,
                               const PlaybackSettings& settings,
                               QString* errorMsg = nullptr);

    // 停止当前任务
    void requestStop();
    void emergencyStop();

    // 暂停/继续 (仅回放)
    void requestPause();
    void requestResume();

signals:
    void taskStateChanged(TaskState state);
    void taskStarted(const QString& description);
    void taskStopped();
    void taskError(const QString& message);

private:
    bool tryStartTask(TaskState newState, QString* errorMsg);
    void onEngineFinished();

    MouseClickEngine* m_mouseEngine = nullptr;
    KeyboardClickEngine* m_keyboardEngine = nullptr;
    ScriptPlayer* m_player = nullptr;
    ScriptRecorder* m_recorder = nullptr;

    TaskState m_currentState = TaskState::Idle;
    QMutex m_stateMutex;
};

#endif // TASKMANAGER_H
