#include "MouseClickEngine.h"
#include "platform/windows/WindowsInputSimulator.h"
#include "core/MonitorManager.h"
#include "core/CoordinateMapper.h"
#include "utils/ValidationUtils.h"
#include "utils/TimeUtils.h"
#include "utils/Logger.h"
#include <QElapsedTimer>
#include <QDateTime>

MouseClickEngine::MouseClickEngine(WindowsInputSimulator* simulator,
                                     MonitorManager* monitorMgr,
                                     QObject* parent)
    : QObject(parent)
    , m_simulator(simulator)
    , m_monitorMgr(monitorMgr)
{
}

MouseClickEngine::~MouseClickEngine()
{
    stop();
}

void MouseClickEngine::setConfig(const Config& config)
{
    if (!m_running.load()) m_config = config;
}

void MouseClickEngine::start()
{
    if (m_running.load()) return;
    if (m_workerThread) {
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    if (!ValidationUtils::validatePressDuration(m_config.pressDurationMs)
        || !ValidationUtils::validateStartDelay(m_config.startDelayMs)
        || !ValidationUtils::validateRepeatCount(m_config.repeatCount, m_config.infiniteRepeat)
        || m_config.button < MouseButton::Left || m_config.button > MouseButton::XButton2
        || m_config.clickMode < ClickMode::Single || m_config.clickMode > ClickMode::Hold
        || m_config.doubleClickIntervalMs < 1 || m_config.doubleClickIntervalMs > 10000) {
        emit errorOccurred("鼠标参数无效"); return;
    }
    // 参数校验
    if (!ValidationUtils::validateClickInterval(m_config.intervalMs, 1)) {
        emit errorOccurred("点击间隔无效");
        return;
    }
    if (m_config.repeatCount < 1 && !m_config.infiniteRepeat) {
        m_config.repeatCount = 1;
    }

    // 工作线程只读启动快照，不跨线程读取随热插拔变化的 Qt 容器。
    m_monitors = m_monitorMgr->monitors();
    m_desktopBounds = m_monitorMgr->virtualDesktopBounds();
    m_stopRequested.store(false);
    m_currentCount.store(0);
    m_state = TaskState::Preparing;
    emit stateChanged(m_state.load());

    // 记录原始鼠标位置（在主线程记录，避免跨线程访问）
    m_originalCursorPos = m_monitorMgr->currentCursorPos();

    m_running.store(true);
    m_state = TaskState::MouseClicking;
    emit stateChanged(m_state.load());
    emit started();

    LOG_INFO("========================================");
    LOG_INFO(">>> 鼠标连点任务启动 <<<");
    LOG_INFO(QString("启动时间: %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")));
    LOG_INFO(QString("配置: 按键=%1  模式=%2  坐标模式=%3")
        .arg(mouseButtonToString(m_config.button))
        .arg(clickModeToString(m_config.clickMode))
        .arg(static_cast<int>(m_config.coordMode)));
    LOG_INFO(QString("时间: 间隔=%1ms  按压=%2ms  启动延迟=%3ms")
        .arg(m_config.intervalMs)
        .arg(m_config.pressDurationMs)
        .arg(m_config.startDelayMs));
    LOG_INFO(QString("执行: 次数=%1  无限循环=%2  恢复光标=%3")
        .arg(m_config.infiniteRepeat ? "无限" : QString::number(m_config.repeatCount))
        .arg(m_config.infiniteRepeat ? "是" : "否")
        .arg(m_config.restoreCursor ? "是" : "否"));
    if (m_config.coordMode == CoordinateMode::VirtualDesktopAbsolute) {
        LOG_INFO(QString("固定坐标: (%1, %2)")
            .arg(m_config.fixedVirtualPos.x())
            .arg(m_config.fixedVirtualPos.y()));
    }
    LOG_INFO("========================================");

    // 在工作线程中执行点击循环，保持主线程 UI 响应
    m_workerThread = QThread::create([this]() {
        // 启动延迟（在工作线程中执行，不阻塞 UI）
        if (m_config.startDelayMs > 0) {
            LOG_INFO(QString("等待启动延迟 %1ms...").arg(m_config.startDelayMs));
            TimeUtils::interruptibleSleep(m_config.startDelayMs, m_stopRequested);
            if (m_stopRequested.load()) {
                LOG_INFO("鼠标连点在启动延迟期间被取消");

                emit stateChanged(TaskState::Idle);
                emit stopped();

                return;
            }
        }

        runLoop();

        m_simulator->releaseAllInputs();

        // 恢复光标
        if (m_config.restoreCursor && !m_stopRequested.load()) {
            m_simulator->mouseMoveAbsolute(
                m_originalCursorPos.x(), m_originalCursorPos.y(),
                m_desktopBounds.width(),
                m_desktopBounds.height());
            LOG_INFO(QString("光标已恢复到原始位置 (%1, %2)")
                .arg(m_originalCursorPos.x())
                .arg(m_originalCursorPos.y()));
        }


        m_state = TaskState::Completed;
        emit stateChanged(m_state.load());
        emit stopped();


        LOG_INFO(QString("<<< 鼠标连点任务结束 (总执行 %1 次) >>>")
            .arg(m_currentCount.load()));
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

void MouseClickEngine::stop()
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

void MouseClickEngine::requestStop()
{
    m_stopRequested.store(true);
}

int MouseClickEngine::totalCount() const
{
    return m_config.infiniteRepeat ? -1 : m_config.repeatCount;
}

// ============================================================================
// 点击执行循环
// ============================================================================
void MouseClickEngine::runLoop()
{
    int maxCount = m_config.infiniteRepeat
        ? std::numeric_limits<int>::max()
        : m_config.repeatCount;

    QElapsedTimer cycleTimer;

    for (int i = 0; i < maxCount && !m_stopRequested.load(); ++i) {
        cycleTimer.start();

        m_currentCount.store(i + 1);
        emit countChanged(i + 1, maxCount);

        // 获取本次点击坐标
        QPoint clickPos;
        resolveTargetPos(clickPos);

        executeClick();

        // 记录每次点击详情: 序号/总数, 坐标, 模式
        QString totalStr = m_config.infiniteRepeat ? "∞" : QString::number(maxCount);
        if (i < 10 || (i + 1) % 100 == 0) LOG_INFO(QString("[%1/%2] 坐标=(%3, %4)  间隔=%5ms")
            .arg(i + 1)
            .arg(totalStr)
            .arg(clickPos.x())
            .arg(clickPos.y())
            .arg(m_config.intervalMs));

        if (m_stopRequested.load()) {
            LOG_INFO(QString("鼠标连点被停止 (已完成 %1 次)").arg(i + 1));
            break;
        }

        // 精确间隔控制: 从本次点击开始计时，减去已用时间
        if (i < maxCount - 1 || m_config.infiniteRepeat) {
            qint64 elapsed = cycleTimer.elapsed();
            qint64 remaining = static_cast<qint64>(m_config.intervalMs) - elapsed;
            if (remaining > 0) {
                if (TimeUtils::interruptibleSleep(remaining, m_stopRequested))
                    break;
            }
        }
    }
}

void MouseClickEngine::executeClick()
{
    QPoint targetPos;
    if (!resolveTargetPos(targetPos)) {
        m_stopRequested = true;
        emit errorOccurred("无法解析目标坐标或坐标不在显示器内");
        return;
    }

    // 移动鼠标到目标位置
    if (m_config.coordMode != CoordinateMode::CurrentCursor) {
        QRect virtBounds = m_desktopBounds;
        m_simulator->mouseMoveAbsolute(targetPos.x(), targetPos.y(),
                                        virtBounds.width(), virtBounds.height());
        QThread::msleep(5);
    }

    // 执行点击
    switch (m_config.clickMode) {
    case ClickMode::Single:
        if (!m_simulator->mouseDown(m_config.button)) { m_stopRequested = true; break; }
        TimeUtils::interruptibleSleep(m_config.pressDurationMs, m_stopRequested);
        m_simulator->mouseUp(m_config.button);
        break;
    case ClickMode::Double:
        for (int click = 0; click < 2 && !m_stopRequested; ++click) {
            if (!m_simulator->mouseDown(m_config.button)) { m_stopRequested = true; break; }
            TimeUtils::interruptibleSleep(50, m_stopRequested);
            m_simulator->mouseUp(m_config.button);
            if (click == 0) TimeUtils::interruptibleSleep(m_config.doubleClickIntervalMs, m_stopRequested);
        }
        break;
    case ClickMode::PressOnly:
        m_simulator->mouseDown(m_config.button);
        break;
    case ClickMode::ReleaseOnly:
        m_simulator->mouseUp(m_config.button);
        break;
    case ClickMode::Hold:
        m_simulator->mouseDown(m_config.button);
        TimeUtils::interruptibleSleep(m_config.pressDurationMs, m_stopRequested);
        m_simulator->mouseUp(m_config.button);
        break;
    }
}

bool MouseClickEngine::resolveTargetPos(QPoint& outPos)
{
    if (m_config.coordMode == CoordinateMode::CurrentCursor) {
        outPos = m_monitorMgr->currentCursorPos();
        return true;
    }
    if (!CoordinateMapper::resolveTargetCoordinate(m_config.coordMode, m_config.fixedVirtualPos,
            m_config.monitorDeviceName, m_config.monitorInternalPos, m_config.monitorRatioPos,
            m_monitors, outPos)) return false;
    for (const auto& monitor : m_monitors)
        if (monitor.containsVirtualPoint(outPos)) return true;
    return false;
}
