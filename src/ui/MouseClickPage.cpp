#include "MouseClickPage.h"
#include "ui/HotkeyEdit.h"
#include "ui/MonitorPreviewWidget.h"
#include "core/AppController.h"
#include "core/MonitorManager.h"
#include "settings/SettingsManager.h"
#include "utils/ValidationUtils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QMessageBox>

MouseClickPage::MouseClickPage(AppController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_settings(controller->settingsManager())
{
    setupUI();
    connectSignals();
    loadSettings();
}

// ============================================================================
// UI 构建
// ============================================================================
void MouseClickPage::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    // ---- 顶部：状态栏 ----
    auto* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #4CAF50; }");
    m_cursorPosLabel = new QLabel("坐标: --");
    m_currentMonitorLabel = new QLabel("显示器: --");
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_cursorPosLabel);
    statusLayout->addWidget(m_currentMonitorLabel);
    mainLayout->addLayout(statusLayout);

    // 使用 QSplitter 分左右
    auto* splitter = new QSplitter(Qt::Horizontal);

    // ---- 左侧：参数设置 ----
    auto* leftWidget = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 4, 0);

    // 按键和模式设置
    auto* btnGroup = new QGroupBox("鼠标按键和模式");
    auto* btnLayout = new QGridLayout(btnGroup);
    btnLayout->setSpacing(4);
    btnLayout->addWidget(new QLabel("鼠标按键:"), 0, 0);
    m_buttonCombo = new QComboBox();
    m_buttonCombo->setMinimumWidth(90);
    m_buttonCombo->addItem("左键", static_cast<int>(MouseButton::Left));
    m_buttonCombo->addItem("右键", static_cast<int>(MouseButton::Right));
    m_buttonCombo->addItem("中键", static_cast<int>(MouseButton::Middle));
    m_buttonCombo->addItem("侧键1", static_cast<int>(MouseButton::XButton1));
    m_buttonCombo->addItem("侧键2", static_cast<int>(MouseButton::XButton2));
    btnLayout->addWidget(m_buttonCombo, 0, 1);

    btnLayout->addWidget(new QLabel("点击模式:"), 1, 0);
    m_modeCombo = new QComboBox();
    m_modeCombo->setMinimumWidth(90);
    m_modeCombo->addItem("单击", static_cast<int>(ClickMode::Single));
    m_modeCombo->addItem("双击", static_cast<int>(ClickMode::Double));
    m_modeCombo->addItem("仅按下", static_cast<int>(ClickMode::PressOnly));
    m_modeCombo->addItem("仅释放", static_cast<int>(ClickMode::ReleaseOnly));
    m_modeCombo->addItem("长按", static_cast<int>(ClickMode::Hold));
    btnLayout->addWidget(m_modeCombo, 1, 1);
    leftLayout->addWidget(btnGroup);

    // 坐标模式
    auto* coordGroup = new QGroupBox("点击位置");
    auto* coordLayout = new QVBoxLayout(coordGroup);

    m_coordModeCombo = new QComboBox();
    m_coordModeCombo->setMinimumWidth(170);
    m_coordModeCombo->addItem("当前鼠标位置", static_cast<int>(CoordinateMode::CurrentCursor));
    m_coordModeCombo->addItem("固定虚拟桌面坐标", static_cast<int>(CoordinateMode::VirtualDesktopAbsolute));
    m_coordModeCombo->addItem("显示器相对坐标", static_cast<int>(CoordinateMode::MonitorRelative));
    coordLayout->addWidget(m_coordModeCombo);

    // 固定坐标输入 (默认隐藏)
    m_fixedPosGroup = new QWidget();
    auto* fixedLayout = new QHBoxLayout(m_fixedPosGroup);
    fixedLayout->setContentsMargins(0, 0, 0, 0);
    fixedLayout->addWidget(new QLabel("X:"));
    m_fixedXEdit = new QLineEdit("0");
    m_fixedXEdit->setMaximumWidth(80);
    fixedLayout->addWidget(m_fixedXEdit);
    fixedLayout->addWidget(new QLabel("Y:"));
    m_fixedYEdit = new QLineEdit("0");
    m_fixedYEdit->setMaximumWidth(80);
    fixedLayout->addWidget(m_fixedYEdit);
    m_getPosBtn = new QPushButton("获取当前位置");
    fixedLayout->addWidget(m_getPosBtn);
    fixedLayout->addStretch();
    m_fixedPosGroup->setVisible(false);
    coordLayout->addWidget(m_fixedPosGroup);

    // 显示器选择 (默认隐藏)
    m_monitorSelectGroup = new QWidget();
    auto* monitorLayout = new QFormLayout(m_monitorSelectGroup);
    m_monitorCombo = new QComboBox();
    monitorLayout->addRow("目标显示器:", m_monitorCombo);
    auto* internalLayout = new QHBoxLayout();
    m_monitorXEdit = new QLineEdit("0");
    m_monitorXEdit->setMaximumWidth(80);
    m_monitorYEdit = new QLineEdit("0");
    m_monitorYEdit->setMaximumWidth(80);
    internalLayout->addWidget(new QLabel("X:"));
    internalLayout->addWidget(m_monitorXEdit);
    internalLayout->addWidget(new QLabel("Y:"));
    internalLayout->addWidget(m_monitorYEdit);
    internalLayout->addStretch();
    monitorLayout->addRow("屏幕内坐标:", internalLayout);
    m_monitorSelectGroup->setVisible(false);
    coordLayout->addWidget(m_monitorSelectGroup);

    leftLayout->addWidget(coordGroup);

    // 时间参数 (2x2 紧凑布局)
    auto* timeGroup = new QGroupBox("时间参数");
    auto* timeLayout = new QGridLayout(timeGroup);
    timeLayout->setSpacing(4);

    // 第1行：点击间隔 | 按压时长
    auto* intervalCol = new QHBoxLayout();
    intervalCol->addWidget(new QLabel("点击间隔:"));
    m_intervalSpinBox = new QSpinBox();
    m_intervalSpinBox->setRange(1, 3600000);
    m_intervalSpinBox->setValue(100);
    m_intervalSpinBox->setSuffix(" ");
    m_intervalSpinBox->setMaximumWidth(80);
    intervalCol->addWidget(m_intervalSpinBox);
    m_intervalUnitCombo = new QComboBox();
    m_intervalUnitCombo->setMinimumWidth(55);
    m_intervalUnitCombo->addItem("毫秒", 1);
    m_intervalUnitCombo->addItem("秒", 1000);
    intervalCol->addWidget(m_intervalUnitCombo);
    intervalCol->addStretch();
    timeLayout->addLayout(intervalCol, 0, 0);

    auto* pressCol = new QHBoxLayout();
    pressCol->addWidget(new QLabel("按压时长:"));
    m_pressDurationSpinBox = new QSpinBox();
    m_pressDurationSpinBox->setRange(1, 60000);
    m_pressDurationSpinBox->setValue(50);
    m_pressDurationSpinBox->setSuffix(" ms");
    m_pressDurationSpinBox->setMaximumWidth(90);
    pressCol->addWidget(m_pressDurationSpinBox);
    pressCol->addStretch();
    timeLayout->addLayout(pressCol, 0, 1);

    // 第2行：双击间隔 | 启动延迟
    auto* doubleCol = new QHBoxLayout();
    doubleCol->addWidget(new QLabel("双击间隔:"));
    m_doubleClickIntervalSpinBox = new QSpinBox();
    m_doubleClickIntervalSpinBox->setRange(1, 10000);
    m_doubleClickIntervalSpinBox->setValue(100);
    m_doubleClickIntervalSpinBox->setSuffix(" ms");
    m_doubleClickIntervalSpinBox->setMaximumWidth(90);
    doubleCol->addWidget(m_doubleClickIntervalSpinBox);
    doubleCol->addStretch();
    timeLayout->addLayout(doubleCol, 1, 0);

    auto* delayCol = new QHBoxLayout();
    delayCol->addWidget(new QLabel("启动延迟:"));
    m_startDelaySpinBox = new QSpinBox();
    m_startDelaySpinBox->setRange(0, 60000);
    m_startDelaySpinBox->setValue(0);
    m_startDelaySpinBox->setSuffix(" ms");
    m_startDelaySpinBox->setMaximumWidth(90);
    delayCol->addWidget(m_startDelaySpinBox);
    delayCol->addStretch();
    timeLayout->addLayout(delayCol, 1, 1);

    leftLayout->addWidget(timeGroup);

    // 执行参数
    auto* execGroup = new QGroupBox("执行参数");
    auto* execLayout = new QGridLayout(execGroup);
    execLayout->setSpacing(4);

    execLayout->addWidget(new QLabel("执行次数:"), 0, 0);
    auto* countLayout = new QHBoxLayout();
    m_repeatCountSpinBox = new QSpinBox();
    m_repeatCountSpinBox->setRange(1, 999999);
    m_repeatCountSpinBox->setValue(1);
    countLayout->addWidget(m_repeatCountSpinBox);
    m_infiniteCheckBox = new QCheckBox("无限循环");
    countLayout->addWidget(m_infiniteCheckBox);
    countLayout->addStretch();
    execLayout->addLayout(countLayout, 0, 1);

    m_restoreCursorCheckBox = new QCheckBox("完成后恢复原鼠标位置");
    execLayout->addWidget(m_restoreCursorCheckBox, 1, 0, 1, 2);

    leftLayout->addWidget(execGroup);

    // 快捷键
    auto* hotkeyGroup = new QGroupBox("快捷键");
    auto* hotkeyLayout = new QFormLayout(hotkeyGroup);
    m_startHotkeyEdit = new HotkeyEdit();
    m_stopHotkeyEdit = new HotkeyEdit();
    hotkeyLayout->addRow("启动快捷键:", m_startHotkeyEdit);
    hotkeyLayout->addRow("停止快捷键:", m_stopHotkeyEdit);
    leftLayout->addWidget(hotkeyGroup);

    // 操作按钮
    auto* btnLayout2 = new QHBoxLayout();
    m_startBtn = new QPushButton("▶ 启动");
    m_startBtn->setMinimumHeight(30);
    m_startBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; font-size: 13px; border-radius: 4px; } QPushButton:hover { background-color: #45a049; } QPushButton:disabled { background-color: #cccccc; }");
    m_stopBtn = new QPushButton("⏹ 停止");
    m_stopBtn->setMinimumHeight(30);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; font-size: 13px; border-radius: 4px; } QPushButton:hover { background-color: #da190b; } QPushButton:disabled { background-color: #cccccc; }");
    btnLayout2->addWidget(m_startBtn);
    btnLayout2->addWidget(m_stopBtn);
    leftLayout->addLayout(btnLayout2);

    leftLayout->addStretch();

    // ---- 右侧：显示器预览 ----
    auto* rightWidget = new QWidget();
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(4, 0, 0, 0);

    auto* previewGroup = new QGroupBox("显示器布局预览");
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_monitorPreview = new MonitorPreviewWidget();
    previewLayout->addWidget(m_monitorPreview);
    rightLayout->addWidget(previewGroup);
    rightLayout->addStretch();

    // 添加到 Splitter
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter, 1);
}

