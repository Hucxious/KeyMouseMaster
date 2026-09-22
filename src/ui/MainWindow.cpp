#include "MainWindow.h"
#include "AboutDialog.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QTabBar>
#include <QLayout>
#include "MouseClickPage.h"
#include "KeyboardClickPage.h"
#include "ScriptPage.h"
#include "core/AppController.h"
#include "core/TaskManager.h"
#include "core/MonitorManager.h"
#include "core/ScriptRecorder.h"
#include "platform/windows/WindowsHookManager.h"
#include <QScreen>
#include "settings/SettingsManager.h"
#include "utils/Logger.h"
#include <QCloseEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QAbstractSpinBox>

MainWindow::MainWindow(AppController* controller, QWidget* parent)
    : QMainWindow(parent)
    , m_controller(controller)
    , m_settings(controller->settingsManager())
{
    setupUI();
    setupMenuBar();
    setupSystemTray();
    setupStatusBar();
    connectSignals();
    applyAppStyle();
    loadSettings();
    m_controller->hookManager()->setOwnWindowHandle(reinterpret_cast<void*>(winId()));
    if (!m_controller->hotkeyErrors().isEmpty())
        statusBar()->showMessage("快捷键注册失败: " + m_controller->hotkeyErrors().join("; "));


    // 启动状态更新定时器
    m_statusUpdateTimer = new QTimer(this);
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusUpdateTimer->start(200); // 每200ms更新

    LOG_INFO("主窗口初始化完成");
}

MainWindow::~MainWindow()
{
    m_statusUpdateTimer->stop();
    saveSettings();
}

// ============================================================================
// UI 构建
// ============================================================================
void MainWindow::setupUI()
{
    setWindowTitle("键鼠大师 KeyMouseMaster v1.0");
    resize(860, 580);
    setMinimumSize(700, 500);

    // 中央 TabWidget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);

    m_mousePage = new MouseClickPage(m_controller);
    m_keyboardPage = new KeyboardClickPage(m_controller);
    m_scriptPage = new ScriptPage(m_controller);

    m_tabWidget->addTab(m_mousePage, "🖱 鼠标连点");
    m_tabWidget->addTab(m_keyboardPage, "⌨ 键盘连点");
    m_tabWidget->addTab(m_scriptPage, "📜 脚本录制");

    setCentralWidget(m_tabWidget);

    // 为所有 QAbstractSpinBox 和 QComboBox 安装事件过滤器 — 禁用鼠标滚轮调节
    const auto widgets = findChildren<QWidget*>(QString(), Qt::FindChildrenRecursively);
    for (QWidget* w : widgets) {
        if (qobject_cast<QAbstractSpinBox*>(w) || qobject_cast<QComboBox*>(w)) {
            w->installEventFilter(this);
            w->setFocusPolicy(Qt::StrongFocus);
        }
    }
}

