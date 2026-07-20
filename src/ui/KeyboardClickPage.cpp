#include "KeyboardClickPage.h"
#include "ui/KeyCaptureEdit.h"
#include "ui/HotkeyEdit.h"
#include "core/AppController.h"
#include "settings/SettingsManager.h"
#include "utils/ValidationUtils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>

KeyboardClickPage::KeyboardClickPage(AppController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_settings(controller->settingsManager())
{
    setupUI();
    connectSignals();
    loadSettings();
}

void KeyboardClickPage::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    // 状态栏
    auto* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #4CAF50; }");
    m_validationLabel = new QLabel();
    m_validationLabel->setStyleSheet("QLabel { color: #f44336; }");
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_validationLabel);
    mainLayout->addLayout(statusLayout);

    // 按键捕获
    auto* keyGroup = new QGroupBox("目标按键");
    auto* keyLayout = new QVBoxLayout(keyGroup);
    auto* captureLayout = new QHBoxLayout();
    captureLayout->addWidget(new QLabel("按键:"));
    m_keyCaptureEdit = new KeyCaptureEdit();
    m_keyCaptureEdit->setMinimumHeight(32);
    captureLayout->addWidget(m_keyCaptureEdit, 1);
    keyLayout->addLayout(captureLayout);

    m_keyDetailLabel = new QLabel("请点击上方输入框捕获按键");
    m_keyDetailLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    keyLayout->addWidget(m_keyDetailLabel);
    mainLayout->addWidget(keyGroup);

    // 输入模式
    auto* modeGroup = new QGroupBox("输入模式");
    auto* modeLayout = new QFormLayout(modeGroup);
    m_inputModeCombo = new QComboBox();
    m_inputModeCombo->setMinimumWidth(220);
    m_inputModeCombo->addItem("普通单键 - 按下后立即释放", static_cast<int>(KeyInputMode::Normal));
    m_inputModeCombo->addItem("组合键 - 同时按下多个键", static_cast<int>(KeyInputMode::Combo));
    m_inputModeCombo->addItem("仅按下 - 只按下不释放", static_cast<int>(KeyInputMode::PressOnly));
    m_inputModeCombo->addItem("仅释放 - 只释放不按下", static_cast<int>(KeyInputMode::ReleaseOnly));
    m_inputModeCombo->addItem("完整按键 - 按压后释放", static_cast<int>(KeyInputMode::FullKey));
    m_inputModeCombo->addItem("长按 - 长时间按压", static_cast<int>(KeyInputMode::Hold));
    modeLayout->addRow("模式:", m_inputModeCombo);
    mainLayout->addWidget(modeGroup);

    // 时间参数 (2列紧凑布局)
    auto* timeGroup = new QGroupBox("时间参数");
    auto* timeLayout = new QGridLayout(timeGroup);
    timeLayout->setSpacing(4);

    // 第1行：按键间隔 | 按压时长
    auto* intervalCol = new QHBoxLayout();
    intervalCol->addWidget(new QLabel("按键间隔:"));
    m_intervalSpinBox = new QSpinBox();
    m_intervalSpinBox->setRange(1, 3600000);
    m_intervalSpinBox->setValue(100);
    m_intervalSpinBox->setSuffix(" ms");
    m_intervalSpinBox->setMaximumWidth(90);
    intervalCol->addWidget(m_intervalSpinBox);
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

    // 第2行：启动延迟
    auto* delayCol = new QHBoxLayout();
    delayCol->addWidget(new QLabel("启动延迟:"));
    m_startDelaySpinBox = new QSpinBox();
    m_startDelaySpinBox->setRange(0, 60000);
    m_startDelaySpinBox->setValue(0);
    m_startDelaySpinBox->setSuffix(" ms");
    m_startDelaySpinBox->setMaximumWidth(90);
    delayCol->addWidget(m_startDelaySpinBox);
    delayCol->addStretch();
    timeLayout->addLayout(delayCol, 1, 0);

    mainLayout->addWidget(timeGroup);

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

    mainLayout->addWidget(execGroup);

    // 快捷键
    auto* hotkeyGroup = new QGroupBox("快捷键");
    auto* hotkeyLayout = new QFormLayout(hotkeyGroup);
    m_startHotkeyEdit = new HotkeyEdit();
    m_stopHotkeyEdit = new HotkeyEdit();
    hotkeyLayout->addRow("启动快捷键:", m_startHotkeyEdit);
    hotkeyLayout->addRow("停止快捷键:", m_stopHotkeyEdit);
    mainLayout->addWidget(hotkeyGroup);

    // 操作按钮
    auto* btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("▶ 启动");
    m_startBtn->setMinimumHeight(30);
    m_startBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; font-size: 13px; border-radius: 4px; } QPushButton:hover { background-color: #45a049; } QPushButton:disabled { background-color: #cccccc; }");
    m_stopBtn = new QPushButton("⏹ 停止");
    m_stopBtn->setMinimumHeight(30);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; font-size: 13px; border-radius: 4px; } QPushButton:hover { background-color: #da190b; } QPushButton:disabled { background-color: #cccccc; }");
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_stopBtn);
    mainLayout->addLayout(btnLayout);

    mainLayout->addStretch();
}