// ============================================================================
// 信号连接
// ============================================================================
void MouseClickPage::connectSignals()
{
    connect(m_startBtn, &QPushButton::clicked, this, &MouseClickPage::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MouseClickPage::onStopClicked);
    connect(m_getPosBtn, &QPushButton::clicked, this, &MouseClickPage::onGetCursorPos);
    connect(m_coordModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MouseClickPage::onCoordinateModeChanged);
    connect(m_monitorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MouseClickPage::onMonitorSelectionChanged);
    connect(m_buttonCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MouseClickPage::onButtonChanged);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MouseClickPage::onModeChanged);
    connect(m_infiniteCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_repeatCountSpinBox->setEnabled(!checked);
    });
}

// ============================================================================
// 设置加载/保存
// ============================================================================
void MouseClickPage::loadSettings()
{
    m_buttonCombo->setCurrentIndex(m_settings->mouseButton());
    m_modeCombo->setCurrentIndex(m_settings->mouseClickMode());
    m_intervalSpinBox->setValue(m_settings->mouseClickInterval());
    m_intervalUnitCombo->setCurrentIndex(m_settings->mouseClickIntervalUnit());
    m_pressDurationSpinBox->setValue(m_settings->mousePressDuration());
    m_doubleClickIntervalSpinBox->setValue(m_settings->mouseDoubleClickInterval());
    m_startDelaySpinBox->setValue(m_settings->mouseStartDelay());
    m_repeatCountSpinBox->setValue(m_settings->mouseRepeatCount());
    m_infiniteCheckBox->setChecked(m_settings->mouseInfinite());
    m_restoreCursorCheckBox->setChecked(m_settings->mouseRestoreCursor());

    CoordinateMode cm = m_settings->mouseCoordinateMode();
    m_coordModeCombo->setCurrentIndex(static_cast<int>(cm));
    onCoordinateModeChanged(static_cast<int>(cm));

    QPoint fixedPos = m_settings->mouseFixedPos();
    m_fixedXEdit->setText(QString::number(fixedPos.x()));
    m_fixedYEdit->setText(QString::number(fixedPos.y()));

    m_startHotkeyEdit->setHotkey(m_settings->mouseStartHotkey());
    m_stopHotkeyEdit->setHotkey(m_settings->mouseStopHotkey());

    updateMonitorList();
}

