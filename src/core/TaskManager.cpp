#include "TaskManager.h"
#include "MouseClickEngine.h"
#include "KeyboardClickEngine.h"
#include "ScriptPlayer.h"
#include "ScriptRecorder.h"
#include "Logger.h"

TaskManager::TaskManager(QObject* parent)
    : QObject(parent)
{
}

void TaskManager::setMouseClickEngine(MouseClickEngine* engine)
{
    m_mouseEngine = engine;
    if (engine) {
        connect(engine, &MouseClickEngine::finished,
                this, &TaskManager::onEngineFinished);
        connect(engine, &MouseClickEngine::stateChanged,
                this, [this](TaskState state) {
                    if (state == TaskState::MouseClicking
                        || state == TaskState::Preparing) {
                        // do nothing, keep current
                    }
                });
    }
}

void TaskManager::setKeyboardClickEngine(KeyboardClickEngine* engine)
{
    m_keyboardEngine = engine;
    if (engine) {
        connect(engine, &KeyboardClickEngine::finished,
                this, &TaskManager::onEngineFinished);
    }
}

void TaskManager::setScriptPlayer(ScriptPlayer* player)
{
    m_player = player;
    if (player) {
        connect(player, &ScriptPlayer::finished,
                this, &TaskManager::onEngineFinished);
    }
}

void TaskManager::setScriptRecorder(ScriptRecorder* recorder)
{
    m_recorder = recorder;
    if (recorder) {
        connect(recorder, &ScriptRecorder::recordingStopped,
                this, &TaskManager::onEngineFinished);
    }
}

bool TaskManager::isAnyTaskRunning() const
{
    return m_currentState == TaskState::MouseClicking
        || m_currentState == TaskState::KeyboardClicking
        || m_currentState == TaskState::Recording
        || m_currentState == TaskState::Playing
        || m_currentState == TaskState::Paused
        || m_currentState == TaskState::Preparing
        || m_currentState == TaskState::Stopping;
}

bool TaskManager::canStartTask() const
{
    return m_currentState == TaskState::Idle
        || m_currentState == TaskState::Completed
        || m_currentState == TaskState::Error;
}

bool TaskManager::tryStartTask(TaskState newState, QString* errorMsg)
{
    QMutexLocker locker(&m_stateMutex);

    if (!canStartTask()) {
        if (errorMsg)
            *errorMsg = QString("当前有任务正在运行 (%1)，请先停止")
                .arg(taskStateToString(m_currentState));
        return false;
    }

    m_currentState = newState;
    emit taskStateChanged(newState);
    return true;
}

bool TaskManager::requestStartMouseClick(QString* errorMsg)
{
    if (!m_mouseEngine) {
        if (errorMsg) *errorMsg = "鼠标连点引擎未初始化";
        return false;
    }
    if (!tryStartTask(TaskState::MouseClicking, errorMsg))
        return false;

    LOG_INFO("启动鼠标连点任务");
    emit taskStarted("鼠标连点");
    m_mouseEngine->start();
    return true;
}

bool TaskManager::requestStartKeyboardClick(QString* errorMsg)
{
    if (!m_keyboardEngine) {
        if (errorMsg) *errorMsg = "键盘连点引擎未初始化";
        return false;
    }
    if (!tryStartTask(TaskState::KeyboardClicking, errorMsg))
        return false;

    LOG_INFO("启动键盘连点任务");
    emit taskStarted("键盘连点");
    m_keyboardEngine->start();
    return true;
}

bool TaskManager::requestStartRecording(const RecordingSettings& settings,
                                          QString* errorMsg)
{
    if (!m_recorder) {
        if (errorMsg) *errorMsg = "脚本录制器未初始化";
        return false;
    }
    if (!tryStartTask(TaskState::Recording, errorMsg))
        return false;

    LOG_INFO("启动脚本录制任务");
    emit taskStarted("脚本录制");

    if (!m_recorder->startRecording(settings)) {
        m_currentState = TaskState::Error;
        if (errorMsg) *errorMsg = "启动录制失败";
        emit taskError("启动录制失败");
        return false;
    }
    return true;
}

bool TaskManager::requestStartPlayback(const ScriptDocument& doc,
                                         const PlaybackSettings& settings,
                                         QString* errorMsg)
{
    if (!m_player) {
        if (errorMsg) *errorMsg = "脚本播放器未初始化";
        return false;
    }
    if (!tryStartTask(TaskState::Playing, errorMsg))
        return false;

    LOG_INFO("启动脚本回放任务");
    emit taskStarted("脚本回放");

    m_player->setDocument(doc);
    m_player->setPlaybackSettings(settings);
    m_player->start();
    return true;
}

void TaskManager::requestStop()
{
    LOG_INFO("请求停止当前任务");

    switch (m_currentState) {
    case TaskState::MouseClicking:
        if (m_mouseEngine) m_mouseEngine->requestStop();
        break;
    case TaskState::KeyboardClicking:
        if (m_keyboardEngine) m_keyboardEngine->requestStop();
        break;
    case TaskState::Recording:
        if (m_recorder) m_recorder->stopRecording();
        break;
    case TaskState::Playing:
    case TaskState::Paused:
        if (m_player) m_player->requestStop();
        break;
    default:
        break;
    }

    m_currentState = TaskState::Stopping;
    emit taskStateChanged(m_currentState);
}

void TaskManager::emergencyStop()
{
    LOG_INFO("紧急停止!");

    // 强制停止所有引擎
    if (m_mouseEngine) m_mouseEngine->stop();
    if (m_keyboardEngine) m_keyboardEngine->stop();
    if (m_player) m_player->stop();
    if (m_recorder) m_recorder->stopRecording();

    m_currentState = TaskState::Idle;
    emit taskStateChanged(m_currentState);
    emit taskStopped();
}

void TaskManager::requestPause()
{
    if (m_currentState == TaskState::Playing && m_player) {
        m_player->pause();
    }
}

void TaskManager::requestResume()
{
    if (m_currentState == TaskState::Paused && m_player) {
        m_player->resume();
    }
}

void TaskManager::onEngineFinished()
{
    QMutexLocker locker(&m_stateMutex);
    m_currentState = TaskState::Idle;
    emit taskStateChanged(m_currentState);
    emit taskStopped();
}
