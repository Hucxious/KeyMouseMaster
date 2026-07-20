#ifndef APPTYPES_H
#define APPTYPES_H

#include <QString>
#include <QMetaType>
#include <cstdint>

// ============================================================================
// 统一任务状态枚举
// ============================================================================
enum class TaskState
{
    Idle,
    Preparing,
    MouseClicking,
    KeyboardClicking,
    Recording,
    Playing,
    Paused,
    Stopping,
    Completed,
    Error
};

inline const char* taskStateToString(TaskState state)
{
    switch (state) {
    case TaskState::Idle:            return "空闲";
    case TaskState::Preparing:       return "准备中";
    case TaskState::MouseClicking:   return "鼠标连点中";
    case TaskState::KeyboardClicking:return "键盘连点中";
    case TaskState::Recording:       return "录制中";
    case TaskState::Playing:         return "回放中";
    case TaskState::Paused:          return "已暂停";
    case TaskState::Stopping:        return "停止中";
    case TaskState::Completed:       return "已完成";
    case TaskState::Error:           return "错误";
    }
    return "未知";
}

// ============================================================================
// 鼠标按键枚举
// ============================================================================
enum class MouseButton
{
    Left,
    Right,
    Middle,
    XButton1,
    XButton2
};

inline const char* mouseButtonToString(MouseButton btn)
{
    switch (btn) {
    case MouseButton::Left:     return "左键";
    case MouseButton::Right:    return "右键";
    case MouseButton::Middle:   return "中键";
    case MouseButton::XButton1: return "侧键1";
    case MouseButton::XButton2: return "侧键2";
    }
    return "未知";
}

// ============================================================================
// 点击模式枚举
// ============================================================================
enum class ClickMode
{
    Single,
    Double,
    PressOnly,
    ReleaseOnly,
    Hold
};

inline const char* clickModeToString(ClickMode mode)
{
    switch (mode) {
    case ClickMode::Single:      return "单击";
    case ClickMode::Double:      return "双击";
    case ClickMode::PressOnly:   return "仅按下";
    case ClickMode::ReleaseOnly: return "仅释放";
    case ClickMode::Hold:        return "长按";
    }
    return "未知";
}

// ============================================================================
// 键盘输入模式枚举
// ============================================================================
enum class KeyInputMode
{
    Normal,
    Combo,
    PressOnly,
    ReleaseOnly,
    FullKey,
    Hold
};

inline const char* keyInputModeToString(KeyInputMode mode)
{
    switch (mode) {
    case KeyInputMode::Normal:      return "普通单键";
    case KeyInputMode::Combo:       return "组合键";
    case KeyInputMode::PressOnly:   return "仅按下";
    case KeyInputMode::ReleaseOnly: return "仅释放";
    case KeyInputMode::FullKey:     return "完整按键";
    case KeyInputMode::Hold:        return "长按";
    }
    return "未知";
}

// ============================================================================
// 坐标模式枚举
// ============================================================================
enum class CoordinateMode
{
    CurrentCursor,         // 当前鼠标位置
    VirtualDesktopAbsolute,// 虚拟桌面绝对坐标
    MonitorRelative,       // 显示器相对坐标
    MonitorRatio           // 显示器比例坐标
};

inline const char* coordinateModeToString(CoordinateMode mode)
{
    switch (mode) {
    case CoordinateMode::CurrentCursor:          return "当前光标";
    case CoordinateMode::VirtualDesktopAbsolute: return "虚拟桌面绝对";
    case CoordinateMode::MonitorRelative:        return "显示器相对";
    case CoordinateMode::MonitorRatio:           return "显示器比例";
    }
    return "未知";
}

// ============================================================================
// 脚本事件类型枚举
// ============================================================================
enum class ScriptEventType : uint8_t
{
    MouseMove,
    MouseDown,
    MouseUp,
    MouseWheel,
    MouseHWheel,
    KeyDown,
    KeyUp
};

inline const char* scriptEventTypeToString(ScriptEventType t)
{
    switch (t) {
    case ScriptEventType::MouseMove:  return "鼠标移动";
    case ScriptEventType::MouseDown:  return "鼠标按下";
    case ScriptEventType::MouseUp:    return "鼠标释放";
    case ScriptEventType::MouseWheel: return "滚轮";
    case ScriptEventType::MouseHWheel:return "水平滚轮";
    case ScriptEventType::KeyDown:    return "按键按下";
    case ScriptEventType::KeyUp:      return "按键释放";
    }
    return "未知";
}

// ============================================================================
// 键盘按键信息结构
// ============================================================================
struct KeyInfo
{
    int      qtKey = 0;            // Qt::Key 值
    uint32_t winVk = 0;           // Windows 虚拟键码
    uint32_t scanCode = 0;        // 扫描码
    bool     isExtended = false;  // 扩展键标志
    QString  displayName;          // 可读名称，如 "Ctrl + Shift + S"
    QString  baseName;             // 基础按键名，如 "S"
    bool     hasCtrl  = false;
    bool     hasShift = false;
    bool     hasAlt   = false;
    bool     hasWin   = false;

    bool isValid() const { return qtKey != 0 || winVk != 0; }
    bool isModifier() const;
};

// ============================================================================
// 热键信息结构
// ============================================================================
struct HotkeyInfo
{
    int  key = 0;           // Qt::Key
    bool ctrl  = false;
    bool shift = false;
    bool alt   = false;
    bool win   = false;
    int  nativeMod = 0;
    int  id = -1;

    bool isValid() const { return key != 0; }
    QString toString() const;
};

// ============================================================================
// 时间间隔单位
// ============================================================================
enum class TimeUnit
{
    Milliseconds,
    Seconds
};

// ============================================================================
// 托盘关闭行为
// ============================================================================
enum class TrayCloseBehavior
{
    Exit,
    MinimizeToTray,
    AskUser
};

// ============================================================================
// 常量定义
// ============================================================================
namespace AppConstants
{
    // Windows 绝对坐标范围 (MOUSEEVENTF_ABSOLUTE)
    constexpr int WIN_ABSOLUTE_MIN = 0;
    constexpr int WIN_ABSOLUTE_MAX = 65535;

    // 参数范围
    constexpr int MIN_CLICK_INTERVAL_MS = 1;
    constexpr int MAX_CLICK_INTERVAL_MS = 3600000; // 1 hour
    constexpr int MIN_PRESS_DURATION_MS = 1;
    constexpr int MAX_PRESS_DURATION_MS = 60000;
    constexpr int MIN_REPEAT_COUNT = 1;
    constexpr int MAX_REPEAT_COUNT = 999999;
    constexpr int MIN_START_DELAY_MS = 0;
    constexpr int MAX_START_DELAY_MS = 60000;

    // 脚本
    constexpr int MAX_SCRIPT_EVENTS = 100000;
    constexpr const char* SCRIPT_FORMAT = "KeyMouseScript";
    constexpr int SCRIPT_VERSION = 1;
    constexpr const char* SCRIPT_EXTENSION = "kms";

    // 日志
    constexpr qint64 MAX_LOG_SIZE = 10 * 1024 * 1024; // 10 MB

    // 播放速度选项
    constexpr double PLAYBACK_SPEEDS[] = {0.5, 0.75, 1.0, 1.5, 2.0};
}

// 注册到Qt元对象系统
Q_DECLARE_METATYPE(TaskState)

#endif // APPTYPES_H
