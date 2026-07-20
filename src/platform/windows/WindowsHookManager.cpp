#include "WindowsHookManager.h"
#include "WindowsInputSimulator.h"
#include "KeyMapper.h"
#include "TimeUtils.h"
#include "Logger.h"
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winuser.h>
#endif

WindowsHookManager* WindowsHookManager::s_instance = nullptr;

WindowsHookManager::WindowsHookManager(QObject* parent)
    : QObject(parent)
{
    s_instance = this;

    m_workerThread = new QThread(this);
    m_workerThread->start();
}

WindowsHookManager::~WindowsHookManager()
{
    stopRecording();
    uninstallAllHooks();

    m_stopProcessing = true;
    m_rawEventCondition.wakeAll();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    s_instance = nullptr;
}

#ifdef Q_OS_WIN

LRESULT CALLBACK WindowsHookManager::mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && s_instance && s_instance->m_recording.load()) {
        MSLLHOOKSTRUCT* pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        // 过滤本软件模拟的输入
        if (s_instance->m_ignoreSimulated
            && pMouse->dwExtraInfo == WindowsInputSimulator::APP_EXTRA_INFO) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        // 本软件窗口事件过滤在这里处理 (通过窗口检测)
        // 在 convert 阶段做进一步过滤

        RawHookEvent raw;
        raw.rawType = RawHookEvent::Mouse;
        raw.timestampMs = TimeUtils::currentTimeMs();
        raw.mouseX = pMouse->pt.x;
        raw.mouseY = pMouse->pt.y;
        raw.mouseFlags = static_cast<DWORD>(wParam);
        raw.mouseData = static_cast<DWORD>(pMouse->mouseData);
        raw.extraInfo = pMouse->dwExtraInfo;

        {
            QMutexLocker locker(&s_instance->m_rawQueueMutex);
            s_instance->m_rawEventQueue.enqueue(raw);
        }
        s_instance->m_rawEventCondition.wakeOne();
        emit s_instance->rawEventReady();
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK WindowsHookManager::keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && s_instance && s_instance->m_recording.load()) {
        KBDLLHOOKSTRUCT* pKbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        // 过滤本软件模拟的输入
        if (s_instance->m_ignoreSimulated
            && pKbd->dwExtraInfo == WindowsInputSimulator::APP_EXTRA_INFO) {
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        RawHookEvent raw;
        raw.rawType = RawHookEvent::Keyboard;
        raw.timestampMs = TimeUtils::currentTimeMs();
        raw.vkCode = pKbd->vkCode;
        raw.scanCode = pKbd->scanCode;
        raw.keyFlags = pKbd->flags;
        raw.keyExtraInfo = pKbd->dwExtraInfo;

        {
            QMutexLocker locker(&s_instance->m_rawQueueMutex);
            s_instance->m_rawEventQueue.enqueue(raw);
        }
        s_instance->m_rawEventCondition.wakeOne();
        emit s_instance->rawEventReady();
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

#else
// Stub static callbacks for non-Windows
#endif

bool WindowsHookManager::installMouseHook()
{
#ifdef Q_OS_WIN
    if (m_mouseHookInstalled) return true;

    m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseHookProc,
                                     GetModuleHandleW(nullptr), 0);
    if (!m_mouseHook) {
        DWORD err = GetLastError();
        QString msg = QString("安装鼠标钩子失败，错误码: %1").arg(err);
        LOG_ERROR(msg);
        emit hookError(msg);
        return false;
    }

    m_mouseHookInstalled = true;
    LOG_INFO("鼠标全局钩子已安装");
    return true;
#else
    LOG_WARNING("非Windows平台，鼠标钩子不可用");
    return false;
#endif
}

bool WindowsHookManager::installKeyboardHook()
{
#ifdef Q_OS_WIN
    if (m_keyboardHookInstalled) return true;

    m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHookProc,
                                        GetModuleHandleW(nullptr), 0);
    if (!m_keyboardHook) {
        DWORD err = GetLastError();
        QString msg = QString("安装键盘钩子失败，错误码: %1").arg(err);
        LOG_ERROR(msg);
        emit hookError(msg);
        return false;
    }

    m_keyboardHookInstalled = true;
    LOG_INFO("键盘全局钩子已安装");
    return true;
#else
    LOG_WARNING("非Windows平台，键盘钩子不可用");
    return false;
#endif
}

