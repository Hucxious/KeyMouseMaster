#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTabWidget>
#include <QTimer>
#include <QLabel>

#include "AppTypes.h"

class AppController;
class MouseClickPage;
class KeyboardClickPage;
class ScriptPage;
class SettingsManager;

// ============================================================================
// 主窗口
// ============================================================================
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(AppController* controller, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onTaskStateChanged(TaskState state);
    void onEmergencyStop();
    void onResetInputState();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTabChanged(int index);
    void onMousePageStart();
    void onMousePageStop();
    void onKeyboardPageStart();
    void onKeyboardPageStop();
    void onScriptRecordStart();
    void onScriptRecordStop();
    void onScriptPlaybackStart();
    void onScriptPlaybackStop();
    void updateStatusBar();

private:
    void setupUI();
    void setupMenuBar();
    void setupSystemTray();
    void setupStatusBar();
    void connectSignals();
    void loadSettings();
    void saveSettings();
    void applyAppStyle();
    void updateTrayIcon(TaskState state);

    AppController* m_controller;
    SettingsManager* m_settings;

    // 分页
    QTabWidget* m_tabWidget;
    MouseClickPage* m_mousePage;
    KeyboardClickPage* m_keyboardPage;
    ScriptPage* m_scriptPage;

    // 菜单
    QMenu* m_fileMenu;
    QMenu* m_viewMenu;
    QMenu* m_helpMenu;

    // 系统托盘
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayMenu;
    QAction* m_trayShowAction;
    QAction* m_trayStartAction;
    QAction* m_trayStopAction;
    QAction* m_trayResetAction;
    QAction* m_trayExitAction;

    // 状态栏
    QLabel* m_stateLabel;
    QLabel* m_cursorLabel;
    QLabel* m_monitorLabel;
    QTimer* m_statusUpdateTimer;

    // 状态
    bool m_isTaskRunning = false;
};

#endif // MAINWINDOW_H
