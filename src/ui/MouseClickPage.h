#ifndef MOUSECLICKPAGE_H
#define MOUSECLICKPAGE_H

#include <QWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include <QRadioButton>
#include "AppTypes.h"
#include "MonitorInfo.h"

class HotkeyEdit;
class MonitorPreviewWidget;
class AppController;
class SettingsManager;

// ============================================================================
// 鼠标连点分页
// ============================================================================
class MouseClickPage : public QWidget
{
    Q_OBJECT

public:
    explicit MouseClickPage(AppController* controller, QWidget* parent = nullptr);

    void loadSettings();
    void saveSettings();
    void setRunningState(bool running);
    void updateCursorInfo();

signals:
    void startRequested();
    void stopRequested();
    void statusMessage(const QString& message);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onGetCursorPos();
    void onCoordinateModeChanged(int index);
    void onMonitorSelectionChanged(int index);
    void onButtonChanged(int index);
    void onModeChanged(int index);
    void onIntervalUnitChanged(int index);

private:
    void setupUI();
    void connectSignals();
    void updateMonitorList();
    void validateAndUpdate();

    AppController* m_controller;
    SettingsManager* m_settings;

    // 按键选择
    QComboBox* m_buttonCombo;
    QComboBox* m_modeCombo;

    // 坐标模式
    QComboBox* m_coordModeCombo;
    QWidget* m_fixedPosGroup;
    QLineEdit* m_fixedXEdit;
    QLineEdit* m_fixedYEdit;
    QPushButton* m_getPosBtn;

    // 显示器选择
    QWidget* m_monitorSelectGroup;
    QComboBox* m_monitorCombo;
    QLineEdit* m_monitorXEdit;
    QLineEdit* m_monitorYEdit;
    MonitorPreviewWidget* m_monitorPreview;

    // 时间参数
    QSpinBox* m_intervalSpinBox;
    QComboBox* m_intervalUnitCombo;
    QSpinBox* m_pressDurationSpinBox;
    QSpinBox* m_doubleClickIntervalSpinBox;
    QSpinBox* m_startDelaySpinBox;

    // 执行参数
    QSpinBox* m_repeatCountSpinBox;
    QCheckBox* m_infiniteCheckBox;
    QCheckBox* m_restoreCursorCheckBox;

    // 快捷键
    HotkeyEdit* m_startHotkeyEdit;
    HotkeyEdit* m_stopHotkeyEdit;

    // 按钮
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;

    // 状态
    QLabel* m_statusLabel;
    QLabel* m_cursorPosLabel;
    QLabel* m_currentMonitorLabel;
};

#endif // MOUSECLICKPAGE_H
