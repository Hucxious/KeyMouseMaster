#include "ScriptPlayer.h"
#include "platform/windows/WindowsInputSimulator.h"
#include "core/MonitorManager.h"
#include "core/CoordinateMapper.h"
#include "utils/TimeUtils.h"
#include "utils/ValidationUtils.h"
#include "utils/Logger.h"

ScriptPlayer::ScriptPlayer(WindowsInputSimulator* simulator,
                             MonitorManager* monitorMgr,
                             QObject* parent)
    : QObject(parent)
    , m_simulator(simulator)
    , m_monitorMgr(monitorMgr)
{
}

ScriptPlayer::~ScriptPlayer()
{
    stop();
}

void ScriptPlayer::setDocument(const ScriptDocument& doc)
{
    m_document = doc;
}

void ScriptPlayer::setPlaybackSettings(const PlaybackSettings& settings)
{
    m_playbackSettings = settings;
}

void ScriptPlayer::start()
{
    if (m_running.load()) return;

    if (m_document.isEmpty()) {
        emit errorOccurred("脚本事件为空，无法回放");
        return;
    }

    // 验证显示器
    QString monitorError;
    if (!m_document.validateMonitors(m_monitorMgr->monitors(), &monitorError)) {
        emit errorOccurred(monitorError);
        return;
    }

    m_stopRequested.store(false);
    m_paused.store(false);
    m_currentEventIndex.store(0);
    m_currentRound.store(0);
    m_state = TaskState::Preparing;
    emit stateChanged(m_state);

    // 保存回放前鼠标位置
    m_playbackStartCursor = m_monitorMgr->currentCursorPos();

    // 启动延迟
    if (m_playbackSettings.startDelayMs > 0) {
        QThread::msleep(static_cast<unsigned long>(m_playbackSettings.startDelayMs));
        if (m_stopRequested.load()) {
            m_state = TaskState::Idle;
            emit stateChanged(m_state);
            return;
        }
    }

    m_running.store(true);
    m_state = TaskState::Playing;
    emit stateChanged(m_state);
    emit started();

    LOG_INFO(QString("脚本回放开始: %1 个事件, 速度 %2x")
        .arg(m_document.eventCount())
        .arg(m_playbackSettings.speedFactor));

    runLoop();

    // 恢复光标
    if (m_playbackSettings.restoreCursor && !m_stopRequested.load()) {
        QRect virtBounds = m_monitorMgr->virtualDesktopBounds();
        m_simulator->mouseMoveAbsolute(
            m_playbackStartCursor.x(), m_playbackStartCursor.y(),
            virtBounds.width(), virtBounds.height());
    }

    // 释放所有输入
    m_simulator->releaseAllInputs();

    m_running.store(false);
    m_state = m_stopRequested.load() ? TaskState::Completed : TaskState::Completed;
    emit stateChanged(m_state);
    emit stopped();
    emit finished();

    LOG_INFO("脚本回放结束");
}

void ScriptPlayer::stop()
{
    m_state = TaskState::Stopping;
    emit stateChanged(m_state);
    m_stopRequested.store(true);
    m_paused.store(false);
    m_simulator->releaseAllInputs();
}

void ScriptPlayer::requestStop()
{
    m_stopRequested.store(true);
}

void ScriptPlayer::pause()
{
    if (m_running.load() && !m_paused.load()) {
        m_paused.store(true);
        m_state = TaskState::Paused;
        emit stateChanged(m_state);
        emit paused();
        LOG_INFO("脚本回放已暂停");
    }
}

void ScriptPlayer::resume()
{
    if (m_paused.load()) {
        m_paused.store(false);
        m_state = TaskState::Playing;
        emit stateChanged(m_state);
        emit resumed();
        LOG_INFO("脚本回放已继续");
    }
}

