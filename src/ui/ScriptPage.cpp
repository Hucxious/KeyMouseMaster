#include "ScriptPage.h"
#include "ui/HotkeyEdit.h"
#include "models/ScriptEventTableModel.h"
#include "core/AppController.h"
#include "core/ScriptSerializer.h"
#include "core/ScriptPlayer.h"
#include "core/ScriptRecorder.h"
#include "settings/SettingsManager.h"
#include "utils/ValidationUtils.h"
#include "utils/TimeUtils.h"
#include "utils/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QScrollArea>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>

ScriptPage::ScriptPage(AppController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_settings(controller->settingsManager())
{
    setupUI();
    connectSignals();
    loadSettings();
    refreshRecentScripts();
}

void ScriptPage::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(3);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    // 状态栏
    auto* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #4CAF50; }");
    m_eventCountLabel = new QLabel("事件: 0");
    m_durationLabel = new QLabel("时长: 0ms");
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_eventCountLabel);
    statusLayout->addWidget(m_durationLabel);
    mainLayout->addLayout(statusLayout);

    // 主分栏：左(管理) | 右(表格)
    auto* splitter = new QSplitter(Qt::Horizontal);

    // ---- 左侧面板 ----
    auto* leftPanel = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 4, 0);
    leftLayout->setSpacing(3);

    // 脚本信息
    auto* infoGroup = new QGroupBox("脚本信息");
    auto* infoLayout = new QFormLayout(infoGroup);
    m_scriptNameEdit = new QLineEdit("新建脚本");
    infoLayout->addRow("名称:", m_scriptNameEdit);
    m_scriptDescEdit = new QTextEdit();
    m_scriptDescEdit->setMaximumHeight(45);
    m_scriptDescEdit->setPlaceholderText("脚本说明...");
    infoLayout->addRow("说明:", m_scriptDescEdit);
    leftLayout->addWidget(infoGroup);

    // 脚本操作
    auto* opsGroup = new QGroupBox("脚本操作");
    auto* opsLayout = new QGridLayout(opsGroup);
    m_newBtn = new QPushButton("新建");
    m_importBtn = new QPushButton("导入");
    m_saveBtn = new QPushButton("保存");
    m_saveAsBtn = new QPushButton("另存为");
    m_deleteBtn = new QPushButton("删除");
    opsLayout->addWidget(m_newBtn, 0, 0);
    opsLayout->addWidget(m_importBtn, 0, 1);
    opsLayout->addWidget(m_saveBtn, 1, 0);
    opsLayout->addWidget(m_saveAsBtn, 1, 1);
    opsLayout->addWidget(m_deleteBtn, 2, 0);
    leftLayout->addWidget(opsGroup);

    // 最近脚本
    auto* recentGroup = new QGroupBox("最近脚本");
    auto* recentLayout = new QVBoxLayout(recentGroup);
    m_recentScriptModel = new QStringListModel(this);
    m_recentScriptList = new QListView();
    m_recentScriptList->setModel(m_recentScriptModel);
    m_recentScriptList->setMaximumHeight(70);
    recentLayout->addWidget(m_recentScriptList);
    leftLayout->addWidget(recentGroup);

    // 录制设置
    auto* recordGroup = new QGroupBox("录制设置");
    auto* recordLayout = new QVBoxLayout(recordGroup);
    auto* checkGrid = new QGridLayout();
    m_recordMouseMoveCheck = new QCheckBox("录制鼠标移动");
    m_recordMouseMoveCheck->setChecked(true);
    m_recordMouseClickCheck = new QCheckBox("录制鼠标点击");
    m_recordMouseClickCheck->setChecked(true);
    m_recordWheelCheck = new QCheckBox("录制滚轮");
    m_recordWheelCheck->setChecked(true);
    m_recordKeyboardCheck = new QCheckBox("录制键盘");
    m_recordKeyboardCheck->setChecked(true);
    checkGrid->addWidget(m_recordMouseMoveCheck, 0, 0);
    checkGrid->addWidget(m_recordMouseClickCheck, 0, 1);
    checkGrid->addWidget(m_recordWheelCheck, 1, 0);
    checkGrid->addWidget(m_recordKeyboardCheck, 1, 1);
    recordLayout->addLayout(checkGrid);

    auto* moveOptsLayout = new QHBoxLayout();
    moveOptsLayout->addWidget(new QLabel("移动最小间隔:"));
    m_moveMinIntervalSpinBox = new QSpinBox();
    m_moveMinIntervalSpinBox->setRange(1, 1000);
    m_moveMinIntervalSpinBox->setValue(10);
    m_moveMinIntervalSpinBox->setSuffix(" ms");
    moveOptsLayout->addWidget(m_moveMinIntervalSpinBox);
    recordLayout->addLayout(moveOptsLayout);

    auto* distOptsLayout = new QHBoxLayout();
    distOptsLayout->addWidget(new QLabel("移动最小距离:"));
    m_moveMinDistanceSpinBox = new QSpinBox();
    m_moveMinDistanceSpinBox->setRange(1, 500);
    m_moveMinDistanceSpinBox->setValue(3);
    m_moveMinDistanceSpinBox->setSuffix(" px");
    distOptsLayout->addWidget(m_moveMinDistanceSpinBox);
    recordLayout->addLayout(distOptsLayout);

    m_recordStartHotkeyEdit = new HotkeyEdit();
    m_recordStopHotkeyEdit = new HotkeyEdit();
    auto* recHotkeyLayout = new QFormLayout();
    recHotkeyLayout->addRow("录制开始:", m_recordStartHotkeyEdit);
    recHotkeyLayout->addRow("录制停止:", m_recordStopHotkeyEdit);
    recordLayout->addLayout(recHotkeyLayout);

    auto* recBtnLayout = new QHBoxLayout();
    m_recordStartBtn = new QPushButton("⏺ 开始录制");
    m_recordStartBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; } QPushButton:disabled { background-color: #cccccc; }");
    m_recordStopBtn = new QPushButton("⏹ 停止录制");
    m_recordStopBtn->setEnabled(false);
    m_recordStopBtn->setStyleSheet("QPushButton { background-color: #757575; color: white; font-weight: bold; } QPushButton:disabled { background-color: #cccccc; }");
    recBtnLayout->addWidget(m_recordStartBtn);
    recBtnLayout->addWidget(m_recordStopBtn);
    recordLayout->addLayout(recBtnLayout);

    leftLayout->addWidget(recordGroup);

    // 回放设置
    auto* playbackGroup = new QGroupBox("回放设置");
    auto* playbackLayout = new QFormLayout(playbackGroup);

    m_speedCombo = new QComboBox();
    m_speedCombo->setMinimumWidth(130);
    m_speedCombo->addItem("0.5x", 0.5);
    m_speedCombo->addItem("0.75x", 0.75);
    m_speedCombo->addItem("1.0x (原速)", 1.0);
    m_speedCombo->addItem("1.5x", 1.5);
    m_speedCombo->addItem("2.0x", 2.0);
    playbackLayout->addRow("播放速度:", m_speedCombo);

    m_playbackStartDelaySpinBox = new QSpinBox();
    m_playbackStartDelaySpinBox->setRange(0, 60000);
    m_playbackStartDelaySpinBox->setSuffix(" ms");
    playbackLayout->addRow("启动延迟:", m_playbackStartDelaySpinBox);

    auto* repeatLayout = new QHBoxLayout();
    m_repeatCountSpinBox = new QSpinBox();
    m_repeatCountSpinBox->setRange(1, 999999);
    m_repeatCountSpinBox->setValue(1);
    repeatLayout->addWidget(m_repeatCountSpinBox);
    m_infiniteRepeatCheckBox = new QCheckBox("无限");
    repeatLayout->addWidget(m_infiniteRepeatCheckBox);
    repeatLayout->addStretch();
    playbackLayout->addRow("重复次数:", repeatLayout);

    m_roundIntervalSpinBox = new QSpinBox();
    m_roundIntervalSpinBox->setRange(0, 3600000);
    m_roundIntervalSpinBox->setValue(1000);
    m_roundIntervalSpinBox->setSuffix(" ms");
    playbackLayout->addRow("每轮间隔:", m_roundIntervalSpinBox);

    m_restoreCursorCheckBox = new QCheckBox("恢复回放前鼠标位置");
    m_restoreCursorCheckBox->setChecked(true);
    playbackLayout->addRow("", m_restoreCursorCheckBox);

    m_skipDisabledCheckBox = new QCheckBox("跳过禁用事件");
    m_skipDisabledCheckBox->setChecked(true);
    playbackLayout->addRow("", m_skipDisabledCheckBox);

    m_coordModeCombo = new QComboBox();
    m_coordModeCombo->setMinimumWidth(170);
    m_coordModeCombo->addItem("显示器相对坐标", static_cast<int>(CoordinateMode::MonitorRelative));
    m_coordModeCombo->addItem("虚拟桌面绝对坐标", static_cast<int>(CoordinateMode::VirtualDesktopAbsolute));
    m_coordModeCombo->addItem("显示器比例坐标", static_cast<int>(CoordinateMode::MonitorRatio));
    playbackLayout->addRow("坐标模式:", m_coordModeCombo);

    m_playbackStartHotkeyEdit = new HotkeyEdit();
    m_playbackStopHotkeyEdit = new HotkeyEdit();
    playbackLayout->addRow("回放开始:", m_playbackStartHotkeyEdit);
    playbackLayout->addRow("回放停止:", m_playbackStopHotkeyEdit);

    auto* pbBtnLayout = new QHBoxLayout();
    m_playbackStartBtn = new QPushButton("▶ 开始回放");
    m_playbackStartBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; } QPushButton:disabled { background-color: #cccccc; }");
    m_playbackStopBtn = new QPushButton("⏹ 停止回放");
    m_playbackStopBtn->setEnabled(false);
    m_playbackStopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; } QPushButton:disabled { background-color: #cccccc; }");
    pbBtnLayout->addWidget(m_playbackStartBtn);
    pbBtnLayout->addWidget(m_playbackStopBtn);
    playbackLayout->addRow(pbBtnLayout);

    leftLayout->addWidget(playbackGroup);

    // 左侧面板放入滚动区域，窗口缩小时可滚动查看
    auto* leftScrollArea = new QScrollArea();
    leftScrollArea->setWidget(leftPanel);
    leftScrollArea->setWidgetResizable(true);
    leftScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftScrollArea->setFrameShape(QFrame::NoFrame);

    // ---- 右侧面板：事件表格 ----
    auto* rightPanel = new QWidget();
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(2, 0, 0, 0);
    rightLayout->setSpacing(3);

    // 表格工具栏
    auto* tableToolbar = new QHBoxLayout();
    m_deleteEventBtn = new QPushButton("删除选中");
    m_toggleEventBtn = new QPushButton("启用/禁用");
    m_moveUpBtn = new QPushButton("上移");
    m_moveDownBtn = new QPushButton("下移");
    m_insertWaitBtn = new QPushButton("插入等待");
    m_clearAllBtn = new QPushButton("清空全部");
    m_clearAllBtn->setStyleSheet("QPushButton { color: #f44336; }");
    tableToolbar->addWidget(m_deleteEventBtn);
    tableToolbar->addWidget(m_toggleEventBtn);
    tableToolbar->addWidget(m_moveUpBtn);
    tableToolbar->addWidget(m_moveDownBtn);
    tableToolbar->addWidget(m_insertWaitBtn);
    tableToolbar->addWidget(m_clearAllBtn);
    tableToolbar->addStretch();
    rightLayout->addLayout(tableToolbar);

    // 事件表格
    m_eventModel = new ScriptEventTableModel(this);
    m_eventModel->setDocument(&m_document);

    m_eventTableView = new QTableView();
    m_eventTableView->setModel(m_eventModel);
    m_eventTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_eventTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_eventTableView->setAlternatingRowColors(true);
    m_eventTableView->horizontalHeader()->setStretchLastSection(true);
    m_eventTableView->verticalHeader()->setVisible(false);
    m_eventTableView->setSortingEnabled(false);

    // 设置列宽
    auto* header = m_eventTableView->horizontalHeader();
    header->setSectionResizeMode(ScriptEventTableModel::ColIndex, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ScriptEventTableModel::ColTime, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ScriptEventTableModel::ColType, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ScriptEventTableModel::ColButton, QHeaderView::Stretch);
    header->setSectionResizeMode(ScriptEventTableModel::ColMonitor, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ScriptEventTableModel::ColDesktopPos, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ScriptEventTableModel::ColRelativePos, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ScriptEventTableModel::ColParams, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ScriptEventTableModel::ColEnabled, QHeaderView::ResizeToContents);

    rightLayout->addWidget(m_eventTableView, 1);

    // 添加到分栏器
    splitter->addWidget(leftScrollArea);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);

    mainLayout->addWidget(splitter, 1);
}

