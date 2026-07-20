#ifndef SCRIPTPAGE_H
#define SCRIPTPAGE_H

#include <QWidget>
#include <QTableView>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QListView>
#include <QStringListModel>
#include "AppTypes.h"
#include "ScriptDocument.h"

class HotkeyEdit;
class AppController;
class SettingsManager;
class ScriptEventTableModel;

// ============================================================================
// 脚本录制/回放分页
// ============================================================================
class ScriptPage : public QWidget
{
    Q_OBJECT

public:
    explicit ScriptPage(AppController* controller, QWidget* parent = nullptr);

    void loadSettings();
    void saveSettings();
    void setRunningState(bool running);

signals:
    void startRecordingRequested();
    void stopRecordingRequested();
    void startPlaybackRequested();
    void stopPlaybackRequested();
    void statusMessage(const QString& message);

private slots:
    void onNewScript();
    void onImportScript();
    void onSaveScript();
    void onSaveAsScript();
    void onDeleteScript();
    void onStartRecording();
    void onStopRecording();
    void onStartPlayback();
    void onStopPlayback();
    void onDeleteSelectedEvents();
    void onToggleSelectedEvents();
    void onMoveEventUp();
    void onMoveEventDown();
    void onClearAllEvents();
    void onInsertWaitEvent();
    void onRecentScriptSelected(const QModelIndex& index);
    void updateEventCount();

private:
    void setupUI();
    void connectSignals();
    void enableEditingControls(bool enable);
    void refreshRecentScripts();

    AppController* m_controller;
    SettingsManager* m_settings;
    ScriptEventTableModel* m_eventModel;

    // 左侧：脚本管理
    QLineEdit* m_scriptNameEdit;
    QTextEdit* m_scriptDescEdit;
    QListView* m_recentScriptList;
    QStringListModel* m_recentScriptModel;
    QPushButton* m_newBtn;
    QPushButton* m_importBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_saveAsBtn;
    QPushButton* m_deleteBtn;

    // 录制参数
    QCheckBox* m_recordMouseMoveCheck;
    QCheckBox* m_recordMouseClickCheck;
    QCheckBox* m_recordWheelCheck;
    QCheckBox* m_recordKeyboardCheck;
    QSpinBox* m_moveMinIntervalSpinBox;
    QSpinBox* m_moveMinDistanceSpinBox;
    HotkeyEdit* m_recordStartHotkeyEdit;
    HotkeyEdit* m_recordStopHotkeyEdit;
    QPushButton* m_recordStartBtn;
    QPushButton* m_recordStopBtn;

    // 回放参数
    QComboBox* m_speedCombo;
    QSpinBox* m_playbackStartDelaySpinBox;
    QSpinBox* m_repeatCountSpinBox;
    QCheckBox* m_infiniteRepeatCheckBox;
    QSpinBox* m_roundIntervalSpinBox;
    QCheckBox* m_restoreCursorCheckBox;
    QCheckBox* m_skipDisabledCheckBox;
    QComboBox* m_coordModeCombo;
    HotkeyEdit* m_playbackStartHotkeyEdit;
    HotkeyEdit* m_playbackStopHotkeyEdit;
    QPushButton* m_playbackStartBtn;
    QPushButton* m_playbackStopBtn;

    // 右侧：事件表格
    QTableView* m_eventTableView;
    QPushButton* m_deleteEventBtn;
    QPushButton* m_toggleEventBtn;
    QPushButton* m_moveUpBtn;
    QPushButton* m_moveDownBtn;
    QPushButton* m_insertWaitBtn;
    QPushButton* m_clearAllBtn;

    // 状态
    QLabel* m_statusLabel;
    QLabel* m_eventCountLabel;
    QLabel* m_durationLabel;

    // 当前脚本
    ScriptDocument m_document;
    QString m_currentFilePath;
    bool m_modified = false;
};

#endif // SCRIPTPAGE_H
