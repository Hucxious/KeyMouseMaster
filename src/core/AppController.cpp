#include "AppController.h"
#include "TaskManager.h"
#include "MouseClickEngine.h"
#include "KeyboardClickEngine.h"
#include "ScriptRecorder.h"
#include "ScriptPlayer.h"
#include "ScriptSerializer.h"
#include "MonitorManager.h"
#include "CoordinateMapper.h"
#include "InputStateManager.h"
#include "settings/SettingsManager.h"
#include "platform/windows/WindowsInputSimulator.h"
#include "platform/windows/WindowsHookManager.h"
#include "platform/windows/WindowsHotkeyManager.h"
#include "utils/Logger.h"

AppController::AppController(QObject* parent)
    : QObject(parent)
{
}

AppController::~AppController()
{
    unregisterAllHotkeys();

    // 确保所有输入被释放
    if (m_inputSimulator)
        m_inputSimulator->releaseAllInputs();
    if (m_inputStateManager)
        m_inputStateManager->clearAll();

    LOG_INFO("AppController 已销毁");
}

bool AppController::initialize()
{
    LOG_INFO("正在初始化 AppController...");

    try {
        createSubsystems();

        // 初始化显示器管理器
        m_monitorManager->initialize();

        // 初始化设置
        // (设置管理器在构造时自动初始化)

        // 连接信号
        connectSignals();

        // 注册全局快捷键
        registerAllHotkeys();

        LOG_INFO("AppController 初始化完成");
        emit initialized();
        return true;
    } catch (const std::exception& e) {
        QString msg = QString("初始化失败: %1").arg(e.what());
        LOG_CRITICAL(msg);
        emit initializationError(msg);
        return false;
    }
}

void AppController::createSubsystems()
{
    // 平台层
    m_inputSimulator = new WindowsInputSimulator(this);
    m_hookManager    = new WindowsHookManager(this);
    m_hotkeyManager  = new WindowsHotkeyManager(this);

    // 核心层
    m_monitorManager   = new MonitorManager(this);
    m_coordinateMapper = new CoordinateMapper(this);
    m_inputStateManager = new InputStateManager(this);

    // 引擎
    m_mouseClickEngine    = new MouseClickEngine(m_inputSimulator, m_monitorManager, this);
    m_keyboardClickEngine = new KeyboardClickEngine(m_inputSimulator, this);

    // 脚本
    m_scriptRecorder = new ScriptRecorder(m_hookManager, m_monitorManager, this);
    m_scriptPlayer   = new ScriptPlayer(m_inputSimulator, m_monitorManager, this);
    m_scriptSerializer = new ScriptSerializer(this);

    // 任务管理
    m_taskManager = new TaskManager(this);
    m_taskManager->setMouseClickEngine(m_mouseClickEngine);
    m_taskManager->setKeyboardClickEngine(m_keyboardClickEngine);
    m_taskManager->setScriptRecorder(m_scriptRecorder);
    m_taskManager->setScriptPlayer(m_scriptPlayer);

    // 设置管理
    m_settingsManager = new SettingsManager(this);
}

void AppController::connectSignals()
{
    // 任务状态 -> 控制器
    connect(m_taskManager, &TaskManager::taskStateChanged,
            this, &AppController::onTaskStateChanged);

    // 录制停止 -> 获取文档
    connect(m_scriptRecorder, &ScriptRecorder::recordingStopped,
            this, [this]() {
                emit statusMessage("录制已停止");
            });

    // 显示器变化
    connect(m_monitorManager, &MonitorManager::monitorsChanged,
            this, &AppController::onMonitorsChanged);

    // 全局热键
    connect(m_hotkeyManager, &WindowsHotkeyManager::hotkeyPressed,
            this, &AppController::onGlobalHotkeyPressed);

    // 引擎错误 -> 日志
    connect(m_mouseClickEngine, &MouseClickEngine::errorOccurred,
            this, [this](const QString& msg) {
                LOG_ERROR("鼠标连点: " + msg);
                emit statusMessage("鼠标连点错误: " + msg);
            });

    connect(m_keyboardClickEngine, &KeyboardClickEngine::errorOccurred,
            this, [this](const QString& msg) {
                LOG_ERROR("键盘连点: " + msg);
                emit statusMessage("键盘连点错误: " + msg);
            });

    connect(m_scriptPlayer, &ScriptPlayer::errorOccurred,
            this, [this](const QString& msg) {
                LOG_ERROR("脚本回放: " + msg);
                emit statusMessage("脚本回放错误: " + msg);
            });
}