void MainWindow::setupMenuBar()
{
    auto action = [this](QMenu* menu, const QString& text, const char* name) {
        auto* result = menu->addAction(text);
        result->setObjectName(QString::fromLatin1(name));
        return result;
    };
    m_fileMenu = menuBar()->addMenu("文件(&F)");
    auto scriptFile = [&](const QString& text, const char* name, void (ScriptPage::*slot)()) {
        auto* item = action(m_fileMenu, text, name);
        m_scriptFileActions.append(item);
        connect(item, &QAction::triggered, this, [this, slot] {
            if (!m_controller->taskManager()->canStartTask()) return;
            m_tabWidget->setCurrentWidget(m_scriptPage);
            (m_scriptPage->*slot)();
        });
    };
    scriptFile("导入脚本...", "importScript", &ScriptPage::onImportScript);
    scriptFile("保存脚本", "saveScript", &ScriptPage::onSaveScript);
    scriptFile("脚本另存为...", "saveScriptAs", &ScriptPage::onSaveAsScript);
    m_fileMenu->addSeparator();
    connect(action(m_fileMenu, "退出", "exitApplication"), &QAction::triggered,
            this, &MainWindow::exitApplication);

    auto* runMenu = menuBar()->addMenu("运行(&R)");
    auto run = [&](const QString& text, const char* name, auto* page, auto slot) {
        auto* item = action(runMenu, text, name);
        m_runActions.append(item);
        connect(item, &QAction::triggered, this, [this, page, slot] {
            // 页面负责校验；TaskManager 是菜单和按钮共同的互斥状态来源。
            if (!m_controller->taskManager()->canStartTask()) return;
            m_tabWidget->setCurrentWidget(page);
            (page->*slot)();
        });
    };
    run("启动鼠标连点", "runMouse", m_mousePage, &MouseClickPage::onStartClicked);
    run("启动键盘连点", "runKeyboard", m_keyboardPage, &KeyboardClickPage::onStartClicked);
    runMenu->addSeparator();
    run("开始脚本录制", "runRecording", m_scriptPage, &ScriptPage::onStartRecording);
    run("启用脚本回放", "runPlayback", m_scriptPage, &ScriptPage::onStartPlayback);
    runMenu->addSeparator();
    auto* restore = action(runMenu, "恢复默认设置...", "restoreDefaults");
    restore->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    restore->setShortcutContext(Qt::WindowShortcut);
    connect(restore, &QAction::triggered, this, &MainWindow::restoreDefaults);


    m_viewMenu = menuBar()->addMenu("视图(&V)");
    m_alwaysOnTopAction = action(m_viewMenu, "总在最前", "alwaysOnTop");
    m_alwaysOnTopAction->setCheckable(true);
    connect(m_alwaysOnTopAction, &QAction::toggled, this, &MainWindow::setAlwaysOnTop);
    m_statusBarAction = action(m_viewMenu, "显示状态栏", "showStatusBar");
    m_statusBarAction->setCheckable(true);
    connect(m_statusBarAction, &QAction::toggled, this, [this](bool visible) {
        statusBar()->setVisible(visible);
        m_settings->setStatusBarVisible(visible);
    });
    auto* tray = action(m_viewMenu, "最小化到系统托盘", "minimizeToTray");
    tray->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());
    connect(tray, &QAction::triggered, this, &QWidget::hide);

    m_helpMenu = menuBar()->addMenu("帮助(&H)");
    connect(action(m_helpMenu, "使用说明", "userGuide"), &QAction::triggered, this, &MainWindow::openUserGuide);
    connect(action(m_helpMenu, "GitHub 项目主页", "github"), &QAction::triggered, this, &MainWindow::openGitHub);
    connect(action(m_helpMenu, "问题反馈", "feedback"), &QAction::triggered, this, &MainWindow::openGitHub);
    m_helpMenu->addSeparator();
    connect(action(m_helpMenu, "关于 KeyMouseMaster", "aboutKmm"), &QAction::triggered, this, [this] {
        AboutDialog dialog(this);
        connect(&dialog, &AboutDialog::githubRequested, this, &MainWindow::openGitHub);
        dialog.exec();
    });
    connect(action(m_helpMenu, "关于 Qt", "aboutQt"), &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::setupSystemTray()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icons/app.png"));
    m_trayIcon->setToolTip("键鼠大师 - 空闲");

    m_trayMenu = new QMenu(this);

    m_trayShowAction = m_trayMenu->addAction("显示主窗口");
    connect(m_trayShowAction, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });

    m_trayMenu->addSeparator();

    m_trayStartAction = m_trayMenu->addAction("启动当前任务");
    m_trayStopAction = m_trayMenu->addAction("停止当前任务");
    m_trayStopAction->setEnabled(false);
    connect(m_trayStartAction, &QAction::triggered, this, [this]() {
        const int index = m_tabWidget->currentIndex();
        m_runActions.at(index == 2 ? 3 : index)->trigger();
    });
    connect(m_trayStopAction, &QAction::triggered, m_controller->taskManager(), &TaskManager::requestStop);

    m_trayMenu->addSeparator();

    m_trayResetAction = m_trayMenu->addAction("输入状态复位");
    connect(m_trayResetAction, &QAction::triggered, m_controller, &AppController::resetInputState);

    m_trayMenu->addSeparator();

    m_trayExitAction = m_trayMenu->addAction("退出");
    connect(m_trayExitAction, &QAction::triggered, this, &MainWindow::exitApplication);

    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);

    m_trayIcon->show();
}

