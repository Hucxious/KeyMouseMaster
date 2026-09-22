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
#include "ui/HotkeyEdit.h"
#include "utils/KeyMapper.h"
#include "utils/Logger.h"

AppController::AppController(QObject* parent)
    : QObject(parent)
{
}

AppController::~AppController()
{
    HotkeyEdit::setCaptureStateCallback({});
    if (m_taskManager) m_taskManager->emergencyStop();
    unregisterAllHotkeys();
    // QObject 子对象按创建顺序删除；引擎析构仍访问平台对象，必须先销毁依赖方。
    delete m_mouseClickEngine; m_mouseClickEngine = nullptr;
    delete m_keyboardClickEngine; m_keyboardClickEngine = nullptr;
    delete m_scriptPlayer; m_scriptPlayer = nullptr;
    delete m_scriptRecorder; m_scriptRecorder = nullptr;

    // 确保所有输入被释放
    if (m_inputSimulator)
        m_inputSimulator->releaseAllInputs();
    if (m_inputStateManager)
        m_inputStateManager->clearAll();

    LOG_INFO("AppController 已销毁");
}

bool AppController::initialize()
{
    qRegisterMetaType<TaskState>("TaskState");
    LOG_INFO("正在初始化 AppController...");

    try {
        createSubsystems();

        // 初始化显示器管理器
        m_monitorManager->initialize();

        // 初始化设置
        // (设置管理器在构造时自动初始化)

        // 连接信号
        connectSignals();

        // 设置热键捕获回调: 捕获时临时注销全局热键，让按键能到达 Qt 事件系统
        HotkeyEdit::setCaptureStateCallback([this](bool capturing) {
            if (capturing) {
                unregisterAllHotkeys();
            } else {
                registerAllHotkeys();
            }
        });

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
                emit recordedDocumentReady(m_scriptRecorder->takeDocument());
                emit statusMessage("录制已停止");
            });

    // 显示器变化
    connect(m_monitorManager, &MonitorManager::monitorsChanged,
            this, &AppController::onMonitorsChanged);

    // 全局热键
    connect(m_hotkeyManager, &WindowsHotkeyManager::hotkeyPressed,
            this, &AppController::onGlobalHotkeyPressed);

    connect(m_hookManager, &WindowsHookManager::hookError, this, &AppController::statusMessage);
    connect(m_scriptRecorder, &ScriptRecorder::errorOccurred, this, &AppController::statusMessage);
    connect(m_inputSimulator, &WindowsInputSimulator::inputError, this, [this](const QString& msg) {
        LOG_ERROR(msg);
        if (m_taskManager->isAnyTaskRunning()) m_taskManager->requestStop();
        emit statusMessage(msg);
    });
    connect(m_hotkeyManager, &WindowsHotkeyManager::hotkeyRegisterFailed, this,
            [this](int, const QString& reason) {
        m_hotkeyErrors.append(reason);
        emit hotkeyRegistrationError(reason);
    });

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
    if (!m_hotkeyManager || HotkeyEdit::isAnyCapturing()) return;
    m_hotkeyErrors.clear();
    unregisterAllHotkeys();

    // 紧急停止热键 (Ctrl+Shift+F12)
    HotkeyInfo emergencyStop;
    emergencyStop.key = Qt::Key_F12;
    emergencyStop.ctrl = true;
    emergencyStop.shift = true;
    int id = m_hotkeyManager->registerHotkey(emergencyStop);
    if (id > 0) {
        m_registeredHotkeyIds.append(id);
        m_hotkeyActions.insert(id, HotkeyAction::EmergencyStop);
    }

    // 鼠标连点切换热键 (启动/停止用同一个键)
    HotkeyInfo hkMouse = m_settingsManager->mouseStartHotkey();
    if (hkMouse.isValid()) {
        int mid = m_hotkeyManager->registerHotkey(hkMouse);
        if (mid > 0) {
            m_registeredHotkeyIds.append(mid);
            m_hotkeyActions.insert(mid, HotkeyAction::MouseToggle);
        }
    }

    // 键盘连点切换热键 (启动/停止用同一个键)
    HotkeyInfo hkKey = m_settingsManager->keyboardStartHotkey();
    if (hkKey.isValid()) {
        int kid = m_hotkeyManager->registerHotkey(hkKey);
        if (kid > 0) {
            m_registeredHotkeyIds.append(kid);
            m_hotkeyActions.insert(kid, HotkeyAction::KeyboardToggle);
        }
    }

    // 屏幕录制切换热键 (开始/停止录制用同一个键)
    HotkeyInfo hkRecording = m_settingsManager->recordingHotkey();
    if (hkRecording.isValid()) {
        int rid = m_hotkeyManager->registerHotkey(hkRecording);
        if (rid > 0) {
            m_registeredHotkeyIds.append(rid);
            m_hotkeyActions.insert(rid, HotkeyAction::RecordingToggle);
        }
    }

    // 脚本回放切换热键 (开始/停止回放用同一个键)
    HotkeyInfo hkPlayback = m_settingsManager->playbackHotkey();
    if (hkPlayback.isValid()) {
        int pid = m_hotkeyManager->registerHotkey(hkPlayback);
        if (pid > 0) {
            m_registeredHotkeyIds.append(pid);
            m_hotkeyActions.insert(pid, HotkeyAction::PlaybackToggle);
        }
    }

    LOG_INFO(QString("已注册 %1 个全局快捷键").arg(m_registeredHotkeyIds.size()));
}

