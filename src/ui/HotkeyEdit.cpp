#include "HotkeyEdit.h"
#include <QKeyEvent>
#include <QFocusEvent>

HotkeyEdit::HotkeyEdit(QWidget* parent)
    : QLineEdit(parent)
{
    setReadOnly(true);
    setPlaceholderText("点击此处设置快捷键...");
    setMinimumWidth(180);
    updateDisplay();
}

void HotkeyEdit::setHotkey(const HotkeyInfo& hk)
{
    m_hotkey = hk;
    updateDisplay();
    emit hotkeyChanged(m_hotkey);
}

void HotkeyEdit::clearHotkey()
{
    m_hotkey = HotkeyInfo();
    updateDisplay();
    emit hotkeyChanged(m_hotkey);
}

void HotkeyEdit::mousePressEvent(QMouseEvent* event)
{
    if (!m_capturing) {
        startCapture();
    }
    QLineEdit::mousePressEvent(event);
}

void HotkeyEdit::keyPressEvent(QKeyEvent* event)
{
    if (!m_capturing) {
        QLineEdit::keyPressEvent(event);
        return;
    }

    int key = event->key();

    // 忽略单独的修饰键
    if (key == Qt::Key_Control || key == Qt::Key_Shift
        || key == Qt::Key_Alt || key == Qt::Key_Meta
        || key == Qt::Key_AltGr) {
        return;
    }

    // 捕获组合键
    m_hotkey.key = key;
    m_hotkey.ctrl  = event->modifiers() & Qt::ControlModifier;
    m_hotkey.shift = event->modifiers() & Qt::ShiftModifier;
    m_hotkey.alt   = event->modifiers() & Qt::AltModifier;
    m_hotkey.win   = event->modifiers() & Qt::MetaModifier;

    // 如果按下了Escape，清除
    if (key == Qt::Key_Escape) {
        clearHotkey();
        finishCapture();
        return;
    }

    updateDisplay();
    finishCapture();
    emit hotkeyChanged(m_hotkey);
}

void HotkeyEdit::keyReleaseEvent(QKeyEvent* event)
{
    if (m_capturing) {
        Q_UNUSED(event)
        // 等待keyPressEvent处理
        return;
    }
    QLineEdit::keyReleaseEvent(event);
}

void HotkeyEdit::focusOutEvent(QFocusEvent* event)
{
    if (m_capturing) {
        finishCapture();
    }
    QLineEdit::focusOutEvent(event);
}

void HotkeyEdit::startCapture()
{
    m_capturing = true;
    setText("... 按下快捷键 ...");
    setStyleSheet("QLineEdit { color: #2196F3; font-weight: bold; }");
    setFocus();
}

void HotkeyEdit::finishCapture()
{
    m_capturing = false;
    setStyleSheet("");
    updateDisplay();
}

void HotkeyEdit::updateDisplay()
{
    if (!m_hotkey.isValid()) {
        setText("");
        setPlaceholderText("点击此处设置快捷键...");
    } else {
        setText(m_hotkey.toString());

        // 构建编码显示
        QStringList parts;
        if (m_hotkey.ctrl)  parts << "Ctrl";
        if (m_hotkey.shift) parts << "Shift";
        if (m_hotkey.alt)   parts << "Alt";
        if (m_hotkey.win)   parts << "Win";

        QKeySequence ks(static_cast<Qt::Key>(m_hotkey.key));
        parts << ks.toString();

        setText(parts.join(" + "));
    }
}