void MainWindow::setupStatusBar()
{
    m_stateLabel = new QLabel("就绪");
    m_stateLabel->setStyleSheet("QLabel { font-weight: bold; }");

    m_cursorLabel = new QLabel("坐标: (0, 0)");
    m_monitorLabel = new QLabel("显示器: --");

    statusBar()->addWidget(m_stateLabel, 1);
    statusBar()->addPermanentWidget(m_cursorLabel);
    statusBar()->addPermanentWidget(m_monitorLabel);
}

void MainWindow::connectSignals()
{
    connect(m_controller, &AppController::mouseToggleRequested, this, &MainWindow::onMousePageStart);
    connect(m_controller, &AppController::keyboardToggleRequested, this, &MainWindow::onKeyboardPageStart);
    connect(m_controller, &AppController::recordingToggleRequested, this, &MainWindow::onScriptRecordStart);
    connect(m_controller, &AppController::recordedDocumentReady, m_scriptPage, &ScriptPage::setDocument);
    connect(m_controller, &AppController::hotkeyRegistrationError, this, [this](const QString& msg) {
        statusBar()->showMessage("快捷键注册失败: " + msg);
    });
    const auto showStatus = [this](const QString& msg) { statusBar()->showMessage(msg, 10000); };
    connect(m_mousePage, &MouseClickPage::statusMessage, this, showStatus);
    connect(m_keyboardPage, &KeyboardClickPage::statusMessage, this, showStatus);
    connect(m_scriptPage, &ScriptPage::statusMessage, this, showStatus);
    // 分页切换
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    // 鼠标连点页
    connect(m_mousePage, &MouseClickPage::startRequested,
            this, &MainWindow::onMousePageStart);
    connect(m_mousePage, &MouseClickPage::stopRequested,
            this, &MainWindow::onMousePageStop);

    // 键盘连点页
    connect(m_keyboardPage, &KeyboardClickPage::startRequested,
            this, &MainWindow::onKeyboardPageStart);
    connect(m_keyboardPage, &KeyboardClickPage::stopRequested,
            this, &MainWindow::onKeyboardPageStop);

    // 脚本页
    connect(m_scriptPage, &ScriptPage::startRecordingRequested,
            this, &MainWindow::onScriptRecordStart);
    connect(m_scriptPage, &ScriptPage::stopRecordingRequested,
            this, &MainWindow::onScriptRecordStop);
    connect(m_scriptPage, &ScriptPage::startPlaybackRequested,
            this, &MainWindow::onScriptPlaybackStart);
    connect(m_scriptPage, &ScriptPage::stopPlaybackRequested,
            this, &MainWindow::onScriptPlaybackStop);

    // 任务状态
    connect(m_controller->taskManager(), &TaskManager::taskStateChanged,
            this, &MainWindow::onTaskStateChanged);

    // 紧急停止
    connect(m_controller, &AppController::emergencyStopTriggered,
            this, &MainWindow::onEmergencyStop);

    // 状态消息
    connect(m_controller, &AppController::statusMessage,
            this, [this](const QString& msg) {
                statusBar()->showMessage(msg, 5000);
            });

    // 回放切换快捷键 (F10) 触发
    connect(m_controller, &AppController::playbackToggleRequested,
            this, [this]() {
                // 切换到脚本页面
                m_tabWidget->setCurrentIndex(2);
                // 触发回放
                onScriptPlaybackStart();
            });
}

