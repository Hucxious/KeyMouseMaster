#ifndef KEYCAPTUREEDIT_H
#define KEYCAPTUREEDIT_H

#include <QLineEdit>
#include "AppTypes.h"

// ============================================================================
// 按键捕获控件
// 用户点击后进入捕获模式，按下按键后显示组合键名称
// 用于键盘连点页面的目标按键选择
// ============================================================================
class KeyCaptureEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit KeyCaptureEdit(QWidget* parent = nullptr);

    void setKeyInfo(const KeyInfo& info);
    KeyInfo keyInfo() const { return m_keyInfo; }
    void clearKeyInfo();

    bool isCapturing() const { return m_capturing; }

signals:
    void keyInfoChanged(const KeyInfo& info);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    void startCapture();
    void finishCapture();
    void updateDisplay();

    KeyInfo m_keyInfo;
    bool m_capturing = false;
};

#endif // KEYCAPTUREEDIT_H