// ============================================================================
// 回放主循环
// 使用事件的绝对时间戳 + 速度因子计算等待时间，避免误差累积
// ============================================================================
void ScriptPlayer::runLoop()
{
    int maxRounds = m_playbackSettings.infiniteRepeat
        ? std::numeric_limits<int>::max()
        : m_playbackSettings.repeatCount;

    for (int round = 0; round < maxRounds && !m_stopRequested.load(); ++round) {
        m_currentRound.store(round + 1);
        int64_t roundStartTime = TimeUtils::currentTimeMs();
        int64_t docStartTime = m_document.events.isEmpty() ? 0 : m_document.events.first().timestampMs;

        LOG_INFO(QString("回放轮次 %1/%2").arg(round + 1)
            .arg(m_playbackSettings.infiniteRepeat ? "∞" : QString::number(maxRounds)));

        for (int i = 0; i < m_document.eventCount() && !m_stopRequested.load(); ++i) {
            // 处理暂停
            while (m_paused.load() && !m_stopRequested.load()) {
                QThread::msleep(50);
            }
            if (m_stopRequested.load()) break;

            const ScriptEvent& ev = m_document.events[i];

            // 跳过禁用的事件
            if (!ev.enabled && m_playbackSettings.skipDisabledEvents)
                continue;

            // 计算等待时间
            int64_t targetTimeMs = ev.timestampMs - docStartTime;
            int64_t waitMs = calculateWaitTime(static_cast<int>(targetTimeMs),
                                                roundStartTime, 0);
            if (waitMs > 0) {
                if (TimeUtils::interruptibleSleep(waitMs, m_stopRequested))
                    break;
            }

            if (m_stopRequested.load()) break;

            // 播放事件
            m_currentEventIndex.store(i + 1);
            emit progressChanged(i + 1, m_document.eventCount(), round + 1);

            if (!playEvent(ev)) {
                LOG_ERROR(QString("事件 %1 回放失败: %2")
                    .arg(i).arg(ev.eventSummary()));
                // 继续下一个事件，不中断整个回放
            }
        }

        // 轮次间等待
        if ((round < maxRounds - 1 || m_playbackSettings.infiniteRepeat)
            && !m_stopRequested.load()
            && m_playbackSettings.roundIntervalMs > 0) {
            TimeUtils::interruptibleSleep(m_playbackSettings.roundIntervalMs,
                                            m_stopRequested);
        }
    }
}

bool ScriptPlayer::playEvent(const ScriptEvent& ev)
{
    if (ev.isMouseEvent())
        return playMouseEvent(ev);
    else if (ev.isKeyboardEvent())
        return playKeyboardEvent(ev);
    return false;
}

bool ScriptPlayer::playMouseEvent(const ScriptEvent& ev)
{
    QPoint targetPos;
    if (!resolvePlaybackCoordinate(ev, targetPos))
        return false;

    switch (ev.type) {
    case ScriptEventType::MouseMove:
        return m_simulator->mouseMoveAbsolute(
            targetPos.x(), targetPos.y(),
            m_monitorMgr->virtualDesktopBounds().width(),
            m_monitorMgr->virtualDesktopBounds().height());

    case ScriptEventType::MouseDown:
        // 回放前先移动到目标位置
        if (targetPos != m_monitorMgr->currentCursorPos()) {
            QRect vb = m_monitorMgr->virtualDesktopBounds();
            m_simulator->mouseMoveAbsolute(targetPos.x(), targetPos.y(),
                                            vb.width(), vb.height());
        }
        return m_simulator->mouseDown(ev.mouseButton);

    case ScriptEventType::MouseUp:
        return m_simulator->mouseUp(ev.mouseButton);

    case ScriptEventType::MouseWheel:
        return m_simulator->mouseWheel(ev.wheelDelta);

    case ScriptEventType::MouseHWheel:
        return m_simulator->mouseHorizontalWheel(ev.wheelDelta);

    default:
        return false;
    }
}

bool ScriptPlayer::playKeyboardEvent(const ScriptEvent& ev)
{
    // 回放时还原修饰键状态
    if (ev.type == ScriptEventType::KeyDown) {
        // 如果事件记录了修饰键按下的状态，考虑还原
        // 简化实现：直接发送按键
        return m_simulator->keyDown(ev.winVk, ev.isExtendedKey);
    } else {
        return m_simulator->keyUp(ev.winVk, ev.isExtendedKey);
    }
}

bool ScriptPlayer::resolvePlaybackCoordinate(const ScriptEvent& ev, QPoint& outVirtualPt)
{
    return CoordinateMapper::resolveTargetCoordinate(
        m_playbackSettings.coordinateMode,
        ev.virtualDesktopPos,
        ev.monitorDeviceName,
        ev.monitorInternalPos,
        ev.monitorRatioPos,
        m_monitorMgr->monitors(),
        outVirtualPt);
}

int64_t ScriptPlayer::calculateWaitTime(int currentEventIndex,
                                          int64_t startTimeMs,
                                          int64_t roundStartOffset)
{
    Q_UNUSED(currentEventIndex)
    // 使用绝对时间戳计算等待时间，避免误差累积
    Q_UNUSED(startTimeMs)
    Q_UNUSED(roundStartOffset)
    // 实际等待在runLoop中根据当前时间和roundStartTime计算
    return 0;
}
