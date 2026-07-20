#ifndef SCRIPTEVENT_H
#define SCRIPTEVENT_H

#include "AppTypes.h"
#include <QString>
#include <QPoint>
#include <QPointF>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdint>

// ============================================================================
// 单个脚本事件
// 包含鼠标/键盘事件的完整信息，支持多显示器坐标
// ============================================================================
struct ScriptEvent
{
    // 基本信息
    ScriptEventType type = ScriptEventType::MouseMove;
    int64_t  timestampMs = 0;        // 相对录制开始时刻 (毫秒)
    int      eventIndex = 0;         // 事件序号
    bool     enabled = true;         // 是否在回放时执行

    // 鼠标相关
    MouseButton mouseButton = MouseButton::Left;
    QPoint   virtualDesktopPos;      // 虚拟桌面绝对坐标 (可能为负)
    QString  monitorDeviceName;      // 目标显示器设备名
    QPoint   monitorInternalPos;     // 显示器内部坐标
    QPointF  monitorRatioPos;        // 显示器比例坐标 [0,1]
    int      wheelDelta = 0;         // 滚轮增量 (WHEEL_DELTA = 120)

    // 键盘相关
    uint32_t winVk = 0;              // Windows虚拟键码
    uint32_t scanCode = 0;           // 扫描码
    bool     isExtendedKey = false;
    bool     hasCtrl  = false;
    bool     hasShift = false;
    bool     hasAlt   = false;
    bool     hasWin   = false;
    QString  keyDisplayName;         // 可读名称

    // 连带数据: 按下事件的修饰键状态 (用于回放时还原)
    uint32_t modifiersAtPress = 0;

    // 序列化
    QJsonObject toJson() const;
    static ScriptEvent fromJson(const QJsonObject& obj, bool* ok = nullptr);

    // 验证
    bool isValid() const;
    QString validationError() const;

    // 工具方法
    bool isMouseEvent() const;
    bool isKeyboardEvent() const;
    QString eventSummary() const;
};

// ============================================================================
// 录制设置
// ============================================================================
struct RecordingSettings
{
    bool recordMouseMove     = true;
    bool recordMouseClick    = true;
    bool recordWheel         = true;
    bool recordKeyboard      = true;
    int  mouseMoveMinIntervalMs = 10;   // 鼠标移动最小记录间隔
    int  mouseMoveMinDistance    = 3;    // 鼠标移动最小记录像素
    bool ignoreOwnWindow     = true;     // 忽略本软件窗口
    bool ignoreSimulated     = true;     // 忽略本软件模拟的输入
    HotkeyInfo startHotkey;
    HotkeyInfo stopHotkey;
};

// ============================================================================
// 回放设置
// ============================================================================
struct PlaybackSettings
{
    int     startDelayMs = 0;
    int     repeatCount = 1;
    bool    infiniteRepeat = false;
    int     roundIntervalMs = 1000;       // 每轮间隔
    double  speedFactor = 1.0;
    bool    restoreCursor = true;
    bool    skipDisabledEvents = true;
    CoordinateMode coordinateMode = CoordinateMode::MonitorRelative;
    HotkeyInfo startHotkey;
    HotkeyInfo stopHotkey;
};

#endif // SCRIPTEVENT_H