void ScriptPage::connectSignals()
{
    // 脚本管理
    connect(m_newBtn, &QPushButton::clicked, this, &ScriptPage::onNewScript);
    connect(m_importBtn, &QPushButton::clicked, this, &ScriptPage::onImportScript);
    connect(m_saveBtn, &QPushButton::clicked, this, &ScriptPage::onSaveScript);
    connect(m_saveAsBtn, &QPushButton::clicked, this, &ScriptPage::onSaveAsScript);
    connect(m_deleteBtn, &QPushButton::clicked, this, &ScriptPage::onDeleteScript);

    // 录制
    connect(m_recordStartBtn, &QPushButton::clicked, this, &ScriptPage::onStartRecording);
    connect(m_recordStopBtn, &QPushButton::clicked, this, &ScriptPage::onStopRecording);

    // 回放
    connect(m_playbackStartBtn, &QPushButton::clicked, this, &ScriptPage::onStartPlayback);
    connect(m_playbackStopBtn, &QPushButton::clicked, this, &ScriptPage::onStopPlayback);

    // 表格编辑
    connect(m_deleteEventBtn, &QPushButton::clicked, this, &ScriptPage::onDeleteSelectedEvents);
    connect(m_toggleEventBtn, &QPushButton::clicked, this, &ScriptPage::onToggleSelectedEvents);
    connect(m_moveUpBtn, &QPushButton::clicked, this, &ScriptPage::onMoveEventUp);
    connect(m_moveDownBtn, &QPushButton::clicked, this, &ScriptPage::onMoveEventDown);
    connect(m_clearAllBtn, &QPushButton::clicked, this, &ScriptPage::onClearAllEvents);
    connect(m_insertWaitBtn, &QPushButton::clicked, this, &ScriptPage::onInsertWaitEvent);

    // 最近脚本
    connect(m_recentScriptList, &QListView::clicked,
            this, &ScriptPage::onRecentScriptSelected);

    // 文档修改
    connect(m_eventModel, &ScriptEventTableModel::documentModified,
            this, &ScriptPage::updateEventCount);

    // 无限重复
    connect(m_infiniteRepeatCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_repeatCountSpinBox->setEnabled(!checked);
    });
}