// ============================================================================
// 全局热键处理
// ============================================================================
void AppController::registerAllHotkeys()
{
    unregisterAllHotkeys();

    // 紧急停止热键
    HotkeyInfo emergencyStop;
    emergencyStop.key = Qt::Key_F12;
    emergencyStop.ctrl = true;
    emergencyStop.shift = true;
    int id = m_hotkeyManager->registerHotkey(emergencyStop);
    if (id > 0) m_registeredHotkeyIds.append(id);

    // 鼠标连点启动热键
    HotkeyInfo hkMouseStart = m_settingsManager->mouseStartHotkey();
    if (hkMouseStart.isValid()) {
        int mid = m_hotkeyManager->registerHotkey(hkMouseStart);
        if (mid > 0) m_registeredHotkeyIds.append(mid);
    }

    // 鼠标连点停止热键
    HotkeyInfo hkMouseStop = m_settingsManager->mouseStopHotkey();
    if (hkMouseStop.isValid()) {
        int mid = m_hotkeyManager->registerHotkey(hkMouseStop);
        if (mid > 0) m_registeredHotkeyIds.append(mid);
    }

    // 键盘连点启动热键
    HotkeyInfo hkKeyStart = m_settingsManager->keyboardStartHotkey();
    if (hkKeyStart.isValid()) {
        int kid = m_hotkeyManager->registerHotkey(hkKeyStart);
        if (kid > 0) m_registeredHotkeyIds.append(kid);
    }

    // 键盘连点停止热键
    HotkeyInfo hkKeyStop = m_settingsManager->keyboardStopHotkey();
    if (hkKeyStop.isValid()) {
        int kid = m_hotkeyManager->registerHotkey(hkKeyStop);
        if (kid > 0) m_registeredHotkeyIds.append(kid);
    }

    LOG_INFO(QString("已注册 %1 个全局快捷键").arg(m_registeredHotkeyIds.size()));
}

void AppController::unregisterAllHotkeys()
{
    m_hotkeyManager->unregisterAll();
    m_registeredHotkeyIds.clear();
}

void AppController::onGlobalHotkeyPressed(int id)
{
    // 根据快捷键ID触发对应操作
    // 实际实现中需要通过存储的ID映射来确定操作
    Q_UNUSED(id)
    // 简化: 在这里根据registerAllHotkeys中分配的ID来判断
    // 更完善的实现应该在注册时保存ID->功能映射
}

// ============================================================================
// 全局操作
// ============================================================================
void AppController::emergencyStop()
{
    LOG_INFO("触发紧急停止");

    // 1. 设置停止标志
    m_taskManager->emergencyStop();

    // 2. 释放所有输入
    m_inputSimulator->releaseAllInputs();

    // 3. 停止录制钩子
    if (m_scriptRecorder && m_scriptRecorder->isRecording()) {
        m_scriptRecorder->stopRecording();
    }

    // 4. 清除输入状态追踪
    m_inputStateManager->clearAll();

    emit emergencyStopTriggered();
    emit statusMessage("紧急停止已触发，所有输入已释放");
}

void AppController::resetInputState()
{
    LOG_INFO("执行输入状态复位");

    // 释放所有常见的修饰键
    m_inputSimulator->releaseAllInputs();

    // 清除追踪状态
    m_inputStateManager->clearAll();

    emit inputStateReset();
    emit statusMessage("输入状态已复位");
}

// ============================================================================
// 槽函数
// ============================================================================
void AppController::onMonitorsChanged()
{
    LOG_INFO("显示器配置发生变化");
    emit statusMessage("显示器配置已更新");

    // 如果正在运行依赖坐标的任务，发出警告
    if (m_taskManager->currentState() == TaskState::MouseClicking
        || m_taskManager->currentState() == TaskState::Playing) {
        LOG_WARNING("显示器配置在任务运行期间发生变化，可能导致坐标偏移");
        emit statusMessage("警告: 显示器配置在任务运行期间发生变化");
    }
}

void AppController::onTaskStateChanged(TaskState state)
{
    QString msg = QString("任务状态: %1").arg(taskStateToString(state));
    emit statusMessage(msg);
}
