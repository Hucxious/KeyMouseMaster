#include "ScriptPage.h"
#include "ui/HotkeyEdit.h"
#include "models/ScriptEventTableModel.h"
#include "core/AppController.h"
#include "core/ScriptSerializer.h"
#include "core/ScriptPlayer.h"
#include "core/ScriptRecorder.h"
#include "core/TaskManager.h"
#include "settings/SettingsManager.h"
#include "utils/ValidationUtils.h"
#include "utils/TimeUtils.h"
#include "utils/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QResizeEvent>
#include <QScrollArea>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>

namespace {
// QScrollArea 默认会缓存子控件尺寸；配置重排后应使用当前布局提示。
class ConfigurationScrollArea final : public QScrollArea {
public:
    QSize sizeHint() const override {
        return widget() ? widget()->sizeHint() + QSize(2 * frameWidth(), 2 * frameWidth())
                        : QScrollArea::sizeHint();
    }
};
}

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

    m_configurationPanel = new QWidget();
    m_configurationLayout = new QGridLayout(m_configurationPanel);
    m_configurationLayout->setContentsMargins(0, 0, 0, 0);
    m_configurationLayout->setSpacing(6);

    // 脚本信息
    auto* infoGroup = new QGroupBox("脚本信息");
    auto* infoLayout = new QFormLayout(infoGroup);
    m_scriptNameEdit = new QLineEdit("新建脚本");
    infoLayout->addRow("名称:", m_scriptNameEdit);
    m_scriptDescEdit = new QTextEdit();
    m_scriptDescEdit->setMaximumHeight(fontMetrics().lineSpacing() * 3);
    m_scriptDescEdit->setPlaceholderText("脚本说明...");
    infoLayout->addRow("说明:", m_scriptDescEdit);

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
    opsLayout->addWidget(m_saveBtn, 0, 2);
    opsLayout->addWidget(m_saveAsBtn, 0, 3);
    opsLayout->addWidget(m_deleteBtn, 0, 4);

    // 最近脚本
    auto* recentGroup = new QGroupBox("最近脚本");
    auto* recentLayout = new QVBoxLayout(recentGroup);
    m_recentScriptModel = new QStringListModel(this);
    m_recentScriptList = new QListView();
    m_recentScriptList->setModel(m_recentScriptModel);
    m_recentScriptList->setMaximumHeight(fontMetrics().lineSpacing() * 4);
    m_recentScriptList->setMinimumWidth(0);
    recentLayout->addWidget(m_recentScriptList);

    // 录制设置
    auto* recordGroup = m_recordGroup = new QGroupBox("录制设置");
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

    m_recordHotkeyEdit = new HotkeyEdit();
    auto* recHotkeyLayout = new QFormLayout();
    recHotkeyLayout->addRow("录制 开始/停止:", m_recordHotkeyEdit);
    recordLayout->addLayout(recHotkeyLayout);

    auto* recBtnLayout = new QHBoxLayout();
    m_recordStartBtn = new QPushButton("⏺ 开始录制");
    m_recordStartBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; } QPushButton:disabled { background-color: #cccccc; }");
    m_recordStopBtn = new QPushButton("⏹ 停止录制");
    m_recordStopBtn->setEnabled(false);
    m_recordStopBtn->setStyleSheet("QPushButton { background-color: #757575; color: white; font-weight: bold; } QPushButton:disabled { background-color: #cccccc; }");
    recBtnLayout->addWidget(m_recordStartBtn);
    recBtnLayout->addWidget(m_recordStopBtn);

    // 回放设置
    auto* playbackGroup = m_playbackGroup = new QGroupBox("回放设置");
    auto* playbackLayout = new QGridLayout(playbackGroup);
    auto addPlaybackField = [playbackLayout](const QString& label, QWidget* field, int row, int col) {
        auto* fieldLayout = new QVBoxLayout();
        fieldLayout->setSpacing(2);
        fieldLayout->addWidget(new QLabel(label));
        field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        fieldLayout->addWidget(field);
        playbackLayout->addLayout(fieldLayout, row, col);
    };

    m_speedCombo = new QComboBox();
    m_speedCombo->addItem("0.5x", 0.5);
    m_speedCombo->addItem("0.75x", 0.75);
    m_speedCombo->addItem("1.0x (原速)", 1.0);
    m_speedCombo->addItem("1.5x", 1.5);
    m_speedCombo->addItem("2.0x", 2.0);
    addPlaybackField("播放速度:", m_speedCombo, 0, 0);

    m_playbackStartDelaySpinBox = new QSpinBox();
    m_playbackStartDelaySpinBox->setRange(0, 60000);
    m_playbackStartDelaySpinBox->setSuffix(" ms");
    addPlaybackField("启动延迟:", m_playbackStartDelaySpinBox, 0, 1);

    auto* repeatWidget = new QWidget();
    auto* repeatLayout = new QHBoxLayout(repeatWidget);
    repeatLayout->setContentsMargins(0, 0, 0, 0);
    m_repeatCountSpinBox = new QSpinBox();
    m_repeatCountSpinBox->setRange(1, 999999);
    m_repeatCountSpinBox->setValue(1);
    repeatLayout->addWidget(m_repeatCountSpinBox);
    m_infiniteRepeatCheckBox = new QCheckBox("无限");
    repeatLayout->addWidget(m_infiniteRepeatCheckBox);
    addPlaybackField("重复次数:", repeatWidget, 1, 0);

    m_roundIntervalSpinBox = new QSpinBox();
    m_roundIntervalSpinBox->setRange(0, 3600000);
    m_roundIntervalSpinBox->setValue(1000);
    m_roundIntervalSpinBox->setSuffix(" ms");
    addPlaybackField("每轮间隔:", m_roundIntervalSpinBox, 1, 1);

    m_restoreCursorCheckBox = new QCheckBox("恢复回放前鼠标位置");
    m_restoreCursorCheckBox->setChecked(true);
    playbackLayout->addWidget(m_restoreCursorCheckBox, 2, 0);

    m_skipDisabledCheckBox = new QCheckBox("跳过禁用事件");
    m_skipDisabledCheckBox->setChecked(true);
    playbackLayout->addWidget(m_skipDisabledCheckBox, 2, 1);

    m_coordModeCombo = new QComboBox();
    m_coordModeCombo->addItem("显示器相对坐标", static_cast<int>(CoordinateMode::MonitorRelative));
    m_coordModeCombo->addItem("虚拟桌面绝对坐标", static_cast<int>(CoordinateMode::VirtualDesktopAbsolute));
    m_coordModeCombo->addItem("显示器比例坐标", static_cast<int>(CoordinateMode::MonitorRatio));
    addPlaybackField("坐标模式:", m_coordModeCombo, 3, 0);

    m_playbackHotkeyEdit = new HotkeyEdit();
    addPlaybackField("回放 开始/停止:", m_playbackHotkeyEdit, 3, 1);

    auto* pbBtnLayout = new QHBoxLayout();
    m_playbackStartBtn = new QPushButton("▶ 开始回放");
    m_playbackStartBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; } QPushButton:disabled { background-color: #cccccc; }");
    m_playbackStopBtn = new QPushButton("⏹ 停止回放");
    m_playbackStopBtn->setEnabled(false);
    m_playbackStopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; } QPushButton:disabled { background-color: #cccccc; }");
    pbBtnLayout->addWidget(m_playbackStartBtn);
    pbBtnLayout->addWidget(m_playbackStopBtn);

    m_configurationLayout->addWidget(recordGroup, 0, 0);
    m_configurationLayout->addWidget(playbackGroup, 0, 1);
    m_configurationLayout->setColumnStretch(0, 2);
    m_configurationLayout->setColumnStretch(1, 3);
    m_configurationScroll = new ConfigurationScrollArea();
    m_configurationScroll->setObjectName("scriptConfiguration");
    m_configurationScroll->setFrameShape(QFrame::NoFrame);
    m_configurationScroll->setWidget(m_configurationPanel);
    m_configurationScroll->setWidgetResizable(true);
    m_configurationScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    m_configurationScroll->setMinimumHeight(fontMetrics().lineSpacing() * 5);
    mainLayout->addWidget(m_configurationScroll);

    // 运行按钮在滚动区外，小窗口也无需滚动寻找停止操作。
    auto* controlGroup = new QGroupBox("运行控制");
    controlGroup->setObjectName("scriptRunControls");
    auto* controlLayout = new QHBoxLayout(controlGroup);
    controlLayout->addLayout(recBtnLayout, 1);
    controlLayout->addLayout(pbBtnLayout, 1);
    mainLayout->addWidget(controlGroup);
    mainLayout->addWidget(opsGroup);

    auto* scriptGroup = new QGroupBox("脚本事件信息");
    auto* rightLayout = new QVBoxLayout(scriptGroup);
    rightLayout->setSpacing(6);
    auto* metadataRow = new QHBoxLayout();
    metadataRow->addWidget(infoGroup, 3);
    metadataRow->addWidget(recentGroup, 2);
    rightLayout->addLayout(metadataRow);

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

    m_eventTableView->setObjectName("scriptEvents");
    m_eventTableView->setMinimumHeight(fontMetrics().lineSpacing() * 4);
    mainLayout->addWidget(scriptGroup, 1);
    for (auto* group : {recordGroup, playbackGroup, controlGroup, opsGroup, infoGroup, recentGroup}) {
        group->layout()->setContentsMargins(8, 8, 8, 6);
        group->layout()->setSpacing(6);
        group->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    }

}

