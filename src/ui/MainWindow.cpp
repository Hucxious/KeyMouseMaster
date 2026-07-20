#include "MainWindow.h"
#include "MouseClickPage.h"
#include "KeyboardClickPage.h"
#include "ScriptPage.h"
#include "core/AppController.h"
#include "core/TaskManager.h"
#include "core/MonitorManager.h"
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

    // 启动状态更新定时器
    m_statusUpdateTimer = new QTimer(this);
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusUpdateTimer->start(200); // 每200ms更新

    LOG_INFO("主窗口初始化完成");
}

MainWindow::~MainWindow()
{
    saveSettings();
    m_statusUpdateTimer->stop();
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
    // 文件菜单
    m_fileMenu = menuBar()->addMenu("文件(&F)");

    QAction* exitAction = m_fileMenu->addAction("退出(&X)");
    exitAction->setShortcut(QKeySequence("Alt+F4"));
    connect(exitAction, &QAction::triggered, this, [this]() {
        close();
    });

    // 视图菜单
    m_viewMenu = menuBar()->addMenu("视图(&V)");

    QAction* mouseTabAction = m_viewMenu->addAction("鼠标连点");
    connect(mouseTabAction, &QAction::triggered, this, [this]() {
        m_tabWidget->setCurrentIndex(0);
    });

    QAction* keyboardTabAction = m_viewMenu->addAction("键盘连点");
    connect(keyboardTabAction, &QAction::triggered, this, [this]() {
        m_tabWidget->setCurrentIndex(1);
    });

    QAction* scriptTabAction = m_viewMenu->addAction("脚本录制");
    connect(scriptTabAction, &QAction::triggered, this, [this]() {
        m_tabWidget->setCurrentIndex(2);
    });

    m_viewMenu->addSeparator();

    QAction* resetInputAction = m_viewMenu->addAction("输入状态复位(&R)");
    resetInputAction->setShortcut(QKeySequence("Ctrl+Shift+R"));
    connect(resetInputAction, &QAction::triggered, this, &MainWindow::onResetInputState);

    // 帮助菜单
    m_helpMenu = menuBar()->addMenu("帮助(&H)");

    QAction* aboutAction = m_helpMenu->addAction("关于(&A)");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "关于键鼠大师",
            "键鼠大师 KeyMouseMaster \n "
            "版本号：v1.0\n"
            "作者：Hucxious Assisted by DeepSeekV4\n"
            "QQ：2454100241\n"
            "Windows键鼠自动化工具\n\n"
            "功能：鼠标连点 | 键盘连点 | 脚本录制回放\n"
            "支持多显示器、高DPI、全局快捷键");
    });
}

void MainWindow::setupSystemTray()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icons/app.png"));
    m_trayIcon->setToolTip("键鼠大师 - 空闲");

    m_trayMenu = new QMenu(this);

    m_trayShowAction = m_trayMenu->addAction("显示主窗口");
    connect(m_trayShowAction, &QAction::triggered, this, [this]() {
        show();
        raise();
        activateWindow();
    });

    m_trayMenu->addSeparator();

    m_trayStartAction = m_trayMenu->addAction("启动当前任务");
    m_trayStopAction = m_trayMenu->addAction("停止当前任务");
    m_trayStopAction->setEnabled(false);

    m_trayMenu->addSeparator();

    m_trayResetAction = m_trayMenu->addAction("输入状态复位");
    connect(m_trayResetAction, &QAction::triggered, this, &MainWindow::onResetInputState);

    m_trayMenu->addSeparator();

    m_trayExitAction = m_trayMenu->addAction("退出");
    connect(m_trayExitAction, &QAction::triggered, this, [this]() {
        m_settings->setTrayCloseBehavior(static_cast<int>(TrayCloseBehavior::Exit));
        close();
    });

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
}

// ============================================================================
// 设置
// ============================================================================
void MainWindow::loadSettings()
{
    QSize size = m_settings->windowSize();
    if (size.isValid()) resize(size);

    QPoint pos = m_settings->windowPosition();
    if (pos.x() >= 0 && pos.y() >= 0) move(pos);

    int tabIndex = m_settings->currentTabIndex();
    if (tabIndex >= 0 && tabIndex < m_tabWidget->count())
        m_tabWidget->setCurrentIndex(tabIndex);
}

void MainWindow::saveSettings()
{
    m_settings->setWindowSize(size());
    m_settings->setWindowPosition(pos());
    m_settings->setCurrentTabIndex(m_tabWidget->currentIndex());

    m_mousePage->saveSettings();
    m_keyboardPage->saveSettings();
    m_scriptPage->saveSettings();
    m_settings->sync();
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
            show();
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
    QString error;
    if (!m_controller->taskManager()->requestStartMouseClick(&error)) {
        statusBar()->showMessage("启动失败: " + error, 5000);
        return;
    }
    m_isTaskRunning = true;
    m_mousePage->setRunningState(true);
}

void MainWindow::onMousePageStop()
{
    m_controller->taskManager()->requestStop();
}

void MainWindow::onKeyboardPageStart()
{
    QString error;
    if (!m_controller->taskManager()->requestStartKeyboardClick(&error)) {
        statusBar()->showMessage("启动失败: " + error, 5000);
        return;
    }
    m_isTaskRunning = true;
    m_keyboardPage->setRunningState(true);
}

void MainWindow::onKeyboardPageStop()
{
    m_controller->taskManager()->requestStop();
}

void MainWindow::onScriptRecordStart()
{
    RecordingSettings settings;
    settings.recordMouseMove  = m_scriptPage->findChild<QCheckBox*>("") ? true : true;
    // 从 UI 控件读取录制设置
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
    // 从脚本页面获取当前脚本和回放设置
    QString error;
    ScriptDocument doc; // 从ScriptPage获取
    PlaybackSettings settings;

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

void MainWindow::onResetInputState()
{
    m_controller->resetInputState();
    statusBar()->showMessage("输入状态已复位", 3000);
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
        msgBox.setInformativeText("是否记住此选择？");

        QPushButton* exitBtn = msgBox.addButton("直接退出", QMessageBox::AcceptRole);
        QPushButton* trayBtn = msgBox.addButton("最小化到托盘", QMessageBox::RejectRole);
        QPushButton* cancelBtn = msgBox.addButton("取消", QMessageBox::DestructiveRole);

        msgBox.setDefaultButton(cancelBtn);
        msgBox.exec();

        if (msgBox.clickedButton() == trayBtn) {
            event->ignore();
            hide();
            return;
        } else if (msgBox.clickedButton() == cancelBtn) {
            event->ignore();
            return;
        }
    }

    // 直接退出
    if (m_isTaskRunning) {
        m_controller->emergencyStop();
    }

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

void MainWindow::onTaskStateChanged(TaskState state)
{
    updateTrayIcon(state);

    bool running = (state == TaskState::MouseClicking
                    || state == TaskState::KeyboardClicking
                    || state == TaskState::Recording
                    || state == TaskState::Playing
                    || state == TaskState::Paused);

    if (!running && m_isTaskRunning) {
        // 任务结束
        m_isTaskRunning = false;
        m_mousePage->setRunningState(false);
        m_keyboardPage->setRunningState(false);
        m_scriptPage->setRunningState(false);

        // 录制停止后获取事件
        if (state == TaskState::Idle || state == TaskState::Completed) {
            // 从录制器获取文档并更新脚本页面
        }
    }

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