// ============================================================================
// 设置
// ============================================================================
void MainWindow::loadSettings()
{
    m_alwaysOnTopAction->setChecked(m_settings->alwaysOnTop());
    m_statusBarAction->setChecked(m_settings->statusBarVisible());
    statusBar()->setVisible(m_settings->statusBarVisible());
    ensurePolished();
    for (auto* widget : m_scriptPage->findChildren<QWidget*>()) widget->ensurePolished();
    m_scriptPage->ensurePolished();
    for (auto* layout : m_scriptPage->findChildren<QLayout*>()) layout->invalidate();
    m_scriptPage->layout()->activate();
    // 使用页面自然尺寸加窗口装饰；最小尺寸仍独立保持 700×500。
    const QSize pageHint = m_scriptPage->recommendedPageSize();
    const int chrome = menuBar()->sizeHint().height() + m_tabWidget->tabBar()->sizeHint().height()
        + (m_statusBarAction->isChecked() ? statusBar()->sizeHint().height() : 0);
    QSize desired = QSize(pageHint.width() + 24, pageHint.height() + chrome + 24).expandedTo(minimumSize());
    const QSize saved = m_settings->windowSize();
    const bool validSaved = saved.width() >= minimumWidth() && saved.height() >= minimumHeight();
    if (validSaved) desired = saved;
    QScreen* screen = QGuiApplication::primaryScreen();
    const QPoint savedPos = m_settings->windowPosition();
    for (auto* candidate : QGuiApplication::screens()) {
        if (candidate->availableGeometry().contains(savedPos)) { screen = candidate; break; }
    }
    const QRect available = screen->availableGeometry().adjusted(8, fontMetrics().height() * 2, -8, -8);
    desired = desired.boundedTo(available.size()).expandedTo(minimumSize());
    resize(desired);
    QPoint target = validSaved ? savedPos : available.center() - QPoint(width()/2, height()/2);
    target.setX(qBound(available.left(), target.x(), qMax(available.left(), available.right()-width()+1)));
    target.setY(qBound(available.top(), target.y(), qMax(available.top(), available.bottom()-height()+1)));
    move(target);
    // 恢复标签期间不能触发保存，覆盖尚未恢复的窗口状态。
    const QSignalBlocker blocker(m_tabWidget);
    const int tabIndex = m_settings->currentTabIndex();
    if (tabIndex >= 0 && tabIndex < m_tabWidget->count()) m_tabWidget->setCurrentIndex(tabIndex);
    if (m_settings->windowMaximized()) setWindowState(windowState() | Qt::WindowMaximized);
}

void MainWindow::saveSettings()
{
    const QRect normal = (isMaximized() || isMinimized()) ? normalGeometry() : geometry();
    m_settings->setWindowSize(normal.size());
    m_settings->setWindowPosition(normal.topLeft());
    m_settings->setWindowMaximized(isMaximized());
    m_settings->setCurrentTabIndex(m_tabWidget->currentIndex());

    m_mousePage->saveSettings();
    m_keyboardPage->saveSettings();
    m_scriptPage->saveSettings();

    // 检查快捷键冲突
    QString conflictMsg = m_settings->checkHotkeyConflicts();
    if (!conflictMsg.isEmpty()) {
        LOG_WARNING(conflictMsg);
        // 不阻塞保存，但记录警告
    }

    m_settings->sync();

    // 热键变更后立即重新注册，无需重启生效
    m_controller->registerAllHotkeys();
    LOG_INFO("设置已保存，快捷键已重新注册");
}

void MainWindow::applyAppStyle()
{
    setStyleSheet(R"(
        QMainWindow {
            background-color: #f5f5f5;
        }
        QGroupBox {
            font-weight: bold;
            font-size: 12px;
            border: 1px solid #ddd;
            border-radius: 3px;
            margin-top: 4px;
            padding-top: 4px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 3px;
        }
        QTableView {
            gridline-color: #e0e0e0;
            selection-background-color: #2196F3;
            selection-color: white;
        }
        QPushButton {
            padding: 3px 10px;
            border: 1px solid #ccc;
            border-radius: 3px;
            background-color: #fff;
        }
        QPushButton:hover {
            background-color: #e8e8e8;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
        QComboBox {
            padding: 2px 4px;
            border: 1px solid #ccc;
            border-radius: 3px;
            min-width: 80px;
        }
        QSpinBox, QLineEdit {
            padding: 2px 4px;
            border: 1px solid #ccc;
            border-radius: 3px;
        }
        QSpinBox::up-button, QDoubleSpinBox::up-button {
            min-width: 18px;
        }
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            min-width: 18px;
        }
        QTabWidget::pane {
            border: 1px solid #ddd;
        }
        QTabBar::tab {
            padding: 5px 12px;
            border: 1px solid #ddd;
            border-bottom: none;
            background-color: #f0f0f0;
        }
        QTabBar::tab:selected {
            background-color: white;
            border-bottom: 2px solid #2196F3;
        }
        QStatusBar {
            border-top: 1px solid #ddd;
            background-color: #fafafa;
        }
    )");
}

