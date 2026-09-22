#include "ScriptRecorder.h"
#include "platform/windows/WindowsHookManager.h"
#include "core/MonitorManager.h"
#include "core/CoordinateMapper.h"
#include "utils/Logger.h"
#include "utils/TimeUtils.h"
#include "utils/KeyMapper.h"

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
    if (!m_hookManager->startRecording(settings)) {
        m_recording = false;
        m_state = TaskState::Error;
        emit stateChanged(m_state);
        emit errorOccurred("无法安装录制钩子或未选择录制事件");
        return false;
    }

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
    m_recording = false;
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
    for (auto& ev : events) {
        if (!ev.isMouseEvent()) continue;

        // 查找坐标所在的显示器
        MonitorInfo monitor;
        for (const auto& candidate : m_document.monitors) {
            if (candidate.containsVirtualPoint(ev.virtualDesktopPos)) { monitor = candidate; break; }
        }
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
    // 不再删除“最后 500ms 的任意按键”，这会丢失用户正常输入。
    const uint32_t vk = KeyMapper::qtKeyToWinVk(m_settings.stopHotkey.key);
    while (!events.isEmpty() && events.last().isKeyboardEvent()
           && vk != 0 && events.last().winVk == vk)
        events.removeLast();
}

void ScriptRecorder::processRecordedEvents()
{
    // 此方法由信号驱动调用，实际处理在 onHookRecordingStopped 中完成
}
