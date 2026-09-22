#include "KeyboardClickPage.h"
#include "ui/KeyCaptureEdit.h"
#include "ui/HotkeyEdit.h"
#include "core/AppController.h"
#include "settings/SettingsManager.h"
#include "utils/ValidationUtils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
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

    auto* keyGroup = new QGroupBox("按键设置");
    auto* keyLayout = new QGridLayout(keyGroup);
    m_keyCaptureEdit = new KeyCaptureEdit();
    m_keyCaptureEdit->setObjectName("keyboardKey");
    m_keyCaptureEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_inputModeCombo = new QComboBox();
    m_inputModeCombo->setObjectName("keyboardMode");
    m_inputModeCombo->addItem("普通按键", static_cast<int>(KeyInputMode::Normal));
    m_inputModeCombo->addItem("长按", static_cast<int>(KeyInputMode::Hold));
    keyLayout->addWidget(new QLabel("按键:"), 0, 0);
    keyLayout->addWidget(m_keyCaptureEdit, 0, 1);
    keyLayout->addWidget(new QLabel("输入模式:"), 0, 2);
    keyLayout->addWidget(m_inputModeCombo, 0, 3);
    keyLayout->setColumnStretch(1, 3);
    keyLayout->setColumnStretch(3, 1);
    m_keyDetailLabel = new QLabel("请点击上方输入框捕获按键");
    m_keyDetailLabel->setWordWrap(true);
    keyLayout->addWidget(m_keyDetailLabel, 1, 0, 1, 4);
    mainLayout->addWidget(keyGroup);

    auto* timeGroup = new QGroupBox("时间参数");
    auto* timeLayout = new QHBoxLayout(timeGroup);
    m_intervalSpinBox = new QSpinBox();
    m_intervalSpinBox->setObjectName("keyboardInterval");
    m_intervalSpinBox->setRange(1, 3600000);
    m_intervalSpinBox->setSuffix(" ms");
    m_pressDurationSpinBox = new QSpinBox();
    m_pressDurationSpinBox->setObjectName("keyboardHoldDuration");
    m_pressDurationSpinBox->setRange(1, 60000);
    m_pressDurationSpinBox->setSuffix(" ms");
    m_startDelaySpinBox = new QSpinBox();
    m_startDelaySpinBox->setObjectName("keyboardStartDelay");
    m_startDelaySpinBox->setRange(0, 60000);
    m_startDelaySpinBox->setSuffix(" ms");
    timeLayout->addWidget(new QLabel("按键间隔:"));
    timeLayout->addWidget(m_intervalSpinBox, 1);
    timeLayout->addWidget(new QLabel("长按时间:"));
    timeLayout->addWidget(m_pressDurationSpinBox, 1);
    timeLayout->addWidget(new QLabel("启动延迟:"));
    timeLayout->addWidget(m_startDelaySpinBox, 1);
    mainLayout->addWidget(timeGroup);

    auto* executionRow = new QHBoxLayout();
    auto* execGroup = new QGroupBox("执行参数");
    auto* execLayout = new QHBoxLayout(execGroup);
    m_repeatCountSpinBox = new QSpinBox();
    m_repeatCountSpinBox->setRange(1, 999999);
    m_infiniteCheckBox = new QCheckBox("无限循环");
    execLayout->addWidget(new QLabel("执行次数:"));
    execLayout->addWidget(m_repeatCountSpinBox, 1);
    execLayout->addWidget(m_infiniteCheckBox);
    auto* hotkeyGroup = new QGroupBox("快捷键");
    auto* hotkeyLayout = new QHBoxLayout(hotkeyGroup);
    m_startHotkeyEdit = new HotkeyEdit();
    hotkeyLayout->addWidget(new QLabel("启动/停止:"));
    hotkeyLayout->addWidget(m_startHotkeyEdit, 1);
    executionRow->addWidget(execGroup, 1);
    executionRow->addWidget(hotkeyGroup, 1);
    mainLayout->addLayout(executionRow);

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

    // 快捷键变更时立刻保存并检测冲突 (冲突时恢复原值)
    connect(m_startHotkeyEdit, &HotkeyEdit::hotkeyChanged, this, [this](const HotkeyInfo& hk) {
        HotkeyInfo oldHk = m_settings->keyboardStartHotkey();
        m_settings->setKeyboardStartHotkey(hk);
        QString conflict = m_settings->checkHotkeyConflicts();
        if (!conflict.isEmpty()) {
            // 恢复原来的快捷键
            m_settings->setKeyboardStartHotkey(oldHk);
            m_startHotkeyEdit->blockSignals(true);
            m_startHotkeyEdit->setHotkey(oldHk);
            m_startHotkeyEdit->blockSignals(false);
            QMessageBox::warning(this, "快捷键冲突",
                conflict + "\n\n已恢复为原来的快捷键。");
        }
    });
}