void MouseClickPage::saveSettings()
{
    m_settings->setMouseButton(m_buttonCombo->currentIndex());
    m_settings->setMouseClickMode(m_modeCombo->currentIndex());
    m_settings->setMouseClickInterval(m_intervalSpinBox->value());
    m_settings->setMouseClickIntervalUnit(m_intervalUnitCombo->currentIndex());
    m_settings->setMousePressDuration(m_pressDurationSpinBox->value());
    m_settings->setMouseDoubleClickInterval(m_doubleClickIntervalSpinBox->value());
    m_settings->setMouseStartDelay(m_startDelaySpinBox->value());
    m_settings->setMouseRepeatCount(m_repeatCountSpinBox->value());
    m_settings->setMouseInfinite(m_infiniteCheckBox->isChecked());
    m_settings->setMouseRestoreCursor(m_restoreCursorCheckBox->isChecked());
    m_settings->setMouseCoordinateMode(
        static_cast<CoordinateMode>(m_coordModeCombo->currentIndex()));
    m_settings->setMouseFixedPos(QPoint(
        m_fixedXEdit->text().toInt(), m_fixedYEdit->text().toInt()));
    m_settings->setMouseStartHotkey(m_startHotkeyEdit->hotkey());
    m_settings->setMouseStopHotkey(m_stopHotkeyEdit->hotkey());
    m_settings->sync();
}