QSize ScriptPage::recommendedPageSize() const
{
    // 隐藏页可能尚未经过 resizeEvent；直接取两组配置尺寸，避免用窄屏堆叠后的缓存。
    const int width = m_recordGroup->sizeHint().width() + m_playbackGroup->sizeHint().width()
        + m_configurationLayout->spacing() + 24;
    const int configHeight = qMax(m_recordGroup->sizeHint().height(), m_playbackGroup->sizeHint().height()) + 2;
    return QSize(qMax(width, minimumSizeHint().width()),
                 qMax(sizeHint().height(), configHeight * 2 + 24));
}

void ScriptPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // 根据控件的最小尺寸决定换行，不提高主窗口最小宽度。
    const int requiredWidth = m_recordGroup->minimumSizeHint().width()
        + m_playbackGroup->minimumSizeHint().width() + m_configurationLayout->spacing();
    const bool stacked = width() - 24 < requiredWidth;
    m_configurationLayout->removeWidget(m_playbackGroup);
    m_configurationLayout->addWidget(m_playbackGroup, stacked ? 1 : 0, stacked ? 0 : 1);
    m_configurationLayout->setColumnStretch(1, stacked ? 0 : 3);
    m_configurationLayout->activate();
    // 正常尺寸显示完整配置；高度不足时仅配置区滚动，表格获得剩余空间。
    const int naturalHeight = m_configurationPanel->sizeHint().height() + 2;
    m_configurationScroll->setMaximumHeight(qMax(fontMetrics().lineSpacing() * 5,
        qMin(naturalHeight, height() / 2)));
    m_configurationScroll->updateGeometry();
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
    connect(m_eventModel, &ScriptEventTableModel::documentModified, this, [this]() {
        m_modified = true; updateEventCount();
    });
    connect(m_scriptNameEdit, &QLineEdit::textEdited, this, [this]() { m_modified = true; });
    connect(m_scriptDescEdit, &QTextEdit::textChanged, this, [this]() { m_modified = true; });

    // 无限重复
    connect(m_infiniteRepeatCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_repeatCountSpinBox->setEnabled(!checked);
    });

    // 快捷键变更时立刻保存并检测冲突 (冲突时恢复原值)
    connect(m_recordHotkeyEdit, &HotkeyEdit::hotkeyChanged, this, [this](const HotkeyInfo& hk) {
        HotkeyInfo oldHk = m_settings->recordingHotkey();
        m_settings->setRecordingHotkey(hk);
        QString conflict = m_settings->checkHotkeyConflicts();
        if (!conflict.isEmpty()) {
            // 恢复原来的快捷键
            m_settings->setRecordingHotkey(oldHk);
            m_recordHotkeyEdit->blockSignals(true);
            m_recordHotkeyEdit->setHotkey(oldHk);
            m_recordHotkeyEdit->blockSignals(false);
            QMessageBox::warning(this, "快捷键冲突",
                conflict + "\n\n已恢复为原来的快捷键。");
        }
    });
    connect(m_playbackHotkeyEdit, &HotkeyEdit::hotkeyChanged, this, [this](const HotkeyInfo& hk) {
        HotkeyInfo oldHk = m_settings->playbackHotkey();
        m_settings->setPlaybackHotkey(hk);
        QString conflict = m_settings->checkHotkeyConflicts();
        if (!conflict.isEmpty()) {
            // 恢复原来的快捷键
            m_settings->setPlaybackHotkey(oldHk);
            m_playbackHotkeyEdit->blockSignals(true);
            m_playbackHotkeyEdit->setHotkey(oldHk);
            m_playbackHotkeyEdit->blockSignals(false);
            QMessageBox::warning(this, "快捷键冲突",
                conflict + "\n\n已恢复为原来的快捷键。");
        }
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
    if (!confirmDiscardChanges()) return;
    QString defaultDir = m_settings->scriptDefaultDir();
    QString filePath = QFileDialog::getOpenFileName(this, "导入脚本", defaultDir,
        ScriptSerializer::fileFilter());
    if (filePath.isEmpty()) return;

    QString error;
    if (!m_controller->scriptSerializer()->load(filePath, m_document, &error)) {
        QMessageBox::critical(this, "导入失败", error);
        return;
    }

    m_currentFilePath = m_document.filePath;
    setDocument(m_document);
    m_modified = false;
    m_eventModel->refreshAll();
    updateEventCount();
    m_settings->addRecentScript(m_currentFilePath);
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
    m_document.playbackSettings = playbackSettings();
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
    // 录制标题含时间冒号，文档名称保留，默认文件名转换为 Windows 合法名称。
    defaultName.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")), "_");
    if (!defaultName.endsWith(".kms")) defaultName += ".kms";
    QString filePath = QFileDialog::getSaveFileName(this, "另存为",
        defaultDir + "/" + defaultName, ScriptSerializer::fileFilter());
    if (filePath.isEmpty()) return;

    m_document.playbackSettings = playbackSettings();
    m_document.name = m_scriptNameEdit->text();
    m_document.description = m_scriptDescEdit->toPlainText();

    QString error;
    if (!m_controller->scriptSerializer()->save(m_document, filePath, &error)) {
        QMessageBox::critical(this, "保存失败", error);
        return;
    }

    m_currentFilePath = m_document.filePath;
    m_modified = false;
    m_settings->addRecentScript(m_currentFilePath);
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

    m_document.events.clear();
    m_document.markModified();
    m_modified = true;
    m_eventModel->refreshAll();
    updateEventCount();
}

void ScriptPage::onInsertWaitEvent()
{
    // v1 无等待事件类型：顺延后续时间戳，不伪造会把鼠标移到原点的事件。
    const QModelIndex current = m_eventTableView->currentIndex();
    if (m_document.isEmpty()) return;
    const int row = current.isValid() ? current.row() : 0;
    for (int i = row; i < m_document.eventCount(); ++i)
        m_document.events[i].timestampMs += 1000;
    m_document.markModified();
    m_modified = true;
    m_eventModel->refreshAll();
    updateEventCount();
}

void ScriptPage::onRecentScriptSelected(const QModelIndex& index)
{
    if (!confirmDiscardChanges()) return;
    QString path = m_recentScriptModel->data(index, Qt::DisplayRole).toString();
    if (path.isEmpty()) return;

    QString error;
    if (!m_controller->scriptSerializer()->load(path, m_document, &error)) {
        emit statusMessage("加载失败: " + error);
        return;
    }

    m_currentFilePath = path;
    setDocument(m_document);
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
    const auto options = m_settings->scriptOptions();
    m_playbackStartDelaySpinBox->setValue(options.value("startDelay", 0).toInt());
    m_roundIntervalSpinBox->setValue(options.value("roundInterval", 1000).toInt());
    m_infiniteRepeatCheckBox->setChecked(options.value("infinite", false).toBool());
    m_restoreCursorCheckBox->setChecked(options.value("restoreCursor", true).toBool());
    m_skipDisabledCheckBox->setChecked(options.value("skipDisabled", true).toBool());
    m_coordModeCombo->setCurrentIndex(m_coordModeCombo->findData(options.value("coordMode", int(CoordinateMode::MonitorRelative))));
    m_recordMouseMoveCheck->setChecked(options.value("recordMove", true).toBool());
    m_recordMouseClickCheck->setChecked(options.value("recordClick", true).toBool());
    m_recordWheelCheck->setChecked(options.value("recordWheel", true).toBool());
    m_recordKeyboardCheck->setChecked(options.value("recordKeyboard", true).toBool());
    m_moveMinIntervalSpinBox->setValue(options.value("moveInterval", 10).toInt());
    m_moveMinDistanceSpinBox->setValue(options.value("moveDistance", 3).toInt());
    m_repeatCountSpinBox->setValue(m_settings->scriptRepeatCount());

    double speed = m_settings->scriptPlaybackSpeed();
    for (int i = 0; i < m_speedCombo->count(); ++i) {
        if (qAbs(m_speedCombo->itemData(i).toDouble() - speed) < 0.01) {
            m_speedCombo->setCurrentIndex(i);
            break;
        }
    }

    m_recordHotkeyEdit->setHotkey(m_settings->recordingHotkey());
    m_playbackHotkeyEdit->setHotkey(m_settings->playbackHotkey());

    refreshRecentScripts();
}

void ScriptPage::saveSettings()
{
    m_settings->setScriptOptions({
        {"startDelay", m_playbackStartDelaySpinBox->value()},
        {"roundInterval", m_roundIntervalSpinBox->value()},
        {"infinite", m_infiniteRepeatCheckBox->isChecked()},
        {"restoreCursor", m_restoreCursorCheckBox->isChecked()},
        {"skipDisabled", m_skipDisabledCheckBox->isChecked()},
        {"coordMode", m_coordModeCombo->currentData()},
        {"recordMove", m_recordMouseMoveCheck->isChecked()},
        {"recordClick", m_recordMouseClickCheck->isChecked()},
        {"recordWheel", m_recordWheelCheck->isChecked()},
        {"recordKeyboard", m_recordKeyboardCheck->isChecked()},
        {"moveInterval", m_moveMinIntervalSpinBox->value()},
        {"moveDistance", m_moveMinDistanceSpinBox->value()}
    });
    double speed = m_speedCombo->currentData().toDouble();
    m_settings->setScriptPlaybackSpeed(speed);
    m_settings->setScriptRepeatCount(m_repeatCountSpinBox->value());
    m_settings->setRecordingHotkey(m_recordHotkeyEdit->hotkey());
    m_settings->setPlaybackHotkey(m_playbackHotkeyEdit->hotkey());
    m_settings->sync();
}

void ScriptPage::setRunningState(bool running)
{
    m_playbackStartBtn->setEnabled(!running);
    m_playbackStopBtn->setEnabled(running);
    m_recordStartBtn->setEnabled(!running);
    const TaskState state = m_controller->taskManager()->currentState();
    m_recordStopBtn->setEnabled(running && state == TaskState::Recording);
    m_playbackStopBtn->setEnabled(running && (state == TaskState::Playing || state == TaskState::Paused));

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

bool ScriptPage::confirmDiscardChanges()
{
    if (!m_modified) return true;
    return QMessageBox::question(this, "未保存的脚本", "当前脚本有未保存的修改，是否放弃修改？",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}

void ScriptPage::setDocument(const ScriptDocument& doc)
{
    m_document = doc;
    m_currentFilePath = doc.filePath;
    const auto& pb = doc.playbackSettings;
    m_playbackStartDelaySpinBox->setValue(pb.startDelayMs);
    m_repeatCountSpinBox->setValue(pb.repeatCount);
    m_infiniteRepeatCheckBox->setChecked(pb.infiniteRepeat);
    m_roundIntervalSpinBox->setValue(pb.roundIntervalMs);
    m_restoreCursorCheckBox->setChecked(pb.restoreCursor);
    m_skipDisabledCheckBox->setChecked(pb.skipDisabledEvents);
    m_coordModeCombo->setCurrentIndex(m_coordModeCombo->findData(static_cast<int>(pb.coordinateMode)));
    const int speedIndex = m_speedCombo->findData(pb.speedFactor);
    if (speedIndex >= 0) m_speedCombo->setCurrentIndex(speedIndex);
    else {
        m_speedCombo->addItem(QString("%1x").arg(pb.speedFactor), pb.speedFactor);
        m_speedCombo->setCurrentIndex(m_speedCombo->count() - 1);
    }
    m_eventModel->setDocument(&m_document);
    m_eventModel->refreshAll();
    m_scriptNameEdit->setText(doc.name);
    m_scriptDescEdit->setPlainText(doc.description);
    m_modified = doc.filePath.isEmpty();
    updateEventCount();
}

RecordingSettings ScriptPage::recordingSettings() const
{
    RecordingSettings settings;
    settings.recordMouseMove  = m_recordMouseMoveCheck->isChecked();
    settings.recordMouseClick = m_recordMouseClickCheck->isChecked();
    settings.recordWheel      = m_recordWheelCheck->isChecked();
    settings.recordKeyboard   = m_recordKeyboardCheck->isChecked();
    settings.mouseMoveMinIntervalMs = m_moveMinIntervalSpinBox->value();
    settings.mouseMoveMinDistance    = m_moveMinDistanceSpinBox->value();
    settings.ignoreOwnWindow  = true;
    settings.ignoreSimulated  = true;
    settings.stopHotkey = m_settings->recordingHotkey();
    return settings;
}

PlaybackSettings ScriptPage::playbackSettings() const
{
    PlaybackSettings settings;
    settings.startDelayMs     = m_playbackStartDelaySpinBox->value();
    settings.repeatCount      = m_repeatCountSpinBox->value();
    settings.infiniteRepeat   = m_infiniteRepeatCheckBox->isChecked();
    settings.roundIntervalMs  = m_roundIntervalSpinBox->value();
    settings.speedFactor      = m_speedCombo->currentData().toDouble();
    settings.restoreCursor    = m_restoreCursorCheckBox->isChecked();
    settings.skipDisabledEvents = m_skipDisabledCheckBox->isChecked();
    settings.coordinateMode   = static_cast<CoordinateMode>(m_coordModeCombo->currentData().toInt());
    return settings;
}

void ScriptPage::enableEditingControls(bool enable)
{
    m_eventTableView->setEnabled(enable);
    m_newBtn->setEnabled(enable);
    m_importBtn->setEnabled(enable);
    m_recentScriptList->setEnabled(enable);
    m_saveBtn->setEnabled(enable);
    m_saveAsBtn->setEnabled(enable);
    m_recordHotkeyEdit->setEnabled(enable);
    m_playbackHotkeyEdit->setEnabled(enable);
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
