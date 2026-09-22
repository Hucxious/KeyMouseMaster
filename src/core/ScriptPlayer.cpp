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
    if (!m_running.load()) m_document = doc;
}

void ScriptPlayer::setPlaybackSettings(const PlaybackSettings& settings)
{
    if (!m_running.load()) m_playbackSettings = settings;
}

void ScriptPlayer::start()
{
    if (m_running.load()) return;
    if (m_workerThread) {
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    if (m_document.isEmpty()) {
        emit errorOccurred("脚本事件为空，无法回放");
        return;
    }

    QString validationError;
    if (!m_document.isValid(&validationError)
        || !ValidationUtils::validatePlaybackSpeed(m_playbackSettings.speedFactor, &validationError)
        || !ValidationUtils::validateStartDelay(m_playbackSettings.startDelayMs, &validationError)
        || !ValidationUtils::validateRepeatCount(m_playbackSettings.repeatCount, m_playbackSettings.infiniteRepeat, &validationError)
        || m_playbackSettings.roundIntervalMs < 0 || m_playbackSettings.roundIntervalMs > 3600000) {
        emit errorOccurred(validationError.isEmpty() ? "回放参数无效" : validationError); return;
    }
    // 验证显示器
    QString monitorError;
    if ((m_playbackSettings.coordinateMode == CoordinateMode::MonitorRelative
         || m_playbackSettings.coordinateMode == CoordinateMode::MonitorRatio)
        && !m_document.validateMonitors(m_monitorMgr->monitors(), &monitorError)) {
        emit errorOccurred(monitorError);
        return;
    }

    // 工作线程只读启动快照，不跨线程读取随热插拔变化的 Qt 容器。
    m_monitors = m_monitorMgr->monitors();
    m_desktopBounds = m_monitorMgr->virtualDesktopBounds();
    m_stopRequested.store(false);
    m_paused.store(false);
    m_currentEventIndex.store(0);
    m_currentRound.store(0);
    m_state = TaskState::Preparing;
    emit stateChanged(m_state.load());

    // 保存回放前鼠标位置（在主线程记录，避免跨线程访问）
    m_playbackStartCursor = m_monitorMgr->currentCursorPos();

    m_running.store(true);
    m_state = TaskState::Playing;
    emit stateChanged(m_state.load());
    emit started();

    LOG_INFO(QString("脚本回放开始: %1 个事件, 速度 %2x")
        .arg(m_document.eventCount())
        .arg(m_playbackSettings.speedFactor));

    // 在工作线程中执行回放循环，保持主线程 UI 响应
    m_workerThread = QThread::create([this]() {
        // 启动延迟（在工作线程中执行，不阻塞 UI）
        if (m_playbackSettings.startDelayMs > 0) {
            TimeUtils::interruptibleSleep(m_playbackSettings.startDelayMs, m_stopRequested);
            if (m_stopRequested.load()) {
                LOG_INFO("脚本回放在启动延迟期间被取消");

                m_state = TaskState::Idle;
                emit stateChanged(m_state.load());
                emit stopped();

                return;
            }
        }

        runLoop();

        // 恢复光标
        if (m_playbackSettings.restoreCursor && !m_stopRequested.load()) {
            QRect virtBounds = m_desktopBounds;
            m_simulator->mouseMoveAbsolute(
                m_playbackStartCursor.x(), m_playbackStartCursor.y(),
                virtBounds.width(), virtBounds.height());
        }

        // 释放所有输入
        m_simulator->releaseAllInputs();


        m_state = TaskState::Completed;
        emit stateChanged(m_state.load());
        emit stopped();


        LOG_INFO("脚本回放结束");
    });
    const quint64 generation = ++m_generation;
    connect(m_workerThread, &QThread::finished, this, [this, generation]() {
        // disconnect 不撤回已排队信号；旧任务通知不能完成新任务。
        if (generation != m_generation) return;
        m_running.store(false);
        emit finished();
    });
    m_workerThread->start();
}

void ScriptPlayer::stop()
{
    ++m_generation;
    m_stopRequested.store(true);
    // 所有等待均可中断；先 join，后释放，防止释放后工作线程再次按下。
    if (m_workerThread) {
        disconnect(m_workerThread, nullptr, this, nullptr);
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }
    const bool wasRunning = m_running.exchange(false);
    m_state = TaskState::Idle;
    if (wasRunning) {
        m_simulator->releaseAllInputs();
        emit stateChanged(TaskState::Idle);
        emit finished();
    }
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
        emit stateChanged(m_state.load());
        emit paused();
        LOG_INFO("脚本回放已暂停");
    }
}