void KeyboardClickPage::connectSignals()
{
    connect(m_startBtn, &QPushButton::clicked, this, &KeyboardClickPage::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &KeyboardClickPage::onStopClicked);
    connect(m_inputModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &KeyboardClickPage::onInputModeChanged);
    connect(m_infiniteCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_repeatCountSpinBox->setEnabled(!checked);
    });
    connect(m_keyCaptureEdit, &KeyCaptureEdit::keyInfoChanged, this, [this](const KeyInfo& info) {
        if (info.isValid()) {
            m_keyDetailLabel->setText(QString("Qt键值: 0x%1 | VK: 0x%2 | 扫描码: 0x%3")
                .arg(info.qtKey, 0, 16)
                .arg(info.winVk, 2, 16, QChar('0'))
                .arg(info.scanCode, 2, 16, QChar('0')));
            m_keyDetailLabel->setStyleSheet("QLabel { color: #4CAF50; font-size: 11px; }");
        } else {
            m_keyDetailLabel->setText("请点击上方输入框捕获按键");
            m_keyDetailLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
        }
    });
}

void KeyboardClickPage::loadSettings()
{
    m_inputModeCombo->setCurrentIndex(m_settings->keyboardInputMode());
    m_intervalSpinBox->setValue(m_settings->keyboardInterval());
    m_pressDurationSpinBox->setValue(m_settings->keyboardPressDuration());
    m_startDelaySpinBox->setValue(m_settings->keyboardStartDelay());
    m_repeatCountSpinBox->setValue(m_settings->keyboardRepeatCount());
    m_infiniteCheckBox->setChecked(m_settings->keyboardInfinite());

    m_startHotkeyEdit->setHotkey(m_settings->keyboardStartHotkey());
    m_stopHotkeyEdit->setHotkey(m_settings->keyboardStopHotkey());

    // 恢复上次按键
    QString keyDisplay = m_settings->keyboardKeyDisplay();
    if (!keyDisplay.isEmpty()) {
        KeyInfo info;
        info.qtKey = m_settings->keyboardQtKey();
        info.displayName = keyDisplay;
        m_keyCaptureEdit->setKeyInfo(info);
    }
}

void KeyboardClickPage::saveSettings()
{
    m_settings->setKeyboardInputMode(m_inputModeCombo->currentIndex());
    m_settings->setKeyboardInterval(m_intervalSpinBox->value());
    m_settings->setKeyboardPressDuration(m_pressDurationSpinBox->value());
    m_settings->setKeyboardStartDelay(m_startDelaySpinBox->value());
    m_settings->setKeyboardRepeatCount(m_repeatCountSpinBox->value());
    m_settings->setKeyboardInfinite(m_infiniteCheckBox->isChecked());
    m_settings->setKeyboardQtKey(m_keyCaptureEdit->keyInfo().qtKey);
    m_settings->setKeyboardKeyDisplay(m_keyCaptureEdit->keyInfo().displayName);
    m_settings->setKeyboardStartHotkey(m_startHotkeyEdit->hotkey());
    m_settings->setKeyboardStopHotkey(m_stopHotkeyEdit->hotkey());
    m_settings->sync();
}

void KeyboardClickPage::onStartClicked()
{
    saveSettings();

    // 校验
    KeyInfo keyInfo = m_keyCaptureEdit->keyInfo();
    if (!keyInfo.isValid()) {
        emit statusMessage("请先捕获目标按键");
        return;
    }

    validateAndUpdate();
    emit startRequested();
}

void KeyboardClickPage::onStopClicked()
{
    emit stopRequested();
}

void KeyboardClickPage::onInputModeChanged(int index)
{
    KeyInputMode mode = static_cast<KeyInputMode>(index);
    bool showPressDuration = (mode == KeyInputMode::Normal
                              || mode == KeyInputMode::FullKey
                              || mode == KeyInputMode::Hold);
    m_pressDurationSpinBox->setVisible(showPressDuration);

    // 完整按键模式：提示按压时长 < 按键间隔
    if (mode == KeyInputMode::FullKey) {
        m_validationLabel->setText("注意：完整按键模式下，按压时长必须小于按键间隔");
    } else {
        m_validationLabel->clear();
    }
}

void KeyboardClickPage::setRunningState(bool running)
{
    m_startBtn->setEnabled(!running);
    m_stopBtn->setEnabled(running);

    m_keyCaptureEdit->setEnabled(!running);
    m_inputModeCombo->setEnabled(!running);
    m_intervalSpinBox->setEnabled(!running);
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

void KeyboardClickPage::validateAndUpdate()
{
    QString error;
    int intervalMs = m_intervalSpinBox->value();
    int pressDuration = m_pressDurationSpinBox->value();
    KeyInputMode mode = static_cast<KeyInputMode>(m_inputModeCombo->currentIndex());

    if (!ValidationUtils::validateClickInterval(intervalMs, 1, &error)) {
        m_validationLabel->setText(error);
        return;
    }

    if (mode == KeyInputMode::FullKey) {
        if (!ValidationUtils::validatePressVsInterval(pressDuration, intervalMs, &error)) {
            m_validationLabel->setText(error);
            return;
        }
    }

    if (!m_infiniteCheckBox->isChecked()
        && !ValidationUtils::validateRepeatCount(m_repeatCountSpinBox->value(), false, &error)) {
        m_validationLabel->setText(error);
        return;
    }

    m_validationLabel->clear();
}