// ============================================================================
// 系统托盘
// ============================================================================
void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible()) {
            hide();
        } else {
            showNormal();
            raise();
            activateWindow();
        }
    }
}

void MainWindow::updateTrayIcon(TaskState state)
{
    switch (state) {
    case TaskState::MouseClicking:
    case TaskState::KeyboardClicking:
        m_trayIcon->setToolTip("键鼠大师 - 连点中");
        break;
    case TaskState::Recording:
        m_trayIcon->setToolTip("键鼠大师 - 录制中");
        break;
    case TaskState::Playing:
        m_trayIcon->setToolTip("键鼠大师 - 回放中");
        break;
    case TaskState::Error:
        m_trayIcon->setToolTip("键鼠大师 - 错误");
        break;
    default:
        m_trayIcon->setToolTip("键鼠大师 - 空闲");
        break;
    }
}

// ============================================================================
// 任务操作
// ============================================================================
void MainWindow::onMousePageStart()
{
    if (!m_controller->taskManager()->canStartTask()) return;
    m_mousePage->saveSettings();
    QString error;
    // 热键变更后立即重新注册，无需重启生效
    m_controller->registerAllHotkeys();
    // 先将设置应用到引擎
    m_controller->applyMouseSettings();
    // 先设置运行状态，再启动引擎（引擎在线程中异步执行，不阻塞 UI）
    m_isTaskRunning = true;
    m_mousePage->setRunningState(true);
    if (!m_controller->taskManager()->requestStartMouseClick(&error)) {
        // 启动失败，回退状态
        m_isTaskRunning = false;
        m_mousePage->setRunningState(false);
        statusBar()->showMessage("启动失败: " + error, 5000);
        return;
    }
}

void MainWindow::onMousePageStop()
{
    m_controller->taskManager()->requestStop();
}

void MainWindow::onKeyboardPageStart()
{
    if (!m_controller->taskManager()->canStartTask()) return;
    m_keyboardPage->saveSettings();
    QString error;
    // 热键变更后立即重新注册，无需重启生效
    m_controller->registerAllHotkeys();
    // 先将设置应用到引擎
    m_controller->applyKeyboardSettings();
    // 先设置运行状态，再启动引擎（引擎在线程中异步执行，不阻塞 UI）
    m_isTaskRunning = true;
    m_keyboardPage->setRunningState(true);
    if (!m_controller->taskManager()->requestStartKeyboardClick(&error)) {
        // 启动失败，回退状态
        m_isTaskRunning = false;
        m_keyboardPage->setRunningState(false);
        statusBar()->showMessage("启动失败: " + error, 5000);
        return;
    }
}

void MainWindow::onKeyboardPageStop()
{
    m_controller->taskManager()->requestStop();
}

void MainWindow::onScriptRecordStart()
{
    if (!m_controller->taskManager()->canStartTask()) return;
    if (!m_scriptPage->confirmDiscardChanges()) return;
    // 从 ScriptPage 读取录制设置
    RecordingSettings settings = m_scriptPage->recordingSettings();
    QString error;
    if (!m_controller->taskManager()->requestStartRecording(settings, &error)) {
        statusBar()->showMessage("启动录制失败: " + error, 5000);
        return;
    }
    m_isTaskRunning = true;
    m_scriptPage->setRunningState(true);
}

void MainWindow::onScriptRecordStop()
{
    m_controller->taskManager()->requestStop();
}