void ScriptPlayer::resume()
{
    if (m_paused.load()) {
        m_paused.store(false);
        m_state = TaskState::Playing;
        emit stateChanged(m_state.load());
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
        int64_t pausedMs = 0;

        LOG_INFO(QString("回放轮次 %1/%2").arg(round + 1)
            .arg(m_playbackSettings.infiniteRepeat ? "∞" : QString::number(maxRounds)));

        for (int i = 0; i < m_document.eventCount() && !m_stopRequested.load(); ++i) {
            // 处理暂停
            while (m_paused.load() && !m_stopRequested.load()) {
                const auto before = TimeUtils::currentTimeMs();
                TimeUtils::interruptibleSleep(10, m_stopRequested);
                pausedMs += TimeUtils::currentTimeMs() - before;
            }
            if (m_stopRequested.load()) break;

            const ScriptEvent& ev = m_document.events[i];

            // 跳过禁用的事件
            if (!ev.enabled && m_playbackSettings.skipDisabledEvents)
                continue;

            // 计算等待时间
            // 时间戳相对录制起点，保留第一事件之前的等待；暂停不消耗脚本时间。
            while (!m_stopRequested.load()) {
                if (m_paused.load()) {
                    const auto before = TimeUtils::currentTimeMs();
                    TimeUtils::interruptibleSleep(10, m_stopRequested);
                    pausedMs += TimeUtils::currentTimeMs() - before;
                    continue;
                }
                const int64_t remaining = TimeUtils::remainingWaitMs(
                    ev.timestampMs, roundStartTime + pausedMs, m_playbackSettings.speedFactor);
                if (remaining <= 0) break;
                TimeUtils::interruptibleSleep(qMin<int64_t>(remaining, 10), m_stopRequested);
            }

            if (m_stopRequested.load()) break;

            // 播放事件
            m_currentEventIndex.store(i + 1);
            emit progressChanged(i + 1, m_document.eventCount(), round + 1);

            if (!playEvent(ev)) {
                LOG_ERROR(QString("事件 %1 回放失败: %2")
                    .arg(i).arg(ev.eventSummary()));
                emit errorOccurred(QString("事件 %1 回放失败，已停止").arg(i + 1));
                m_stopRequested = true;
                break;
            }
        }

        // 即使脚本缺失 KeyUp，下一轮也不能继承上一轮按下状态。
        m_simulator->releaseAllInputs();
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

    if (m_playbackSettings.coordinateMode != CoordinateMode::CurrentCursor
        && !m_simulator->mouseMoveAbsolute(targetPos.x(), targetPos.y(),
                                          m_desktopBounds.width(), m_desktopBounds.height()))
        return false;
    switch (ev.type) {
    case ScriptEventType::MouseMove:
        return true;

    case ScriptEventType::MouseDown:
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
    // 修饰键本身也是独立的 down/up 事件，按原始顺序发送，避免重复按下。
    if (ev.type == ScriptEventType::KeyDown) {
        return m_simulator->keyDown(ev.winVk, ev.isExtendedKey);
    } else {
        return m_simulator->keyUp(ev.winVk, ev.isExtendedKey);
    }
}

bool ScriptPlayer::resolvePlaybackCoordinate(const ScriptEvent& ev, QPoint& outVirtualPt)
{
    if (m_playbackSettings.coordinateMode == CoordinateMode::CurrentCursor) {
        outVirtualPt = m_monitorMgr->currentCursorPos(); return true;
    }
    if (!CoordinateMapper::resolveTargetCoordinate(
        m_playbackSettings.coordinateMode,
        ev.virtualDesktopPos,
        ev.monitorDeviceName,
        ev.monitorInternalPos,
        ev.monitorRatioPos,
        m_monitors, outVirtualPt)) return false;
    for (const auto& monitor : m_monitors)
        if (monitor.containsVirtualPoint(outVirtualPt)) return true;
    return false;
}
