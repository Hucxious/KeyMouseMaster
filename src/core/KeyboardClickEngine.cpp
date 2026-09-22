#include "KeyboardClickEngine.h"
#include "platform/windows/WindowsInputSimulator.h"
#include "utils/ValidationUtils.h"
#include "utils/TimeUtils.h"
#include "utils/Logger.h"
#include <QElapsedTimer>
#include <QDateTime>

KeyboardClickEngine::KeyboardClickEngine(WindowsInputSimulator* simulator,
                                           QObject* parent)
    : QObject(parent)
    , m_simulator(simulator)
{
}

KeyboardClickEngine::~KeyboardClickEngine()
{
    stop();
    releaseAllModifiers();
}

void KeyboardClickEngine::setConfig(const Config& config)
{
    if (!m_running.load()) m_config = config;
}

void KeyboardClickEngine::start()
{
    if (m_running.load()) return;
    if (m_workerThread) {
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    if (!m_config.winVk || m_config.winVk > 254
        || !ValidationUtils::validateClickInterval(m_config.intervalMs, 1)
        || (m_config.inputMode == KeyInputMode::Hold
            && !ValidationUtils::validatePressDuration(m_config.pressDurationMs))
        || !ValidationUtils::validateStartDelay(m_config.startDelayMs)
        || !ValidationUtils::validateRepeatCount(m_config.repeatCount, m_config.infiniteRepeat)
        || (m_config.inputMode != KeyInputMode::Normal && m_config.inputMode != KeyInputMode::Hold)) {
        emit errorOccurred("键盘参数无效，请先捕获按键"); return;
    }
    if (m_config.repeatCount < 1 && !m_config.infiniteRepeat)
        m_config.repeatCount = 1;

    m_stopRequested.store(false);
    m_currentCount.store(0);
    m_state = TaskState::Preparing;
    emit stateChanged(m_state.load());

    m_running.store(true);
    m_state = TaskState::KeyboardClicking;
    emit stateChanged(m_state.load());
    emit started();

    LOG_INFO("========================================");
    LOG_INFO(">>> 键盘连点任务启动 <<<");
    LOG_INFO(QString("启动时间: %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")));
    LOG_INFO(QString("配置: 按键=%1  模式=%2")
        .arg(m_config.displayName)
        .arg(keyInputModeToString(m_config.inputMode)));
    LOG_INFO(QString("修饰键: Ctrl=%1 Shift=%2 Alt=%3 Win=%4")
        .arg(m_config.hasCtrl ? "是" : "否")
        .arg(m_config.hasShift ? "是" : "否")
        .arg(m_config.hasAlt ? "是" : "否")
        .arg(m_config.hasWin ? "是" : "否"));
    LOG_INFO(QString("时间: 间隔=%1ms  按压=%2ms  启动延迟=%3ms")
        .arg(m_config.intervalMs)
        .arg(m_config.pressDurationMs)
        .arg(m_config.startDelayMs));
    LOG_INFO(QString("执行: 次数=%1  无限循环=%2")
        .arg(m_config.infiniteRepeat ? "无限" : QString::number(m_config.repeatCount))
        .arg(m_config.infiniteRepeat ? "是" : "否"));
    LOG_INFO("========================================");

    // 在工作线程中执行连点循环，保持主线程 UI 响应
    m_workerThread = QThread::create([this]() {
        // 启动延迟（在工作线程中执行，不阻塞 UI）
        if (m_config.startDelayMs > 0) {
            LOG_INFO(QString("等待启动延迟 %1ms...").arg(m_config.startDelayMs));
            TimeUtils::interruptibleSleep(m_config.startDelayMs, m_stopRequested);
            if (m_stopRequested.load()) {
                LOG_INFO("键盘连点在启动延迟期间被取消");

                emit stateChanged(TaskState::Idle);
                emit stopped();

                return;
            }
        }

        runLoop();

        // 确保所有按键都已释放
        releaseAllModifiers();
        m_simulator->releaseAllInputs();

        m_state = TaskState::Completed;
        emit stateChanged(m_state.load());
        emit stopped();

        LOG_INFO(QString("<<< 键盘连点任务结束 (总执行 %1 次) >>>")
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

void KeyboardClickEngine::stop()
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

void KeyboardClickEngine::requestStop()
{
    m_stopRequested.store(true);
}

int KeyboardClickEngine::totalCount() const
{
    return m_config.infiniteRepeat ? -1 : m_config.repeatCount;
}

void KeyboardClickEngine::runLoop()
{
    int maxCount = m_config.infiniteRepeat
        ? std::numeric_limits<int>::max()
        : m_config.repeatCount;

    QElapsedTimer cycleTimer;

    for (int i = 0; i < maxCount && !m_stopRequested.load(); ++i) {
        cycleTimer.start();

        m_currentCount.store(i + 1);
        emit countChanged(i + 1, maxCount);

        executeKeyAction();

        // 记录每次按键详情: 序号/总数, 按键, 模式
        QString totalStr = m_config.infiniteRepeat ? "∞" : QString::number(maxCount);
        if (i < 10 || (i + 1) % 100 == 0) LOG_INFO(QString("[%1/%2] 按键=%3  模式=%4  间隔=%5ms")
            .arg(i + 1)
            .arg(totalStr)
            .arg(m_config.displayName)
            .arg(keyInputModeToString(m_config.inputMode))
            .arg(m_config.intervalMs));

        if (m_stopRequested.load()) {
            LOG_INFO(QString("键盘连点被停止 (已完成 %1 次)").arg(i + 1));
            break;
        }

        // 精确间隔控制: 从本次按键开始计时，减去已用时间
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

void KeyboardClickEngine::executeKeyAction()
{
    // 两种模式都保留捕获到的组合键；只有长按消费保持时间。
    if (m_config.hasCtrl)
        m_simulator->keyDown(VK_CONTROL);
    if (m_config.hasShift)
        m_simulator->keyDown(VK_SHIFT);
    if (m_config.hasAlt)
        m_simulator->keyDown(VK_MENU);
    if (m_config.hasWin)
        m_simulator->keyDown(VK_LWIN);

    if (!m_stopRequested) m_simulator->keyDown(m_config.winVk, m_config.isExtended);
    m_ctrlDown.store(m_config.hasCtrl);
    m_shiftDown.store(m_config.hasShift);
    m_altDown.store(m_config.hasAlt);
    m_winDown.store(m_config.hasWin);

    if (m_config.inputMode == KeyInputMode::Hold)
        TimeUtils::interruptibleSleep(m_config.pressDurationMs, m_stopRequested);

    m_simulator->keyUp(m_config.winVk, m_config.isExtended);

    // 逆序释放修饰键
    if (m_config.hasWin)  m_simulator->keyUp(VK_LWIN);
    if (m_config.hasAlt)  m_simulator->keyUp(VK_MENU);
    if (m_config.hasShift) m_simulator->keyUp(VK_SHIFT);
    if (m_config.hasCtrl)  m_simulator->keyUp(VK_CONTROL);

    m_ctrlDown.store(false);
    m_shiftDown.store(false);
    m_altDown.store(false);
    m_winDown.store(false);
}

void KeyboardClickEngine::releaseAllModifiers()
{
    if (m_ctrlDown.load())  m_simulator->keyUp(VK_CONTROL);
    if (m_shiftDown.load()) m_simulator->keyUp(VK_SHIFT);
    if (m_altDown.load())   m_simulator->keyUp(VK_MENU);
    if (m_winDown.load())   m_simulator->keyUp(VK_LWIN);

    m_ctrlDown.store(false);
    m_shiftDown.store(false);
    m_altDown.store(false);
    m_winDown.store(false);
}
