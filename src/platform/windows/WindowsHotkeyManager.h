#ifndef WINDOWSHOTKEYMANAGER_H
#define WINDOWSHOTKEYMANAGER_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QVector>

#include "AppTypes.h"

// ============================================================================
// Windows 全局热键管理器
// 使用 RegisterHotKey / UnregisterHotKey + WM_HOTKEY
// 通过 QAbstractNativeEventFilter 接收原生消息
// ============================================================================
class WindowsHotkeyManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit WindowsHotkeyManager(QObject* parent = nullptr);
    ~WindowsHotkeyManager() override;

    // 注册热键，返回热键ID (失败返回-1)
    int registerHotkey(const HotkeyInfo& hk);

    // 注销指定ID的热键
    bool unregisterHotkey(int id);

    // 注销所有热键
    void unregisterAll();

    // 检查热键是否已被注册
    bool isRegistered(int id) const;

    // 通过ID获取热键信息
    HotkeyInfo hotkeyInfo(int id) const;

    // Windows 原生事件过滤 (处理 WM_HOTKEY)
    bool nativeEventFilter(const QByteArray& eventType, void* message,
                           qintptr* result) override;

    // 生成下一个可用ID
    int nextId() { return m_nextId++; }

    // 将 HotkeyInfo 转换为 Windows MOD_xxx 组合
    static uint32_t hotkeyToNativeMod(const HotkeyInfo& hk);

signals:
    void hotkeyPressed(int id);
    void hotkeyRegisterFailed(int id, const QString& reason);
    void hotkeyConflict(int id1, int id2);

private:
#ifdef Q_OS_WIN
    void* m_hwnd = nullptr;  // 消息接收窗口句柄
#endif
    int m_nextId = 1;
    QHash<int, HotkeyInfo> m_registeredHotkeys;
};

#endif // WINDOWSHOTKEYMANAGER_H