void WindowsHookManager::uninstallMouseHook()
{
#ifdef Q_OS_WIN
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
#endif
    m_mouseHookInstalled = false;
    LOG_INFO("鼠标全局钩子已卸载");
}

void WindowsHookManager::uninstallKeyboardHook()
{
#ifdef Q_OS_WIN
    if (m_keyboardHook) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
    }
#endif
    m_keyboardHookInstalled = false;
    LOG_INFO("键盘全局钩子已卸载");
}

void WindowsHookManager::uninstallAllHooks()
{
    uninstallMouseHook();
    uninstallKeyboardHook();
}

void WindowsHookManager::startRecording(const RecordingSettings& settings)
{
    if (m_recording.load()) return;

    m_settings = settings;
    m_processedEvents.clear();
    m_rawEventQueue.clear();
    m_recordingStartTime = TimeUtils::currentTimeMs();
    m_lastRecordedMousePos = QPoint();
    m_lastMouseMoveTime = 0;
    m_currentModifiers = 0;

    // 根据设置安装对应钩子
    bool needMouse = settings.recordMouseMove || settings.recordMouseClick || settings.recordWheel;
    bool needKeyboard = settings.recordKeyboard;

    if (needMouse) installMouseHook();
    if (needKeyboard) installKeyboardHook();

    m_recording.store(true);

    // 启动事件处理 (在工作线程中运行)
    QMetaObject::invokeMethod(this, "processRawEvents", Qt::QueuedConnection);

    LOG_INFO("开始录制键鼠事件");
    emit recordingStarted();
}

void WindowsHookManager::stopRecording()
{
    if (!m_recording.load()) return;

    m_recording.store(false);
    m_rawEventCondition.wakeAll();

    // 卸载钩子
    uninstallAllHooks();

    LOG_INFO(QString("停止录制，共 %1 个事件").arg(m_processedEvents.size()));
    emit recordingStopped();
}

QVector<ScriptEvent> WindowsHookManager::takeRecordedEvents()
{
    QMutexLocker locker(&m_processedMutex);
    QVector<ScriptEvent> events = m_processedEvents;
    m_processedEvents.clear();
    return events;
}

void WindowsHookManager::setOwnWindowHandle(void* hwnd)
{
#ifdef Q_OS_WIN
    m_ownHwnd = reinterpret_cast<HWND>(hwnd);
#else
    m_ownHwnd = hwnd;
#endif
}

void WindowsHookManager::setIgnoreSimulatedInput(bool ignore)
{
    m_ignoreSimulated = ignore;
}

// ============================================================================
// 事件处理 (在工作线程中运行)
// ============================================================================
void WindowsHookManager::processRawEvents()
{
    ScriptEvent lastMouseEvent; // 用于鼠标移动压缩
    bool hasLastMouse = false;

    while (m_recording.load() || !m_rawEventQueue.isEmpty()) {
        RawHookEvent raw;
        {
            QMutexLocker locker(&m_rawQueueMutex);
            if (m_rawEventQueue.isEmpty()) {
                if (!m_recording.load()) break;
                // 等待新事件 (最多100ms，以便检查停止标志)
                m_rawEventCondition.wait(&m_rawQueueMutex, 100);
                continue;
            }
            raw = m_rawEventQueue.dequeue();
        }

        if (m_stopProcessing.load()) break;

        ScriptEvent ev;
        if (raw.rawType == RawHookEvent::Mouse) {
            ev = convertMouseEvent(raw);

            // 鼠标移动压缩
            if (ev.type == ScriptEventType::MouseMove && hasLastMouse) {
                if (!shouldRecordMouseMove(lastMouseEvent, ev))
                    continue;
            }

            if (ev.type == ScriptEventType::MouseMove) {
                lastMouseEvent = ev;
                hasLastMouse = true;
            }
        } else {
            ev = convertKeyboardEvent(raw);
        }

        if (ev.isValid()) {
            ev.eventIndex = m_processedEvents.size();
            QMutexLocker locker(&m_processedMutex);
            m_processedEvents.append(ev);
        }
    }
}

