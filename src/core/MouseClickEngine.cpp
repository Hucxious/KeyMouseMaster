#include "MouseClickEngine.h"
#include "platform/windows/WindowsInputSimulator.h"
#include "core/MonitorManager.h"
#include "core/CoordinateMapper.h"
#include "utils/ValidationUtils.h"
#include "utils/TimeUtils.h"
#include "utils/Logger.h"

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
    m_config = config;
}

void MouseClickEngine::start()
{
    if (m_running.load()) return;

    // 参数校验
    if (!ValidationUtils::validateClickInterval(m_config.intervalMs, 1)) {
        emit errorOccurred("点击间隔无效");
        return;
    }
    if (m_config.repeatCount < 1 && !m_config.infiniteRepeat) {
        m_config.repeatCount = 1;
    }

    m_stopRequested.store(false);
    m_currentCount.store(0);
    m_state = TaskState::Preparing;
    emit stateChanged(m_state);

    // 记录原始鼠标位置
    m_originalCursorPos = m_monitorMgr->currentCursorPos();

    // 启动延迟
    if (m_config.startDelayMs > 0) {
        QThread::msleep(static_cast<unsigned long>(m_config.startDelayMs));
        if (m_stopRequested.load()) {
            m_state = TaskState::Idle;
            emit stateChanged(m_state);
            return;
        }
    }

    m_running.store(true);
    m_state = TaskState::MouseClicking;
    emit stateChanged(m_state);
    emit started();

    LOG_INFO(QString("鼠标连点开始: %1 %2 间隔%3ms 次数%4")
        .arg(mouseButtonToString(m_config.button))
        .arg(clickModeToString(m_config.clickMode))
        .arg(m_config.intervalMs)
        .arg(m_config.infiniteRepeat ? "无限" : QString::number(m_config.repeatCount)));

    runLoop();

    // 恢复光标
    if (m_config.restoreCursor) {
        m_simulator->mouseMoveAbsolute(
            m_originalCursorPos.x(), m_originalCursorPos.y(),
            m_monitorMgr->virtualDesktopBounds().width(),
            m_monitorMgr->virtualDesktopBounds().height());
    }

    m_running.store(false);
    m_state = m_stopRequested.load() ? TaskState::Completed : TaskState::Completed;
    emit stateChanged(m_state);
    emit stopped();
    emit finished();

    LOG_INFO("鼠标连点结束");
}

void MouseClickEngine::stop()
{
    if (!m_running.load()) return;

    m_state = TaskState::Stopping;
    emit stateChanged(m_state);
    m_stopRequested.store(true);

    // 等待线程结束
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
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

    for (int i = 0; i < maxCount && !m_stopRequested.load(); ++i) {
        m_currentCount.store(i + 1);
        emit countChanged(i + 1, maxCount);

        executeClick();

        if (m_stopRequested.load()) break;

        // 等待间隔 (可中断)
        if (i < maxCount - 1 || m_config.infiniteRepeat) {
            if (TimeUtils::interruptibleSleep(m_config.intervalMs, m_stopRequested))
                break;
        }
    }
}

void MouseClickEngine::executeClick()
{
    QPoint targetPos;
    if (!resolveTargetPos(targetPos)) {
        emit errorOccurred("无法解析目标坐标");
        return;
    }

    // 移动鼠标到目标位置
    if (m_config.coordMode != CoordinateMode::CurrentCursor) {
        QRect virtBounds = m_monitorMgr->virtualDesktopBounds();
        m_simulator->mouseMoveAbsolute(targetPos.x(), targetPos.y(),
                                        virtBounds.width(), virtBounds.height());
        QThread::msleep(5);
    }

    // 执行点击
    switch (m_config.clickMode) {
    case ClickMode::Single:
        m_simulator->mouseClick(m_config.button, m_config.pressDurationMs);
        break;
    case ClickMode::Double:
        m_simulator->mouseDoubleClick(m_config.button, m_config.doubleClickIntervalMs);
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
    switch (m_config.coordMode) {
    case CoordinateMode::CurrentCursor:
        outPos = m_monitorMgr->currentCursorPos();
        return true;
    case CoordinateMode::VirtualDesktopAbsolute:
        outPos = m_config.fixedVirtualPos;
        return true;
    case CoordinateMode::MonitorRelative: {
        MonitorInfo monitor = m_monitorMgr->monitorByName(m_config.monitorDeviceName);
        if (monitor.deviceName.isEmpty()) {
            emit errorOccurred(QString("找不到目标显示器: %1")
                .arg(m_config.monitorDeviceName));
            return false;
        }
        outPos = monitor.internalToVirtual(m_config.monitorInternalPos);
        return true;
    }
    case CoordinateMode::MonitorRatio: {
        MonitorInfo monitor = m_monitorMgr->monitorByName(m_config.monitorDeviceName);
        if (monitor.deviceName.isEmpty()) {
            emit errorOccurred(QString("找不到目标显示器: %1")
                .arg(m_config.monitorDeviceName));
            return false;
        }
        outPos = CoordinateMapper::ratioToVirtual(m_config.monitorRatioPos, monitor);
        return true;
    }
    }
    return false;
}
