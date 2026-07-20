#include "WindowsInputSimulator.h"
#include "AppTypes.h"
#include "Logger.h"
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winuser.h>
#endif

WindowsInputSimulator::WindowsInputSimulator(QObject* parent)
    : QObject(parent)
{
}

WindowsInputSimulator::~WindowsInputSimulator()
{
    releaseAllInputs();
}

// ============================================================================
// 鼠标绝对移动
// 将虚拟桌面坐标映射到 Windows 的 0~65535 绝对范围
// ============================================================================
bool WindowsInputSimulator::mouseMoveAbsolute(int virtualDesktopX, int virtualDesktopY,
                                               int virtualDesktopWidth, int virtualDesktopHeight)
{
#ifdef Q_OS_WIN
    // 保护: 宽高不能为0或1
    if (virtualDesktopWidth <= 1) virtualDesktopWidth = 2;
    if (virtualDesktopHeight <= 1) virtualDesktopHeight = 2;

    // 映射公式:
    //   normalizedX = (virtualX - virtualLeft) * 65535 / (virtualWidth - 1)
    // 虚拟桌面左上角不一定是 (0,0)，需要考虑偏移

    // 获取虚拟桌面左上角
    int virtualLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int virtualTop  = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int virtW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int virtH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // 使用实际系统虚拟桌面范围
    if (virtW > 1 && virtH > 1) {
        virtualDesktopWidth = virtW;
        virtualDesktopHeight = virtH;
    }

    // 计算归一化坐标
    // 减去虚拟桌面原点，映射到 [0, 65535]
    int64_t normX = (static_cast<int64_t>(virtualDesktopX) - virtualLeft) * 65535
                    / (virtualDesktopWidth - 1);
    int64_t normY = (static_cast<int64_t>(virtualDesktopY) - virtualTop) * 65535
                    / (virtualDesktopHeight - 1);

    // 边界裁剪
    if (normX < 0) normX = 0;
    if (normX > 65535) normX = 65535;
    if (normY < 0) normY = 0;
    if (normY > 65535) normY = 65535;

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>(normX);
    input.mi.dy = static_cast<LONG>(normY);
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    input.mi.dwExtraInfo = APP_EXTRA_INFO;

    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        LOG_ERROR(QString("SendInput mouseMoveAbsolute failed: (%1,%2) virt=(%3,%4,%5x%6)")
            .arg(virtualDesktopX).arg(virtualDesktopY)
            .arg(virtualLeft).arg(virtualTop).arg(virtualDesktopWidth).arg(virtualDesktopHeight));
        emit inputError("鼠标绝对移动失败");
        return false;
    }
    return true;
#else
    Q_UNUSED(virtualDesktopX); Q_UNUSED(virtualDesktopY);
    Q_UNUSED(virtualDesktopWidth); Q_UNUSED(virtualDesktopHeight);
    return true; // stub for non-Windows
#endif
}

bool WindowsInputSimulator::mouseMoveRelative(int deltaX, int deltaY)
{
#ifdef Q_OS_WIN
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = deltaX;
    input.mi.dy = deltaY;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dwExtraInfo = APP_EXTRA_INFO;

    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        emit inputError("鼠标相对移动失败");
        return false;
    }
    return true;
#else
    Q_UNUSED(deltaX); Q_UNUSED(deltaY);
    return true;
#endif
}

bool WindowsInputSimulator::mouseDown(MouseButton button)
{
#ifdef Q_OS_WIN
    DWORD flag = 0;
    switch (button) {
    case MouseButton::Left:     flag = MOUSEEVENTF_LEFTDOWN; break;
    case MouseButton::Right:    flag = MOUSEEVENTF_RIGHTDOWN; break;
    case MouseButton::Middle:   flag = MOUSEEVENTF_MIDDLEDOWN; break;
    case MouseButton::XButton1: flag = MOUSEEVENTF_XDOWN; break;
    case MouseButton::XButton2: flag = MOUSEEVENTF_XDOWN; break;
    }

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flag;
    input.mi.dwExtraInfo = APP_EXTRA_INFO;
    if (button == MouseButton::XButton1)
        input.mi.mouseData = XBUTTON1;
    else if (button == MouseButton::XButton2)
        input.mi.mouseData = XBUTTON2;

    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        emit inputError(QString("鼠标按下失败: %1").arg(mouseButtonToString(button)));
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pressedMouseButtons.append(static_cast<uint32_t>(button));
    }
    return true;
