#ifndef KEYBOARDCLICKPAGE_H
#define KEYBOARDCLICKPAGE_H

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include "AppTypes.h"

class KeyCaptureEdit;
class HotkeyEdit;
class AppController;
class SettingsManager;

// ============================================================================
// 键盘连点分页
// ============================================================================
class KeyboardClickPage : public QWidget
{
    Q_OBJECT

public:
    explicit KeyboardClickPage(AppController* controller, QWidget* parent = nullptr);

    void loadSettings();
    void saveSettings();
    void setRunningState(bool running);

signals:
    void startRequested();
    void stopRequested();
    void statusMessage(const QString& message);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onInputModeChanged(int index);

private:
    void setupUI();
    void connectSignals();
    void validateAndUpdate();

    AppController* m_controller;
    SettingsManager* m_settings;

    // 按键捕获
    KeyCaptureEdit* m_keyCaptureEdit;
    QLabel* m_keyDetailLabel;

    // 输入模式
    QComboBox* m_inputModeCombo;

    // 时间参数
    QSpinBox* m_intervalSpinBox;
    QSpinBox* m_pressDurationSpinBox;
    QSpinBox* m_startDelaySpinBox;

    // 执行参数
    QSpinBox* m_repeatCountSpinBox;
    QCheckBox* m_infiniteCheckBox;

    // 快捷键
    HotkeyEdit* m_startHotkeyEdit;
    HotkeyEdit* m_stopHotkeyEdit;

    // 按钮
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;

    // 状态
    QLabel* m_statusLabel;
    QLabel* m_validationLabel;
};

#endif // KEYBOARDCLICKPAGE_H
