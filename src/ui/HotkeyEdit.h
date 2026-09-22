#ifndef HOTKEYEDIT_H
#define HOTKEYEDIT_H

#include <QLineEdit>
#include <QKeySequence>
#include <functional>
#include "AppTypes.h"

// ============================================================================
// 热键输入控件
// 用户点击后进入捕获模式，按下组合键后显示并保存
// ============================================================================
class HotkeyEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit HotkeyEdit(QWidget* parent = nullptr);
    ~HotkeyEdit() override;

    // 设置/获取热键
    void setHotkey(const HotkeyInfo& hk);
    HotkeyInfo hotkey() const { return m_hotkey; }

    // 清除
    void clearHotkey();

    // 是否正在捕获
    bool isCapturing() const { return m_capturing; }

    // 全局检测：是否有任何 HotkeyEdit 正在捕获按键
    static bool isAnyCapturing() { return s_capturingCount > 0; }

    // 设置捕获状态变化回调 (用于临时注销/恢复全局热键)
    // callback(true) = 捕获开始, callback(false) = 捕获结束
    static void notifyCapture(bool active);
    static void setCaptureStateCallback(std::function<void(bool)> callback);

signals:
    void hotkeyChanged(const HotkeyInfo& hk);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void startCapture();
    void finishCapture();
    void updateDisplay();

    HotkeyInfo m_hotkey;
    bool m_capturing = false;

    static int s_capturingCount;  // 全局捕获计数器
    static std::function<void(bool)> s_captureStateCallback;
};

#endif // HOTKEYEDIT_H
