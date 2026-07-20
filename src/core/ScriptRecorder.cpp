#include "ScriptRecorder.h"
#include "platform/windows/WindowsHookManager.h"
#include "core/MonitorManager.h"
#include "core/CoordinateMapper.h"
#include "utils/Logger.h"
#include "utils/TimeUtils.h"

ScriptRecorder::ScriptRecorder(WindowsHookManager* hookManager,
                                 MonitorManager* monitorMgr,
                                 QObject* parent)
    : QObject(parent)
    , m_hookManager(hookManager)
    , m_monitorMgr(monitorMgr)
{
    connect(m_hookManager, &WindowsHookManager::recordingStopped,
            this, &ScriptRecorder::onHookRecordingStopped);
}

ScriptRecorder::~ScriptRecorder()
{
    stopRecording();
}

bool ScriptRecorder::startRecording(const RecordingSettings& settings)
{
    if (m_recording.load()) return false;

    m_settings = settings;
    m_document.clear();
    m_document.recordingSettings = settings;
    m_document.name = QString("录制脚本 %1")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    // 记录当前桌面和显示器信息
    m_document.virtualDesktopBounds = m_monitorMgr->virtualDesktopBounds();
    m_document.monitors = m_monitorMgr->monitors();
    m_document.coordinateMode = CoordinateMode::MonitorRelative;

    m_recording.store(true);
    m_state = TaskState::Recording;
    emit stateChanged(m_state);

    // 启动钩子录制
    m_hookManager->startRecording(settings);

    LOG_INFO("脚本录制已启动");
    emit recordingStarted();

    return true;
}

void ScriptRecorder::stopRecording()
{
    if (!m_recording.load()) return;

    m_hookManager->stopRecording();
    m_recording.store(false);
}

void ScriptRecorder::onHookRecordingStopped()
{
    // 从钩子管理器获取录制的事件
    QVector<ScriptEvent> rawEvents = m_hookManager->takeRecordedEvents();

    // 补充显示器信息
    enrichEventsWithMonitorInfo(rawEvents);

    // 移除录制快捷键产生的尾部事件
    removeTrailingHotkeyEvents(rawEvents);

    // 保存到文档
    {
        QMutexLocker locker(&m_documentMutex);
        m_document.events = rawEvents;
        for (int i = 0; i < m_document.events.size(); ++i)
            m_document.events[i].eventIndex = i;
        m_document.markModified();
    }

    m_state = TaskState::Idle;
    emit stateChanged(m_state);
    emit eventCountChanged(m_document.eventCount());
    emit recordingStopped();

    LOG_INFO(QString("录制完成: %1 个事件").arg(m_document.eventCount()));
}

ScriptDocument ScriptRecorder::takeDocument()
{
    QMutexLocker locker(&m_documentMutex);
    ScriptDocument doc = m_document;
    m_document.clear();
    return doc;
}

bool ScriptRecorder::hasEvents() const
{
    return m_document.eventCount() > 0;
}

// ============================================================================
// 为事件补充显示器信息
// ============================================================================
void ScriptRecorder::enrichEventsWithMonitorInfo(QVector<ScriptEvent>& events)
{
    const auto& monitors = m_monitorMgr->monitors();

    for (auto& ev : events) {
        if (!ev.isMouseEvent()) continue;

        // 查找坐标所在的显示器
        MonitorInfo monitor = m_monitorMgr->monitorAtPoint(ev.virtualDesktopPos);
        if (monitor.deviceName.isEmpty()) continue;

        ev.monitorDeviceName = monitor.deviceName;
        ev.monitorInternalPos = monitor.virtualToInternal(ev.virtualDesktopPos);
        ev.monitorRatioPos = monitor.internalToRatio(ev.monitorInternalPos);
    }
}

// ============================================================================
// 移除录制快捷键产生的尾部事件
// 停止录制时，停止快捷键可能被记录为最后的按键事件
// ============================================================================
void ScriptRecorder::removeTrailingHotkeyEvents(QVector<ScriptEvent>& events)
{
    if (events.isEmpty()) return;

    // 从末尾向前查找，移除由停止快捷键产生的 keydown/keyup 事件
    // 策略: 如果最后几个事件是连续的键盘事件且时间非常接近，
    // 可能是停止快捷键，将其移除

    int removeCount = 0;
    int64_t thresholdMs = 500; // 停止快捷键事件窗口

    // 从最后一个事件向前检查
    for (int i = events.size() - 1; i >= 0; --i) {
        const auto& ev = events[i];
        if (ev.isKeyboardEvent()) {
            // 检查是否为修饰键或普通键的按下/释放事件
            int64_t timeFromEnd = events.last().timestampMs - ev.timestampMs;
            if (timeFromEnd < thresholdMs)
                removeCount++;
            else
                break;
        } else {
            break;
        }
    }

    // 最多移除4个事件 (一个组合键的按下+释放)
    removeCount = qMin(removeCount, 4);

    if (removeCount > 0) {
        events.remove(events.size() - removeCount, removeCount);
        LOG_INFO(QString("已移除尾部 %1 个疑似快捷键事件").arg(removeCount));
    }
}

void ScriptRecorder::processRecordedEvents()
{
    // 此方法由信号驱动调用，实际处理在 onHookRecordingStopped 中完成
}