void AppController::unregisterAllHotkeys()
{
    if (m_hotkeyManager) m_hotkeyManager->unregisterAll();
    m_registeredHotkeyIds.clear();
    m_hotkeyActions.clear();
}

void AppController::onGlobalHotkeyPressed(int id)
{
    // 如果有热键编辑框正在捕获按键，忽略全局热键触发
    if (HotkeyEdit::isAnyCapturing()) {
        return;
    }

    if (!m_hotkeyActions.contains(id)) return;
    HotkeyAction action = m_hotkeyActions.value(id);

    switch (action) {
    case HotkeyAction::MouseToggle:
        // 切换模式: 运行中则停止，空闲则启动
        if (m_taskManager->isAnyTaskRunning()) {
            m_taskManager->requestStop();
        } else {
            emit mouseToggleRequested();
        }
        break;

    case HotkeyAction::KeyboardToggle:
        // 切换模式: 运行中则停止，空闲则启动
        if (m_taskManager->isAnyTaskRunning()) {
            m_taskManager->requestStop();
        } else {
            emit keyboardToggleRequested();
        }
        break;

    case HotkeyAction::RecordingToggle:
        // 切换模式: 录制中则停止，空闲则开始录制
        if (m_taskManager->isAnyTaskRunning()) {
            m_taskManager->requestStop();
        } else {
            emit recordingToggleRequested();
        }
        break;

    case HotkeyAction::PlaybackToggle:
        // 切换模式: 回放中则停止，空闲则开始回放
        if (m_taskManager->isAnyTaskRunning()) {
            m_taskManager->requestStop();
        } else {
            emit playbackToggleRequested();
        }
        break;

    case HotkeyAction::EmergencyStop:
    default:
        emergencyStop();
        break;
    }
}