// ============================================================================
// 槽函数
// ============================================================================
void MouseClickPage::onStartClicked()
{
    saveSettings();
    validateAndUpdate();
    emit startRequested();
}

void MouseClickPage::onStopClicked()
{
    emit stopRequested();
}

void MouseClickPage::onGetCursorPos()
{
    QPoint pos = m_controller->monitorManager()->currentCursorPos();
    m_fixedXEdit->setText(QString::number(pos.x()));
    m_fixedYEdit->setText(QString::number(pos.y()));

    // 同时更新显示器信息
    MonitorInfo monitor = m_controller->monitorManager()->currentCursorMonitor();
    if (!monitor.deviceName.isEmpty()) {
        QPoint internal = monitor.virtualToInternal(pos);
        m_monitorXEdit->setText(QString::number(internal.x()));
        m_monitorYEdit->setText(QString::number(internal.y()));
    }
}

void MouseClickPage::onCoordinateModeChanged(int index)
{
    CoordinateMode mode = static_cast<CoordinateMode>(index);
    m_fixedPosGroup->setVisible(mode == CoordinateMode::VirtualDesktopAbsolute);
    m_monitorSelectGroup->setVisible(
        mode == CoordinateMode::MonitorRelative || mode == CoordinateMode::MonitorRatio);
}

