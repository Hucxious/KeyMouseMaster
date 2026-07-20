#ifndef KEYMAPPER_H
#define KEYMAPPER_H

#include <QString>
#include <QPair>
#include <QVector>
#include <cstdint>

// ============================================================================
// 按键映射工具
// Qt键值 <-> Windows虚拟键码 <-> 可读名称
// ============================================================================
class KeyMapper
{
public:
    // 键描述结构
    struct KeyEntry {
        int      qtKey;
        uint32_t winVk;
        QString  displayName;
        bool     isModifier;
        bool     isExtended;
    };

    // 获取所有已知按键
    static const QVector<KeyEntry>& allKeys();

    // Qt键值 -> Windows VK
    static uint32_t qtKeyToWinVk(int qtKey);
    static uint32_t qtKeyToScanCode(int qtKey);

    // Windows VK -> 显示名称
    static QString winVkToDisplayName(uint32_t vk, bool isExtended = false);

    // Qt键值 -> 显示名称
    static QString qtKeyToDisplayName(int qtKey);

    // 通过显示名称查找
    static int displayNameToQtKey(const QString& name);

    // 检查是否为修饰键
    static bool isModifierKey(int qtKey);

    // 获取所有可录制按键 (用于下拉列表)
    static QVector<QPair<QString, int>> recordableKeys();

    // 扫描码 -> Qt键值
    static int scanCodeToQtKey(uint32_t scanCode, bool isExtended);

private:
    static void initKeyTable();
    static bool s_initialized;
    static QVector<KeyEntry> s_keys;
};

#endif // KEYMAPPER_H