#else
    Q_UNUSED(button);
    return true;
#endif
}

bool WindowsInputSimulator::mouseUp(MouseButton button)
{
#ifdef Q_OS_WIN
    DWORD flag = 0;
    switch (button) {
    case MouseButton::Left:     flag = MOUSEEVENTF_LEFTUP; break;
    case MouseButton::Right:    flag = MOUSEEVENTF_RIGHTUP; break;
    case MouseButton::Middle:   flag = MOUSEEVENTF_MIDDLEUP; break;
    case MouseButton::XButton1: flag = MOUSEEVENTF_XUP; break;
    case MouseButton::XButton2: flag = MOUSEEVENTF_XUP; break;
    }

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flag;
    input.mi.dwExtraInfo = APP_EXTRA_INFO;
    if (button == MouseButton::XButton1)
        input.mi.mouseData = XBUTTON1;
    else if (button == MouseButton::XButton2)
        input.mi.mouseData = XBUTTON2;

    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        emit inputError(QString("鼠标释放失败: %1").arg(mouseButtonToString(button)));
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pressedMouseButtons.removeAll(static_cast<uint32_t>(button));
    }
    return true;
#else
    Q_UNUSED(button);
    return true;
#endif
}

bool WindowsInputSimulator::mouseClick(MouseButton button, int pressDurationMs)
{
    if (!mouseDown(button)) return false;
    QThread::msleep(static_cast<unsigned long>(pressDurationMs));
    return mouseUp(button);
}

bool WindowsInputSimulator::mouseDoubleClick(MouseButton button, int clickIntervalMs)
{
    if (!mouseDown(button)) return false;
    QThread::msleep(50);
    if (!mouseUp(button)) return false;
    QThread::msleep(static_cast<unsigned long>(clickIntervalMs));
    if (!mouseDown(button)) return false;
    QThread::msleep(50);
    return mouseUp(button);
}

bool WindowsInputSimulator::mouseWheel(int delta)
{
#ifdef Q_OS_WIN
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(delta);
    input.mi.dwExtraInfo = APP_EXTRA_INFO;

    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        emit inputError("滚轮操作失败");
        return false;
    }
    return true;
#else
    Q_UNUSED(delta);
    return true;
#endif
}

bool WindowsInputSimulator::mouseHorizontalWheel(int delta)
{
#ifdef Q_OS_WIN
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
    input.mi.mouseData = static_cast<DWORD>(delta);
    input.mi.dwExtraInfo = APP_EXTRA_INFO;

    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        emit inputError("水平滚轮操作失败");
        return false;
    }
    return true;
#else
    Q_UNUSED(delta);
    return true;
#endif
}

// ============================================================================
// 键盘操作
// ============================================================================
bool WindowsInputSimulator::keyDown(uint32_t winVk, bool isExtended)
{
#ifdef Q_OS_WIN
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(winVk);
    input.ki.dwFlags = 0;
    if (isExtended)
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    input.ki.dwExtraInfo = APP_EXTRA_INFO;

    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        emit inputError(QString("按键按下失败: VK_%1").arg(winVk, 2, 16, QChar('0')));
        return false;
    }

    trackKeyDown(winVk);
    return true;
#else
    Q_UNUSED(winVk); Q_UNUSED(isExtended);
    return true;
#endif
}

bool WindowsInputSimulator::keyUp(uint32_t winVk, bool isExtended)
{
#ifdef Q_OS_WIN
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(winVk);
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    if (isExtended)
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    input.ki.dwExtraInfo = APP_EXTRA_INFO;

    UINT result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) {
        emit inputError(QString("按键释放失败: VK_%1").arg(winVk, 2, 16, QChar('0')));
        return false;
    }

    trackKeyUp(winVk);
    return true;
#else
    Q_UNUSED(winVk); Q_UNUSED(isExtended);
    return true;
#endif
}

bool WindowsInputSimulator::keyPress(uint32_t winVk, int pressDurationMs, bool isExtended)
{
    if (!keyDown(winVk, isExtended)) return false;
    QThread::msleep(static_cast<unsigned long>(pressDurationMs));
    return keyUp(winVk, isExtended);
}