// ============================================================================
// 将当前设置应用到引擎
// ============================================================================
void AppController::applyMouseSettings()
{
    MouseClickEngine::Config cfg;
    cfg.button       = static_cast<MouseButton>(m_settingsManager->mouseButton());
    cfg.clickMode    = static_cast<ClickMode>(m_settingsManager->mouseClickMode());
    cfg.coordMode    = m_settingsManager->mouseCoordinateMode();

    // 应用间隔单位换算: unit=0→毫秒(×1), unit=1→秒(×1000)
    int intervalRaw  = m_settingsManager->mouseClickInterval();
    int intervalUnit = m_settingsManager->mouseClickIntervalUnit();
    const qint64 intervalMs = static_cast<qint64>(intervalRaw) * (intervalUnit == 1 ? 1000 : 1);
    cfg.intervalMs = intervalMs >= 1 && intervalMs <= AppConstants::MAX_CLICK_INTERVAL_MS
        ? static_cast<int>(intervalMs) : 0;

    cfg.pressDurationMs       = m_settingsManager->mousePressDuration();
    cfg.doubleClickIntervalMs = m_settingsManager->mouseDoubleClickInterval();
    cfg.startDelayMs          = m_settingsManager->mouseStartDelay();
    cfg.repeatCount           = m_settingsManager->mouseRepeatCount();
    cfg.infiniteRepeat        = m_settingsManager->mouseInfinite();
    cfg.restoreCursor         = m_settingsManager->mouseRestoreCursor();
    cfg.fixedVirtualPos       = m_settingsManager->mouseFixedPos();
    cfg.monitorDeviceName     = m_settingsManager->mouseMonitorDevice();
    cfg.monitorInternalPos = m_settingsManager->mouseMonitorPos();

    m_mouseClickEngine->setConfig(cfg);

    LOG_INFO(QString("鼠标引擎配置已应用: 按键=%1 模式=%2 间隔=%3ms 次数=%4")
        .arg(mouseButtonToString(cfg.button))
        .arg(clickModeToString(cfg.clickMode))
        .arg(cfg.intervalMs)
        .arg(cfg.infiniteRepeat ? "无限" : QString::number(cfg.repeatCount)));

    // 记录当前快捷键配置
    HotkeyInfo hkMouse = m_settingsManager->mouseStartHotkey();
    LOG_INFO(QString("鼠标快捷键: 切换=%1  紧急停止=Ctrl+Shift+F12")
        .arg(hkMouse.isValid() ? hkMouse.toString() : "未设置"));
}

void AppController::applyKeyboardSettings()
{
    KeyboardClickEngine::Config cfg;
    cfg.inputMode       = static_cast<KeyInputMode>(m_settingsManager->keyboardInputMode());
    cfg.intervalMs      = m_settingsManager->keyboardInterval();
    cfg.pressDurationMs = m_settingsManager->keyboardPressDuration();
    cfg.startDelayMs    = m_settingsManager->keyboardStartDelay();
    cfg.repeatCount     = m_settingsManager->keyboardRepeatCount();
    cfg.infiniteRepeat  = m_settingsManager->keyboardInfinite();
    cfg.qtKey           = m_settingsManager->keyboardQtKey();
    cfg.winVk           = KeyMapper::qtKeyToWinVk(cfg.qtKey);
    const KeyInfo key = m_settingsManager->keyboardKeyInfo();
    cfg.winVk = key.winVk;
    cfg.scanCode = key.scanCode;
    cfg.isExtended = key.isExtended;
    cfg.hasCtrl = key.hasCtrl; cfg.hasShift = key.hasShift;
    cfg.hasAlt = key.hasAlt; cfg.hasWin = key.hasWin;
    cfg.displayName = key.displayName;

    m_keyboardClickEngine->setConfig(cfg);

    LOG_INFO(QString("键盘引擎配置已应用: 按键=%1 模式=%2 间隔=%3ms 次数=%4")
        .arg(cfg.displayName)
        .arg(keyInputModeToString(cfg.inputMode))
        .arg(cfg.intervalMs)
        .arg(cfg.infiniteRepeat ? "无限" : QString::number(cfg.repeatCount)));

    // 记录当前快捷键配置
    HotkeyInfo hkKey = m_settingsManager->keyboardStartHotkey();
    LOG_INFO(QString("键盘快捷键: 切换=%1  紧急停止=Ctrl+Shift+F12")
        .arg(hkKey.isValid() ? hkKey.toString() : "未设置"));
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

    m_taskManager->emergencyStop();
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
        || m_taskManager->currentState() == TaskState::Playing
        || m_taskManager->currentState() == TaskState::Paused) {
        m_taskManager->emergencyStop();
        LOG_WARNING("显示器配置发生变化，已停止坐标任务");
        emit statusMessage("警告: 显示器配置在任务运行期间发生变化");
    }
}

void AppController::onTaskStateChanged(TaskState state)
{
    QString msg = QString("任务状态: %1").arg(taskStateToString(state));
    emit statusMessage(msg);
}