void MainWindow::onScriptPlaybackStart()
{
    if (!m_controller->taskManager()->canStartTask()) return;
    // 从 ScriptPage 获取当前脚本和回放设置
    ScriptDocument doc = m_scriptPage->currentDocument();
    PlaybackSettings settings = m_scriptPage->playbackSettings();

    if (doc.isEmpty()) {
        statusBar()->showMessage("脚本事件为空，请先录制或导入脚本", 5000);
        return;
    }

    QString error;
    if (!m_controller->taskManager()->requestStartPlayback(doc, settings, &error)) {
        statusBar()->showMessage("启动回放失败: " + error, 5000);
        return;
    }
    m_isTaskRunning = true;
    m_scriptPage->setRunningState(true);
}

void MainWindow::onScriptPlaybackStop()
{
    m_controller->taskManager()->requestStop();
}

void MainWindow::onEmergencyStop()
{
    statusBar()->showMessage("紧急停止已触发！所有输入已释放", 10000);
    m_isTaskRunning = false;
    m_mousePage->setRunningState(false);
    m_keyboardPage->setRunningState(false);
    m_scriptPage->setRunningState(false);
}

void MainWindow::restoreDefaults()
{
    const auto answer = QMessageBox::warning(this, "恢复默认设置",
        "确定要停止当前任务，并恢复所有参数、快捷键和窗口设置吗？\n\n"
        "当前脚本文档和磁盘上的脚本文件将保留。",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    // 先停止录制并接收文档，再重载配置，避免丢失录制结果或留下按下状态。
    m_controller->resetInputState();
    m_controller->unregisterAllHotkeys();
    m_settings->resetAll();
    m_mousePage->loadSettings();
    m_keyboardPage->loadSettings();
    m_scriptPage->loadSettings();
    if (isMaximized() || isMinimized()) showNormal();
    loadSettings();
    saveSettings();
    statusBar()->showMessage("所有设置已恢复为默认值", 5000);
    LOG_INFO("所有设置已恢复为默认值");
}

void MainWindow::exitApplication()
{
    m_controller->emergencyStop();
    if (!m_scriptPage->confirmDiscardChanges()) return;
    saveSettings();
    m_trayIcon->hide();
    hide();
    QApplication::quit();
}

void MainWindow::openGitHub()
{
    if (!QDesktopServices::openUrl(QUrl(QString::fromLatin1(KmmProjectUrl)))) {
        LOG_WARNING("无法打开 GitHub 项目主页");
        QMessageBox::warning(this, "打开链接", "无法打开 GitHub 项目主页，请检查默认浏览器设置。");
    }
}

void MainWindow::openUserGuide()
{
    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath("docs/user-guide.html");
    if (!QFileInfo(path).isFile()) {
        LOG_WARNING("未找到本地使用说明文件: " + path);
        QMessageBox::warning(this, "使用说明", "未找到本地使用说明文件。");
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        LOG_WARNING("无法打开本地使用说明: " + path);
        QMessageBox::warning(this, "使用说明", "无法打开使用说明，请检查默认浏览器设置。");
    }
}

void MainWindow::setAlwaysOnTop(bool enabled)
{
    const QRect previousGeometry = geometry();
    const auto previousState = windowState();
    const bool visible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
    setGeometry(previousGeometry);
    setWindowState(previousState);
    if (visible) show();
    // Windows 改窗口标志可能重建 HWND，Hook 的自身窗口过滤也必须同步。
    m_controller->hookManager()->setOwnWindowHandle(reinterpret_cast<void*>(winId()));
    m_settings->setAlwaysOnTop(enabled);
}

// ============================================================================
// 事件处理
// ============================================================================
bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // 阻止 QAbstractSpinBox 和 QComboBox 响应鼠标滚轮
    if (event->type() == QEvent::Wheel) {
        if (qobject_cast<QAbstractSpinBox*>(obj) || qobject_cast<QComboBox*>(obj)) {
            return true; // 吞噬滚轮事件
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    TrayCloseBehavior behavior = static_cast<TrayCloseBehavior>(m_settings->trayCloseBehavior());

    if (behavior == TrayCloseBehavior::MinimizeToTray) {
        event->ignore();
        hide();
        return;
    }

    if (behavior == TrayCloseBehavior::AskUser) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("键鼠大师");
        msgBox.setText("请选择关闭行为：");
        msgBox.setInformativeText("最小化到托盘后，双击托盘图标可以恢复窗口。");

        QPushButton* exitBtn = msgBox.addButton("直接退出", QMessageBox::AcceptRole);
        QPushButton* trayBtn = msgBox.addButton("最小化到托盘", QMessageBox::RejectRole);
        QPushButton* cancelBtn = msgBox.addButton("取消", QMessageBox::DestructiveRole);

        msgBox.setDefaultButton(cancelBtn);
        msgBox.exec();

        if (msgBox.clickedButton() == trayBtn) {
            event->ignore();
            hide();
            return;
        } else if (msgBox.clickedButton() != exitBtn) {
            event->ignore();
            return;
        }
    }

    // 直接退出
    if (m_isTaskRunning) {
        m_controller->emergencyStop();
    }

    // 停止录制后文档才到达页面，此时检查未保存内容。
    if (!m_scriptPage->confirmDiscardChanges()) { event->ignore(); return; }

    saveSettings();
    m_trayIcon->hide();
    event->accept();
    QApplication::quit();
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    updateStatusBar();
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isMinimized()
        && m_settings->minimizeToTray() && QSystemTrayIcon::isSystemTrayAvailable())
        QTimer::singleShot(0, this, &QWidget::hide);
}