ScriptEvent WindowsHookManager::convertMouseEvent(const RawHookEvent& raw)
{
    ScriptEvent ev;
    ev.timestampMs = raw.timestampMs - m_recordingStartTime;

    switch (raw.mouseFlags) {
    case WM_MOUSEMOVE:
        if (!m_settings.recordMouseMove) return {};
        ev.type = ScriptEventType::MouseMove;
        ev.virtualDesktopPos = QPoint(raw.mouseX, raw.mouseY);
        ev.monitorInternalPos = QPoint(); // 后续由MonitorManager填充
        break;
    case WM_LBUTTONDOWN:
        if (!m_settings.recordMouseClick) return {};
        ev.type = ScriptEventType::MouseDown;
        ev.mouseButton = MouseButton::Left;
        ev.virtualDesktopPos = QPoint(raw.mouseX, raw.mouseY);
        break;
    case WM_LBUTTONUP:
        if (!m_settings.recordMouseClick) return {};
        ev.type = ScriptEventType::MouseUp;
        ev.mouseButton = MouseButton::Left;
        ev.virtualDesktopPos = QPoint(raw.mouseX, raw.mouseY);
        break;
    case WM_RBUTTONDOWN:
        if (!m_settings.recordMouseClick) return {};
        ev.type = ScriptEventType::MouseDown;
        ev.mouseButton = MouseButton::Right;
        ev.virtualDesktopPos = QPoint(raw.mouseX, raw.mouseY);
        break;
    case WM_RBUTTONUP:
        if (!m_settings.recordMouseClick) return {};
        ev.type = ScriptEventType::MouseUp;
        ev.mouseButton = MouseButton::Right;
        ev.virtualDesktopPos = QPoint(raw.mouseX, raw.mouseY);
        break;
    case WM_MBUTTONDOWN:
        if (!m_settings.recordMouseClick) return {};
        ev.type = ScriptEventType::MouseDown;
        ev.mouseButton = MouseButton::Middle;
        ev.virtualDesktopPos = QPoint(raw.mouseX, raw.mouseY);
        break;
    case WM_MBUTTONUP:
        if (!m_settings.recordMouseClick) return {};
        ev.type = ScriptEventType::MouseUp;
        ev.mouseButton = MouseButton::Middle;
        ev.virtualDesktopPos = QPoint(raw.mouseX, raw.mouseY);
        break;
    case WM_XBUTTONDOWN:
        if (!m_settings.recordMouseClick) return {};
        ev.type = ScriptEventType::MouseDown;
        ev.mouseButton = (HIWORD(raw.mouseData) == XBUTTON1) ? MouseButton::XButton1 : MouseButton::XButton2;
        ev.virtualDesktopPos = QPoint(raw.mouseX, raw.mouseY);
        break;
    case WM_XBUTTONUP:
        if (!m_settings.recordMouseClick) return {};
        ev.type = ScriptEventType::MouseUp;
        ev.mouseButton = (HIWORD(raw.mouseData) == XBUTTON1) ? MouseButton::XButton1 : MouseButton::XButton2;
        ev.virtualDesktopPos = QPoint(raw.mouseX, raw.mouseY);
        break;
    case WM_MOUSEWHEEL:
        if (!m_settings.recordWheel) return {};
        ev.type = ScriptEventType::MouseWheel;
        ev.wheelDelta = static_cast<int>(static_cast<SHORT>(HIWORD(raw.mouseData)));
        ev.virtualDesktopPos = QPoint(raw.mouseX, raw.mouseY);
        break;
    case WM_MOUSEHWHEEL:
        if (!m_settings.recordWheel) return {};
        ev.type = ScriptEventType::MouseHWheel;
        ev.wheelDelta = static_cast<int>(static_cast<SHORT>(HIWORD(raw.mouseData)));
        ev.virtualDesktopPos = QPoint(raw.mouseX, raw.mouseY);
        break;
    default:
        return {};
    }

    // 记录默认显示器信息 (由MonitorManager后续更新)
    ev.monitorDeviceName = "";
    ev.monitorInternalPos = QPoint();
    ev.monitorRatioPos = QPointF();

    return ev;
}