void MouseClickPage::onMonitorSelectionChanged(int index)
{
    Q_UNUSED(index)
    QString deviceName = m_monitorCombo->currentData().toString();
    m_monitorPreview->setHighlightedMonitor(deviceName);
}

void MouseClickPage::onButtonChanged(int index)
{
    Q_UNUSED(index)
    saveSettings();
}

void MouseClickPage::onModeChanged(int index)
{
    ClickMode mode = static_cast<ClickMode>(index);
    bool showPressDuration = (mode == ClickMode::Hold || mode == ClickMode::Single);
    bool showDoubleClick = (mode == ClickMode::Double);
    m_pressDurationSpinBox->setVisible(showPressDuration);
    m_doubleClickIntervalSpinBox->setVisible(showDoubleClick);
}

void MouseClickPage::onIntervalUnitChanged(int index)
{
    Q_UNUSED(index)
}

void MouseClickPage::updateMonitorList()
{
    m_monitorCombo->clear();
    auto monitors = m_controller->monitorManager()->monitors();
    for (const auto& m : monitors) {
        QString label = QString("屏幕 %1 %2 %3x%4")
            .arg(m.index)
            .arg(m.isPrimary ? "(主)" : "")
            .arg(m.desktopRect.width())
            .arg(m.desktopRect.height());
        m_monitorCombo->addItem(label, m.deviceName);
    }
    m_monitorPreview->setMonitors(monitors);

    // 恢复上次选择的显示器
    QString savedDevice = m_settings->mouseMonitorDevice();
    if (!savedDevice.isEmpty()) {
        for (int i = 0; i < m_monitorCombo->count(); ++i) {
            if (m_monitorCombo->itemData(i).toString() == savedDevice) {
                m_monitorCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void MouseClickPage::setRunningState(bool running)
{
    m_startBtn->setEnabled(!running);
    m_stopBtn->setEnabled(running);

    // 运行期间锁定参数
    m_buttonCombo->setEnabled(!running);
    m_modeCombo->setEnabled(!running);
    m_coordModeCombo->setEnabled(!running);
    m_intervalSpinBox->setEnabled(!running);
    m_intervalUnitCombo->setEnabled(!running);
    m_repeatCountSpinBox->setEnabled(!running);
    m_infiniteCheckBox->setEnabled(!running);

    if (running) {
        m_statusLabel->setText("运行中...");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #FF9800; }");
    } else {
        m_statusLabel->setText("就绪");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #4CAF50; }");
    }
}

void MouseClickPage::updateCursorInfo()
{
    QPoint pos = m_controller->monitorManager()->currentCursorPos();
    m_cursorPosLabel->setText(QString("坐标: (%1, %2)").arg(pos.x()).arg(pos.y()));

    MonitorInfo monitor = m_controller->monitorManager()->currentCursorMonitor();
    if (!monitor.deviceName.isEmpty()) {
        m_currentMonitorLabel->setText(
            QString("显示器: 屏%1 %2x%3")
                .arg(monitor.index)
                .arg(monitor.desktopRect.width())
                .arg(monitor.desktopRect.height()));
    }

    m_monitorPreview->setCurrentCursorPos(pos);
}

void MouseClickPage::validateAndUpdate()
{
    // 校验参数
    QString error;
    int intervalMs = m_intervalSpinBox->value();

    if (!ValidationUtils::validateClickInterval(intervalMs, 1, &error)) {
        emit statusMessage(error);
        return;
    }
    if (!m_infiniteCheckBox->isChecked()
        && !ValidationUtils::validateRepeatCount(m_repeatCountSpinBox->value(), false, &error)) {
        emit statusMessage(error);
        return;
    }

    // 固定坐标模式检查坐标
    if (static_cast<CoordinateMode>(m_coordModeCombo->currentIndex())
        == CoordinateMode::VirtualDesktopAbsolute) {
        int x = m_fixedXEdit->text().toInt();
        int y = m_fixedYEdit->text().toInt();
        if (!ValidationUtils::validateCoordinate(x, y, &error)) {
            emit statusMessage(error);
            return;
        }
    }
}
