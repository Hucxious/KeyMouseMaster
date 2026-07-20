#include "KeyMapper.h"
#include <QKeySequence>
#include <Qt>

bool KeyMapper::s_initialized = false;
QVector<KeyMapper::KeyEntry> KeyMapper::s_keys;

void KeyMapper::initKeyTable()
{
    if (s_initialized) return;
    s_initialized = true;

    // 字母键 A-Z
    for (int k = Qt::Key_A; k <= Qt::Key_Z; ++k) {
        char letter = 'A' + (k - Qt::Key_A);
        s_keys.append({k, static_cast<uint32_t>(letter),
                       QString(QChar(letter)), false, false});
    }

    // 数字键 0-9
    for (int k = Qt::Key_0; k <= Qt::Key_9; ++k) {
        char digit = '0' + (k - Qt::Key_0);
        s_keys.append({k, static_cast<uint32_t>(digit),
                       QString(QChar(digit)), false, false});
    }

    // F1-F24
    for (int i = 1; i <= 24; ++i) {
        int qtKey = Qt::Key_F1 + (i - 1);
        uint32_t vk = 0x70 + (i - 1); // VK_F1 = 0x70
        s_keys.append({qtKey, vk, QString("F%1").arg(i), false, false});
    }

    // 小键盘数字
    // 小键盘: Qt 6.9 使用 operator| 替代弃用的 operator+
    s_keys.append({static_cast<int>(Qt::Key_0) | static_cast<int>(Qt::KeypadModifier), 0x60, QStringLiteral("Num 0"), false, true});

    // 方向键
    s_keys.append({Qt::Key_Left,  0x25, QStringLiteral("←"), false, true});
    s_keys.append({Qt::Key_Right, 0x27, QStringLiteral("→"), false, true});
    s_keys.append({Qt::Key_Up,    0x26, QStringLiteral("↑"), false, true});
    s_keys.append({Qt::Key_Down,  0x28, QStringLiteral("↓"), false, true});

    // 功能键
    s_keys.append({Qt::Key_Space,     0x20, QStringLiteral("空格"), false, false});
    s_keys.append({Qt::Key_Return,    0x0D, QStringLiteral("回车"), false, false});
    s_keys.append({Qt::Key_Enter,     0x0D, QStringLiteral("小键盘回车"), false, true});
    s_keys.append({Qt::Key_Tab,       0x09, QStringLiteral("Tab"), false, false});
    s_keys.append({Qt::Key_Escape,    0x1B, QStringLiteral("Esc"), false, false});
    s_keys.append({Qt::Key_Backspace, 0x08, QStringLiteral("Backspace"), false, false});
    s_keys.append({Qt::Key_Delete,    0x2E, QStringLiteral("Delete"), false, true});
    s_keys.append({Qt::Key_Insert,    0x2D, QStringLiteral("Insert"), false, true});
    s_keys.append({Qt::Key_Home,      0x24, QStringLiteral("Home"), false, true});
    s_keys.append({Qt::Key_End,       0x23, QStringLiteral("End"), false, true});
    s_keys.append({Qt::Key_PageUp,    0x21, QStringLiteral("PageUp"), false, true});
    s_keys.append({Qt::Key_PageDown,  0x22, QStringLiteral("PageDown"), false, true});

    // 修饰键
    s_keys.append({Qt::Key_Control, 0x11, QStringLiteral("Ctrl"), true, false});
    s_keys.append({Qt::Key_Shift,   0x10, QStringLiteral("Shift"), true, false});
    s_keys.append({Qt::Key_Alt,     0x12, QStringLiteral("Alt"), true, false});
    s_keys.append({Qt::Key_Meta,    0x5B, QStringLiteral("Win"), true, false});

    // 标点符号
    s_keys.append({Qt::Key_Comma,     0xBC, QStringLiteral(","), false, false});
    s_keys.append({Qt::Key_Period,    0xBE, QStringLiteral("."), false, false});
    s_keys.append({Qt::Key_Slash,     0xBF, QStringLiteral("/"), false, false});
    s_keys.append({Qt::Key_Semicolon, 0xBA, QStringLiteral(";"), false, false});
    s_keys.append({Qt::Key_Apostrophe,0xDE, QStringLiteral("'"), false, false});
    s_keys.append({Qt::Key_BracketLeft,  0xDB, QStringLiteral("["), false, false});
    s_keys.append({Qt::Key_BracketRight, 0xDD, QStringLiteral("]"), false, false});
    s_keys.append({Qt::Key_Backslash,    0xDC, QStringLiteral("\\"), false, false});
    s_keys.append({Qt::Key_Minus,   0xBD, QStringLiteral("-"), false, false});
    s_keys.append({Qt::Key_Equal,   0xBB, QStringLiteral("="), false, false});
    s_keys.append({Qt::Key_QuoteLeft, 0xC0, QStringLiteral("`"), false, false});

    // Print Screen, Scroll Lock, Pause
    s_keys.append({Qt::Key_Print,    0x2C, QStringLiteral("PrintScreen"), false, true});
    s_keys.append({Qt::Key_ScrollLock, 0x91, QStringLiteral("ScrollLock"), false, false});
    s_keys.append({Qt::Key_Pause,    0x13, QStringLiteral("Pause"), false, false});

    // Caps Lock, Num Lock
    s_keys.append({Qt::Key_CapsLock, 0x14, QStringLiteral("CapsLock"), false, false});
    s_keys.append({Qt::Key_NumLock,  0x90, QStringLiteral("NumLock"), false, true});

    // Menu/Apps key
    s_keys.append({Qt::Key_Menu, 0x5D, QStringLiteral("Menu"), false, true});
}