void MainWindow::onTaskStateChanged(TaskState state)
{
    updateTrayIcon(state);
    const bool canStart = m_controller->taskManager()->canStartTask();
    for (auto* action : m_runActions) action->setEnabled(canStart);
    for (auto* action : m_scriptFileActions) action->setEnabled(canStart);

    bool running = (state == TaskState::MouseClicking
                    || state == TaskState::KeyboardClicking
                    || state == TaskState::Recording
                    || state == TaskState::Playing
                    || state == TaskState::Paused
                    || state == TaskState::Preparing || state == TaskState::Stopping);

    if (running) {
        // 任务启动 — 同步更新对应页面的按钮状态
        m_isTaskRunning = true;
        switch (state) {
        case TaskState::MouseClicking:
            m_mousePage->setRunningState(true);
            break;
        case TaskState::KeyboardClicking:
            m_keyboardPage->setRunningState(true);
            break;
        case TaskState::Recording:
        case TaskState::Playing:
        case TaskState::Paused:
            m_scriptPage->setRunningState(true);
            break;
        default: break;
        }
    } else if (m_isTaskRunning) {
        // 任务结束 — 复位所有页面按钮状态
        m_isTaskRunning = false;
        m_mousePage->setRunningState(false);
        m_keyboardPage->setRunningState(false);
        m_scriptPage->setRunningState(false);

    }

    m_mousePage->setEnabled(!running || state == TaskState::MouseClicking || state == TaskState::Stopping);
    m_keyboardPage->setEnabled(!running || state == TaskState::KeyboardClicking || state == TaskState::Stopping);
    m_scriptPage->setEnabled(!running || state == TaskState::Recording || state == TaskState::Playing || state == TaskState::Paused || state == TaskState::Stopping);
    m_stateLabel->setText(taskStateToString(state));
    m_trayStartAction->setEnabled(!running);
    m_trayStopAction->setEnabled(running);
}

void MainWindow::onTabChanged(int index)
{
    Q_UNUSED(index)
    saveSettings();
}

void MainWindow::updateStatusBar()
{
    if (!m_controller || !m_controller->monitorManager()) return;

    QPoint pos = m_controller->monitorManager()->currentCursorPos();
    m_cursorLabel->setText(QString("坐标: (%1, %2)").arg(pos.x()).arg(pos.y()));

    MonitorInfo monitor = m_controller->monitorManager()->currentCursorMonitor();
    if (!monitor.deviceName.isEmpty()) {
        m_monitorLabel->setText(
            QString("显示器: 屏%1").arg(monitor.index));
    }

    // 更新各页面
    m_mousePage->updateCursorInfo();

    // 更新状态文本
    TaskState state = m_controller->taskManager()->currentState();
    m_stateLabel->setText(taskStateToString(state));
}