// ============================================================================
// 组合键: 1.按下修饰键 2.按下主键 3.释放主键 4.逆序释放修饰键
// ============================================================================
bool WindowsInputSimulator::sendCombo(uint32_t mainVk,
                                       bool ctrl, bool shift, bool alt, bool win,
                                       int pressDurationMs)
{
    // 1. 依次按下修饰键
    if (ctrl)  { if (!keyDown(VK_CONTROL)) return false; QThread::msleep(5); }
    if (shift) { if (!keyDown(VK_SHIFT))   return false; QThread::msleep(5); }
    if (alt)   { if (!keyDown(VK_MENU))    return false; QThread::msleep(5); }
    if (win)   { if (!keyDown(VK_LWIN))    return false; QThread::msleep(5); }

    // 2. 按下主键
    if (!keyDown(mainVk)) return false;
    QThread::msleep(static_cast<unsigned long>(pressDurationMs));

    // 3. 释放主键
    QThread::msleep(5);
    if (!keyUp(mainVk)) return false;

    // 4. 逆序释放修饰键
    if (win)   { QThread::msleep(5); keyUp(VK_LWIN); }
    if (alt)   { QThread::msleep(5); keyUp(VK_MENU); }
    if (shift) { QThread::msleep(5); keyUp(VK_SHIFT); }
    if (ctrl)  { QThread::msleep(5); keyUp(VK_CONTROL); }

    return true;
}

// ============================================================================
// 释放所有输入状态
// ============================================================================
void WindowsInputSimulator::releaseAllInputs()
{
#ifdef Q_OS_WIN
    // 释放所有记录的按键
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 释放键盘按键
        for (uint32_t vk : m_pressedKeys) {
            INPUT input = {};
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = static_cast<WORD>(vk);
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            input.ki.dwExtraInfo = APP_EXTRA_INFO;
            SendInput(1, &input, sizeof(INPUT));
        }
        m_pressedKeys.clear();

        // 释放鼠标按钮
        for (uint32_t btn : m_pressedMouseButtons) {
            DWORD flag = 0;
            DWORD data = 0;
            switch (static_cast<MouseButton>(btn)) {
            case MouseButton::Left:     flag = MOUSEEVENTF_LEFTUP; break;
            case MouseButton::Right:    flag = MOUSEEVENTF_RIGHTUP; break;
            case MouseButton::Middle:   flag = MOUSEEVENTF_MIDDLEUP; break;
            case MouseButton::XButton1: flag = MOUSEEVENTF_XUP; data = XBUTTON1; break;
            case MouseButton::XButton2: flag = MOUSEEVENTF_XUP; data = XBUTTON2; break;
            }
            INPUT input = {};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = flag;
            input.mi.mouseData = data;
            input.mi.dwExtraInfo = APP_EXTRA_INFO;
            SendInput(1, &input, sizeof(INPUT));
        }
        m_pressedMouseButtons.clear();
    }

    // 额外释放常见修饰键 (安全措施)
    keyUp(VK_CONTROL);
    keyUp(VK_SHIFT);
    keyUp(VK_MENU);
    keyUp(VK_LWIN);
    keyUp(VK_RWIN);
#endif
}

bool WindowsInputSimulator::hasPressedKeys() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_pressedKeys.isEmpty() || !m_pressedMouseButtons.isEmpty();
}

QVector<uint32_t> WindowsInputSimulator::pressedKeys() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pressedKeys;
}

void WindowsInputSimulator::trackKeyDown(uint32_t vk)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_pressedKeys.contains(vk))
        m_pressedKeys.append(vk);
}

void WindowsInputSimulator::trackKeyUp(uint32_t vk)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pressedKeys.removeAll(vk);
}

bool WindowsInputSimulator::sendInputEvent(uint32_t type, uint32_t data1, uint32_t data2,
                                            ULONG_PTR extraInfo)
{
#ifdef Q_OS_WIN
    INPUT input = {};
    if (type == INPUT_MOUSE) {
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>(data1);
        input.mi.dy = static_cast<LONG>(data2);
        input.mi.dwExtraInfo = extraInfo;
    } else {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = static_cast<WORD>(data1);
        input.ki.wScan = static_cast<WORD>(data2);
        input.ki.dwExtraInfo = extraInfo;
    }

    return SendInput(1, &input, sizeof(INPUT)) == 1;
#else
    Q_UNUSED(type); Q_UNUSED(data1); Q_UNUSED(data2); Q_UNUSED(extraInfo);
    return true;
#endif
}