const QVector<KeyMapper::KeyEntry>& KeyMapper::allKeys()
{
    initKeyTable();
    return s_keys;
}

uint32_t KeyMapper::qtKeyToWinVk(int qtKey)
{
    initKeyTable();
    for (const auto& entry : s_keys) {
        if (entry.qtKey == qtKey)
            return entry.winVk;
    }
    return 0;
}

uint32_t KeyMapper::qtKeyToScanCode(int qtKey)
{
    // 简化实现：使用 MapVirtualKey 需要在 Windows 上
    // 这里提供一个基础映射
    uint32_t vk = qtKeyToWinVk(qtKey);
    if (vk == 0) return 0;

    // 常见扫描码映射
    if (vk >= 'A' && vk <= 'Z') return vk - 'A' + 0x1E;
    if (vk >= '0' && vk <= '9') return vk - '0' + 0x0B;
    // 对于精确扫描码，需要 Windows API
    return 0;
}

QString KeyMapper::winVkToDisplayName(uint32_t vk, bool isExtended)
{
    Q_UNUSED(isExtended)
    initKeyTable();
    for (const auto& entry : s_keys) {
        if (entry.winVk == vk)
            return entry.displayName;
    }
    return QString("VK_%1").arg(vk, 2, 16, QChar('0'));
}

QString KeyMapper::qtKeyToDisplayName(int qtKey)
{
    initKeyTable();
    for (const auto& entry : s_keys) {
        if (entry.qtKey == qtKey)
            return entry.displayName;
    }
    return QKeySequence(qtKey).toString();
}

int KeyMapper::displayNameToQtKey(const QString& name)
{
    initKeyTable();
    for (const auto& entry : s_keys) {
        if (entry.displayName == name)
            return entry.qtKey;
    }
    return 0;
}

bool KeyMapper::isModifierKey(int qtKey)
{
    return qtKey == Qt::Key_Control || qtKey == Qt::Key_Shift
        || qtKey == Qt::Key_Alt || qtKey == Qt::Key_Meta
        || qtKey == Qt::Key_AltGr;
}

QVector<QPair<QString, int>> KeyMapper::recordableKeys()
{
    initKeyTable();
    QVector<QPair<QString, int>> result;
    for (const auto& entry : s_keys) {
        if (!entry.isModifier)
            result.append({entry.displayName, entry.qtKey});
    }
    return result;
}

int KeyMapper::scanCodeToQtKey(uint32_t scanCode, bool isExtended)
{
    Q_UNUSED(isExtended)
    initKeyTable();
    // 扫描码到Qt键值的映射需要Windows API辅助
    // 这里提供基础映射
    for (const auto& entry : s_keys) {
        uint32_t entrySc = qtKeyToScanCode(entry.qtKey);
        if (entrySc == scanCode)
            return entry.qtKey;
    }
    return 0;
}
