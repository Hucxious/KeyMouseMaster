#include "KeyCaptureEdit.h"
#include "utils/KeyMapper.h"
#include <QKeyEvent>
#include <QKeySequence>

KeyCaptureEdit::KeyCaptureEdit(QWidget* parent)
    : QLineEdit(parent)
{
    setReadOnly(true);
    setPlaceholderText("点击此处捕获按键...");
    setMinimumWidth(200);
    updateDisplay();
}

void KeyCaptureEdit::setKeyInfo(const KeyInfo& info)
{
    m_keyInfo = info;
    updateDisplay();
    emit keyInfoChanged(m_keyInfo);
}

void KeyCaptureEdit::clearKeyInfo()
{
    m_keyInfo = KeyInfo();
    updateDisplay();
    emit keyInfoChanged(m_keyInfo);
}

void KeyCaptureEdit::mousePressEvent(QMouseEvent* event)
{
    if (!m_capturing) {
        startCapture();
    }
    QLineEdit::mousePressEvent(event);
}

void KeyCaptureEdit::keyPressEvent(QKeyEvent* event)
{
    if (!m_capturing) {
        QLineEdit::keyPressEvent(event);
        return;
    }

    int key = event->key();

    // 忽略单独的修饰键
    if (KeyMapper::isModifierKey(key)) {
        // 但记录修饰键状态
        return;
    }

    // Escape清除
    if (key == Qt::Key_Escape) {
        clearKeyInfo();
        finishCapture();
        return;
    }

    // 构建KeyInfo
    m_keyInfo.qtKey = key;
    m_keyInfo.winVk = KeyMapper::qtKeyToWinVk(key);
    m_keyInfo.scanCode = KeyMapper::qtKeyToScanCode(key);

    // 修饰键
    Qt::KeyboardModifiers mods = event->modifiers();
    m_keyInfo.hasCtrl  = mods & Qt::ControlModifier;
    m_keyInfo.hasShift = mods & Qt::ShiftModifier;
    m_keyInfo.hasAlt   = mods & Qt::AltModifier;
    m_keyInfo.hasWin   = mods & Qt::MetaModifier;

    // 构建显示名称
    m_keyInfo.baseName = KeyMapper::qtKeyToDisplayName(key);

    QStringList parts;
    if (m_keyInfo.hasCtrl)  parts << "Ctrl";
    if (m_keyInfo.hasShift) parts << "Shift";
    if (m_keyInfo.hasAlt)   parts << "Alt";
    if (m_keyInfo.hasWin)   parts << "Win";
    parts << m_keyInfo.baseName;
    m_keyInfo.displayName = parts.join(" + ");

    updateDisplay();
    finishCapture();
    emit keyInfoChanged(m_keyInfo);
}

void KeyCaptureEdit::keyReleaseEvent(QKeyEvent* event)
{
    if (m_capturing) {
        Q_UNUSED(event)
        return;
    }
    QLineEdit::keyReleaseEvent(event);
}

void KeyCaptureEdit::focusOutEvent(QFocusEvent* event)
{
    if (m_capturing) {
        finishCapture();
    }
    QLineEdit::focusOutEvent(event);
}

void KeyCaptureEdit::startCapture()
{
    m_capturing = true;
    setText("... 按下目标按键 ...");
    setStyleSheet("QLineEdit { color: #2196F3; font-weight: bold; }");
    setFocus();
}

void KeyCaptureEdit::finishCapture()
{
    m_capturing = false;
    setStyleSheet("");
    updateDisplay();
}

void KeyCaptureEdit::updateDisplay()
{
    if (!m_keyInfo.isValid()) {
        setText("");
        setPlaceholderText("点击此处捕获按键...");
    } else {
        setText(m_keyInfo.displayName);
    }
}
