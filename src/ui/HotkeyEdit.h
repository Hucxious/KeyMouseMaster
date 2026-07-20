#ifndef HOTKEYEDIT_H
#define HOTKEYEDIT_H

#include <QLineEdit>
#include <QKeySequence>
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

    // 设置/获取热键
    void setHotkey(const HotkeyInfo& hk);
    HotkeyInfo hotkey() const { return m_hotkey; }

    // 清除
    void clearHotkey();

    // 是否正在捕获
    bool isCapturing() const { return m_capturing; }

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
};

#endif // HOTKEYEDIT_H
