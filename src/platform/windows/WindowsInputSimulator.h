#ifndef WINDOWSINPUTSIMULATOR_H
#define WINDOWSINPUTSIMULATOR_H

#include <QObject>
#include <QPoint>
#include <QVector>
#include <cstdint>
#include <atomic>
#include <mutex>

#include "AppTypes.h"
#include "WinCompat.h"

// ============================================================================
// Windows 输入模拟器
// 统一封装 SendInput，支持鼠标移动/点击/滚轮、键盘按键/组合键
// 所有模拟事件使用固定 dwExtraInfo 标记，供录制钩子过滤
// ============================================================================
class WindowsInputSimulator : public QObject
{
    Q_OBJECT

public:
    explicit WindowsInputSimulator(QObject* parent = nullptr);
    ~WindowsInputSimulator() override;

    // 设置本软件输入标记 (用于钩子过滤)
    static constexpr ULONG_PTR APP_EXTRA_INFO = 0x4B4D4D00; // "KMM\0"

    // ---- 鼠标操作 ----
    // 移动鼠标到虚拟桌面绝对坐标 (使用 MOUSEEVENTF_VIRTUALDESK)
    bool mouseMoveAbsolute(int virtualDesktopX, int virtualDesktopY,
                           int virtualDesktopWidth, int virtualDesktopHeight);

    // 移动鼠标相对当前位置
    bool mouseMoveRelative(int deltaX, int deltaY);

    // 鼠标按钮操作
    bool mouseDown(MouseButton button);
    bool mouseUp(MouseButton button);
    bool mouseClick(MouseButton button, int pressDurationMs = 50);
    bool mouseDoubleClick(MouseButton button, int clickIntervalMs = 100);

    // 滚轮
    bool mouseWheel(int delta);        // 正值向上，负值向下 (WHEEL_DELTA=120)
    bool mouseHorizontalWheel(int delta);

    // ---- 键盘操作 ----
    bool keyDown(uint32_t winVk, bool isExtended = false);
    bool keyUp(uint32_t winVk, bool isExtended = false);
    bool keyPress(uint32_t winVk, int pressDurationMs = 50, bool isExtended = false);

    // 组合键: 依次按下修饰键 -> 按主键 -> 逆序释放
    bool sendCombo(uint32_t mainVk,
                   bool ctrl, bool shift, bool alt, bool win,
                   int pressDurationMs = 50);

    // ---- 状态管理 ----
    // 释放所有可能被本程序按下的键鼠状态
    void releaseAllInputs();

    // 检查是否有按键处于按下状态
    bool hasPressedKeys() const;

    // 获取当前记录的按下按键列表
    QVector<uint32_t> pressedKeys() const;

signals:
    void inputError(const QString& message);

private:
    // 内部使用的按下状态追踪
    void trackKeyDown(uint32_t vk);
    void trackKeyUp(uint32_t vk);
    bool sendInputEvent(uint32_t type, uint32_t data1, uint32_t data2,
                        ULONG_PTR extraInfo = APP_EXTRA_INFO);

    QVector<uint32_t> m_pressedKeys;
    QVector<uint32_t> m_pressedMouseButtons;
    mutable std::mutex m_mutex;
};

#endif // WINDOWSINPUTSIMULATOR_H