// ============================================================================
// 脚本管理操作
// ============================================================================
void ScriptPage::onNewScript()
{
    if (m_modified) {
        auto ret = QMessageBox::question(this, "新建脚本",
            "当前脚本未保存，是否继续？", QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }
    m_document.clear();
    m_scriptNameEdit->setText("新建脚本");
    m_scriptDescEdit->clear();
    m_currentFilePath.clear();
    m_modified = false;
    m_eventModel->refreshAll();
    updateEventCount();
    emit statusMessage("已创建新脚本");
}

void ScriptPage::onImportScript()
{
    QString defaultDir = m_settings->scriptDefaultDir();
    QString filePath = QFileDialog::getOpenFileName(this, "导入脚本", defaultDir,
        ScriptSerializer::fileFilter());
    if (filePath.isEmpty()) return;

    QString error;
    if (!m_controller->scriptSerializer()->load(filePath, m_document, &error)) {
        QMessageBox::critical(this, "导入失败", error);
        return;
    }

    m_currentFilePath = filePath;
    m_scriptNameEdit->setText(m_document.name);
    m_scriptDescEdit->setText(m_document.description);
    m_modified = false;
    m_eventModel->refreshAll();
    updateEventCount();
    m_settings->addRecentScript(filePath);
    refreshRecentScripts();
    emit statusMessage(QString("已导入: %1").arg(filePath));
}

void ScriptPage::onSaveScript()
{
    if (m_currentFilePath.isEmpty()) {
        onSaveAsScript();
        return;
    }

    // 更新文档元数据
    m_document.name = m_scriptNameEdit->text();
    m_document.description = m_scriptDescEdit->toPlainText();

    QString error;
    if (!m_controller->scriptSerializer()->save(m_document, m_currentFilePath, &error)) {
        QMessageBox::critical(this, "保存失败", error);
        return;
    }

    m_modified = false;
    m_settings->addRecentScript(m_currentFilePath);
    refreshRecentScripts();
    emit statusMessage("脚本已保存");
}

void ScriptPage::onSaveAsScript()
{
    QString defaultDir = m_settings->scriptDefaultDir();
    QString defaultName = m_scriptNameEdit->text();
    if (!defaultName.endsWith(".kms")) defaultName += ".kms";
    QString filePath = QFileDialog::getSaveFileName(this, "另存为",
        defaultDir + "/" + defaultName, ScriptSerializer::fileFilter());
    if (filePath.isEmpty()) return;

    m_document.name = m_scriptNameEdit->text();
    m_document.description = m_scriptDescEdit->toPlainText();

    QString error;
    if (!m_controller->scriptSerializer()->save(m_document, filePath, &error)) {
        QMessageBox::critical(this, "保存失败", error);
        return;
    }

    m_currentFilePath = filePath;
    m_modified = false;
    m_settings->addRecentScript(filePath);
    m_settings->setScriptDefaultDir(QFileInfo(filePath).absolutePath());
    refreshRecentScripts();
    emit statusMessage(QString("已保存: %1").arg(filePath));
}

void ScriptPage::onDeleteScript()
{
    if (m_document.isEmpty()) return;

    auto ret = QMessageBox::question(this, "删除脚本",
        "确定要清空当前脚本吗？此操作不可撤销。",
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    m_document.clear();
    m_currentFilePath.clear();
    m_modified = false;
    m_eventModel->refreshAll();
    updateEventCount();
    emit statusMessage("脚本已清空");
}

// ============================================================================
// 录制操作
// ============================================================================
void ScriptPage::onStartRecording()
{
    emit startRecordingRequested();
}

void ScriptPage::onStopRecording()
{
    emit stopRecordingRequested();
}

// ============================================================================
// 回放操作
// ============================================================================
void ScriptPage::onStartPlayback()
{
    if (m_document.isEmpty()) {
        emit statusMessage("脚本事件为空，无法回放");
        return;
    }
    emit startPlaybackRequested();
}

void ScriptPage::onStopPlayback()
{
    emit stopPlaybackRequested();
}

// ============================================================================
// 表格编辑
// ============================================================================
void ScriptPage::onDeleteSelectedEvents()
{
    QModelIndexList selected = m_eventTableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;

    QList<int> rows;
    for (const auto& idx : selected) rows.append(idx.row());
    m_eventModel->removeRows(rows);
    updateEventCount();
}

void ScriptPage::onToggleSelectedEvents()
{
    QModelIndexList selected = m_eventTableView->selectionModel()->selectedRows();
    for (const auto& idx : selected)
        m_eventModel->toggleEnabled(idx.row());
}

void ScriptPage::onMoveEventUp()
{
    QModelIndexList selected = m_eventTableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;
    m_eventModel->moveRowUp(selected.first().row());
}

void ScriptPage::onMoveEventDown()
{
    QModelIndexList selected = m_eventTableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;
    m_eventModel->moveRowDown(selected.first().row());
}

void ScriptPage::onClearAllEvents()
{
    if (m_document.isEmpty()) return;

    auto ret = QMessageBox::question(this, "清空事件",
        "确定要清空所有事件吗？", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    m_document.clear();
    m_eventModel->refreshAll();
    updateEventCount();
}

void ScriptPage::onInsertWaitEvent()
{
    // 插入一个等待事件（当前实现为空的鼠标移动事件，仅用于延时）
    QModelIndex current = m_eventTableView->currentIndex();
    int row = current.isValid() ? current.row() + 1 : m_document.eventCount();

    ScriptEvent ev;
    ev.type = ScriptEventType::MouseMove;
    ev.timestampMs = row > 0 ? m_document.events[row - 1].timestampMs + 1000 : 1000;
    ev.enabled = true;

    m_eventModel->insertEvent(row, ev);
    updateEventCount();
}

void ScriptPage::onRecentScriptSelected(const QModelIndex& index)
{
    QString path = m_recentScriptModel->data(index, Qt::DisplayRole).toString();
    if (path.isEmpty()) return;

    QString error;
    if (!m_controller->scriptSerializer()->load(path, m_document, &error)) {
        emit statusMessage("加载失败: " + error);
        return;
    }

    m_currentFilePath = path;
    m_scriptNameEdit->setText(m_document.name);
    m_scriptDescEdit->setText(m_document.description);
    m_modified = false;
    m_eventModel->refreshAll();
    updateEventCount();
    emit statusMessage(QString("已加载: %1").arg(path));
}

void ScriptPage::updateEventCount()
{
    int count = m_document.eventCount();
    m_eventCountLabel->setText(QString("事件: %1").arg(count));

    // 计算总时长
    if (count > 0) {
        int64_t duration = m_document.events.last().timestampMs;
        m_durationLabel->setText(QString("时长: %1").arg(TimeUtils::formatDurationMs(duration)));
    } else {
        m_durationLabel->setText("时长: 0ms");
    }
}

// ============================================================================
// 设置和状态
// ============================================================================
void ScriptPage::loadSettings()
{
    m_playbackStartDelaySpinBox->setValue(m_settings->scriptPlaybackSpeed() > 0 ? 0 : 0);
    m_repeatCountSpinBox->setValue(m_settings->scriptRepeatCount());

    double speed = m_settings->scriptPlaybackSpeed();
    for (int i = 0; i < m_speedCombo->count(); ++i) {
        if (qAbs(m_speedCombo->itemData(i).toDouble() - speed) < 0.01) {
            m_speedCombo->setCurrentIndex(i);
            break;
        }
    }

    refreshRecentScripts();
}

void ScriptPage::saveSettings()
{
    double speed = m_speedCombo->currentData().toDouble();
    m_settings->setScriptPlaybackSpeed(speed);
    m_settings->setScriptRepeatCount(m_repeatCountSpinBox->value());
    m_settings->sync();
}

void ScriptPage::setRunningState(bool running)
{
    m_playbackStartBtn->setEnabled(!running);
    m_playbackStopBtn->setEnabled(running);
    m_recordStartBtn->setEnabled(!running);
    // 录制停止按钮在录制期间始终可用

    // 运行期间禁用编辑
    enableEditingControls(!running);

    if (running) {
        m_statusLabel->setText("运行中...");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #FF9800; }");
    } else {
        m_statusLabel->setText("就绪");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #4CAF50; }");
    }
}

void ScriptPage::enableEditingControls(bool enable)
{
    m_deleteEventBtn->setEnabled(enable);
    m_toggleEventBtn->setEnabled(enable);
    m_moveUpBtn->setEnabled(enable);
    m_moveDownBtn->setEnabled(enable);
    m_insertWaitBtn->setEnabled(enable);
    m_clearAllBtn->setEnabled(enable);
    m_deleteBtn->setEnabled(enable);
}

void ScriptPage::refreshRecentScripts()
{
    QStringList recent = m_settings->recentScripts();
    m_recentScriptModel->setStringList(recent);
}