void KeyboardClickPage::loadSettings()
{
    m_loading = true;
    m_inputModeCombo->setCurrentIndex(m_inputModeCombo->findData(m_settings->keyboardInputMode()));
    m_intervalSpinBox->setValue(m_settings->keyboardInterval());
    m_pressDurationSpinBox->setValue(m_settings->keyboardPressDuration());
    m_startDelaySpinBox->setValue(m_settings->keyboardStartDelay());
    m_repeatCountSpinBox->setValue(m_settings->keyboardRepeatCount());
    m_infiniteCheckBox->setChecked(m_settings->keyboardInfinite());

    m_startHotkeyEdit->setHotkey(m_settings->keyboardStartHotkey());

    m_keyCaptureEdit->setKeyInfo(m_settings->keyboardKeyInfo());
    m_loading = false;
    onInputModeChanged(m_inputModeCombo->currentIndex());
}

void KeyboardClickPage::saveSettings()
{
    if (m_loading) return;
    m_settings->setKeyboardInputMode(m_inputModeCombo->currentData().toInt());
    m_settings->setKeyboardInterval(m_intervalSpinBox->value());
    m_settings->setKeyboardPressDuration(m_pressDurationSpinBox->value());
    m_settings->setKeyboardStartDelay(m_startDelaySpinBox->value());
    m_settings->setKeyboardRepeatCount(m_repeatCountSpinBox->value());
    m_settings->setKeyboardInfinite(m_infiniteCheckBox->isChecked());
    m_settings->setKeyboardKeyInfo(m_keyCaptureEdit->keyInfo());
    m_settings->setKeyboardKeyDisplay(m_keyCaptureEdit->keyInfo().displayName);
    m_settings->setKeyboardStartHotkey(m_startHotkeyEdit->hotkey());
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

    if (!validateAndUpdate()) return;
    emit startRequested();
}

void KeyboardClickPage::onStopClicked()
{
    emit stopRequested();
}

void KeyboardClickPage::onInputModeChanged(int index)
{
    Q_UNUSED(index);
    m_pressDurationSpinBox->setEnabled(m_inputModeCombo->isEnabled()
        && m_inputModeCombo->currentData().toInt() == static_cast<int>(KeyInputMode::Hold));
    m_validationLabel->clear();
}

void KeyboardClickPage::setRunningState(bool running)
{
    m_startHotkeyEdit->setEnabled(!running);
    m_startBtn->setEnabled(!running);
    m_stopBtn->setEnabled(running);

    m_keyCaptureEdit->setEnabled(!running);
    m_inputModeCombo->setEnabled(!running);
    m_startDelaySpinBox->setEnabled(!running);
    onInputModeChanged(m_inputModeCombo->currentIndex());
    m_intervalSpinBox->setEnabled(!running);
    m_repeatCountSpinBox->setEnabled(!running && !m_infiniteCheckBox->isChecked());
    m_infiniteCheckBox->setEnabled(!running);

    if (running) {
        m_statusLabel->setText("运行中...");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #FF9800; }");
    } else {
        m_statusLabel->setText("就绪");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #4CAF50; }");
    }
}

bool KeyboardClickPage::validateAndUpdate()
{
    QString error;
    int intervalMs = m_intervalSpinBox->value();

    if (!ValidationUtils::validateClickInterval(intervalMs, 1, &error)) {
        m_validationLabel->setText(error);
        return false;
    }

    if (!m_infiniteCheckBox->isChecked()
        && !ValidationUtils::validateRepeatCount(m_repeatCountSpinBox->value(), false, &error)) {
        m_validationLabel->setText(error);
        return false;
    }

    m_validationLabel->clear();
    return true;
}
