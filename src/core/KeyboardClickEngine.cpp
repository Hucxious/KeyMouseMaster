#include "KeyboardClickEngine.h"
#include "platform/windows/WindowsInputSimulator.h"
#include "utils/ValidationUtils.h"
#include "utils/TimeUtils.h"
#include "utils/Logger.h"

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
    m_config = config;
}

void KeyboardClickEngine::start()
{
    if (m_running.load()) return;

    // 参数校验: 完整按键模式下按压时长必须小于间隔
    if (m_config.inputMode == KeyInputMode::FullKey) {
        QString err;
        if (!ValidationUtils::validatePressVsInterval(
                m_config.pressDurationMs, m_config.intervalMs, &err)) {
            emit errorOccurred(err);
            return;
        }
    }

    if (m_config.repeatCount < 1 && !m_config.infiniteRepeat)
        m_config.repeatCount = 1;

    m_stopRequested.store(false);
    m_currentCount.store(0);
    m_state = TaskState::Preparing;
    emit stateChanged(m_state);

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
    m_state = TaskState::KeyboardClicking;
    emit stateChanged(m_state);
    emit started();

    LOG_INFO(QString("键盘连点开始: %1 模式=%2 间隔%3ms 次数%4")
        .arg(m_config.displayName)
        .arg(keyInputModeToString(m_config.inputMode))
        .arg(m_config.intervalMs)
        .arg(m_config.infiniteRepeat ? "无限" : QString::number(m_config.repeatCount)));

    runLoop();

    // 确保所有按键都已释放
    releaseAllModifiers();

    m_running.store(false);
    m_state = TaskState::Completed;
    emit stateChanged(m_state);
    emit stopped();
    emit finished();

    LOG_INFO("键盘连点结束");
}

void KeyboardClickEngine::stop()
{
    m_state = TaskState::Stopping;
    emit stateChanged(m_state);
    m_stopRequested.store(true);

    // 释放可能按下的所有按键
    releaseAllModifiers();
    m_simulator->releaseAllInputs();
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

    for (int i = 0; i < maxCount && !m_stopRequested.load(); ++i) {
        m_currentCount.store(i + 1);
        emit countChanged(i + 1, maxCount);

        executeKeyAction();

        if (m_stopRequested.load()) break;

        if (i < maxCount - 1 || m_config.infiniteRepeat) {
            if (TimeUtils::interruptibleSleep(m_config.intervalMs, m_stopRequested))
                break;
        }
    }
}

void KeyboardClickEngine::executeKeyAction()
{
    switch (m_config.inputMode) {
    case KeyInputMode::Normal:    sendNormalKey(); break;
    case KeyInputMode::Combo:     sendComboKey(); break;
    case KeyInputMode::PressOnly:  sendPressOnly(); break;
    case KeyInputMode::ReleaseOnly: sendReleaseOnly(); break;
    case KeyInputMode::FullKey:   sendFullKey(); break;
    case KeyInputMode::Hold:      sendHoldKey(); break;
    }
}

void KeyboardClickEngine::sendNormalKey()
{
    // 普通单键: 按下 -> 短暂按压 -> 释放
    m_simulator->keyDown(m_config.winVk, m_config.isExtended);
    QThread::msleep(static_cast<unsigned long>(m_config.pressDurationMs));
    m_simulator->keyUp(m_config.winVk, m_config.isExtended);
}

void KeyboardClickEngine::sendComboKey()
{
    // 组合键: 依次按下修饰键 -> 按下主键 -> 释放主键 -> 逆序释放修饰键
    m_simulator->sendCombo(m_config.winVk,
                            m_config.hasCtrl, m_config.hasShift,
                            m_config.hasAlt, m_config.hasWin,
                            m_config.pressDurationMs);
}

void KeyboardClickEngine::sendPressOnly()
{
    m_simulator->keyDown(m_config.winVk, m_config.isExtended);
}

void KeyboardClickEngine::sendReleaseOnly()
{
    m_simulator->keyUp(m_config.winVk, m_config.isExtended);
}

void KeyboardClickEngine::sendFullKey()
{
    // 完整按键: 1.按下修饰键 2.等待 3.按下主键 4.按压 5.释放主键 6.逆序释放修饰键
    // 与组合键类似，但有前后延迟和更明确的执行时间

    if (m_config.hasCtrl)
        m_simulator->keyDown(VK_CONTROL);
    if (m_config.hasShift)
        m_simulator->keyDown(VK_SHIFT);
    if (m_config.hasAlt)
        m_simulator->keyDown(VK_MENU);
    if (m_config.hasWin)
        m_simulator->keyDown(VK_LWIN);

    QThread::msleep(10);

    m_simulator->keyDown(m_config.winVk, m_config.isExtended);
    m_ctrlDown.store(m_config.hasCtrl);
    m_shiftDown.store(m_config.hasShift);
    m_altDown.store(m_config.hasAlt);
    m_winDown.store(m_config.hasWin);

    QThread::msleep(static_cast<unsigned long>(m_config.pressDurationMs));

    m_simulator->keyUp(m_config.winVk, m_config.isExtended);
    QThread::msleep(10);

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

void KeyboardClickEngine::sendHoldKey()
{
    // 长按: 按下 -> 长时间等待 -> 释放
    m_simulator->keyDown(m_config.winVk, m_config.isExtended);
    m_ctrlDown.store(m_config.hasCtrl);
    m_shiftDown.store(m_config.hasShift);
    m_altDown.store(m_config.hasAlt);
    m_winDown.store(m_config.hasWin);

    TimeUtils::interruptibleSleep(m_config.pressDurationMs, m_stopRequested);

    m_simulator->keyUp(m_config.winVk, m_config.isExtended);
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