ScriptEvent WindowsHookManager::convertKeyboardEvent(const RawHookEvent& raw)
{
    if (!m_settings.recordKeyboard) return {};

    ScriptEvent ev;
    ev.timestampMs = raw.timestampMs - m_recordingStartTime;

    bool isKeyUp = (raw.keyFlags & 0x80) != 0;
    ev.type = isKeyUp ? ScriptEventType::KeyUp : ScriptEventType::KeyDown;
    ev.winVk = raw.vkCode;
    ev.scanCode = raw.scanCode;
    ev.isExtendedKey = (raw.keyFlags & 0x01) != 0;

    // 追踪修饰键状态
    if (!isKeyUp) {
        switch (raw.vkCode) {
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
            m_currentModifiers |= 0x01; ev.hasCtrl = true; break;
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
            m_currentModifiers |= 0x02; ev.hasShift = true; break;
        case VK_MENU: case VK_LMENU: case VK_RMENU:
            m_currentModifiers |= 0x04; ev.hasAlt = true; break;
        case VK_LWIN: case VK_RWIN:
            m_currentModifiers |= 0x08; ev.hasWin = true; break;
        }
    } else {
        switch (raw.vkCode) {
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
            m_currentModifiers &= ~0x01; break;
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
            m_currentModifiers &= ~0x02; break;
        case VK_MENU: case VK_LMENU: case VK_RMENU:
            m_currentModifiers &= ~0x04; break;
        case VK_LWIN: case VK_RWIN:
            m_currentModifiers &= ~0x08; break;
        }
    }

    // 记录当前修饰键状态
    ev.modifiersAtPress = m_currentModifiers;
    ev.hasCtrl  = (m_currentModifiers & 0x01) != 0;
    ev.hasShift = (m_currentModifiers & 0x02) != 0;
    ev.hasAlt   = (m_currentModifiers & 0x04) != 0;
    ev.hasWin   = (m_currentModifiers & 0x08) != 0;

    // 生成可读名称
    QStringList parts;
    if (ev.hasCtrl)  parts << "Ctrl";
    if (ev.hasShift) parts << "Shift";
    if (ev.hasAlt)   parts << "Alt";
    if (ev.hasWin)   parts << "Win";

    QString keyName = KeyMapper::winVkToDisplayName(raw.vkCode, ev.isExtendedKey);
    parts << keyName;
    ev.keyDisplayName = parts.join(" + ");

    return ev;
}

bool WindowsHookManager::shouldRecordMouseMove(const ScriptEvent& last, const ScriptEvent& current) const
{
    Q_UNUSED(last)
    Q_UNUSED(current)
    int64_t timeDiff = current.timestampMs - last.timestampMs;
    int distX = current.virtualDesktopPos.x() - last.virtualDesktopPos.x();
    int distY = current.virtualDesktopPos.y() - last.virtualDesktopPos.y();
    int distance = qAbs(distX) + qAbs(distY);

    if (timeDiff < m_settings.mouseMoveMinIntervalMs
        && distance < m_settings.mouseMoveMinDistance)
        return false;

    return true;
}
