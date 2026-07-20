#include "WindowsHotkeyManager.h"
#include "Logger.h"
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winuser.h>
#endif

WindowsHotkeyManager::WindowsHotkeyManager(QObject* parent)
    : QObject(parent)
{
}

WindowsHotkeyManager::~WindowsHotkeyManager()
{
    unregisterAll();
}

int WindowsHotkeyManager::registerHotkey(const HotkeyInfo& hk)
{
    if (!hk.isValid()) {
        emit hotkeyRegisterFailed(-1, "快捷键无效");
        return -1;
    }

#ifdef Q_OS_WIN
    int id = nextId();
    uint32_t mod = hotkeyToNativeMod(hk);
    uint32_t vk = static_cast<uint32_t>(hk.key);

    // 检查是否与已注册热键冲突
    for (auto it = m_registeredHotkeys.begin(); it != m_registeredHotkeys.end(); ++it) {
        if (it.value().key == hk.key
            && it.value().ctrl == hk.ctrl
            && it.value().shift == hk.shift
            && it.value().alt == hk.alt
            && it.value().win == hk.win) {
            emit hotkeyConflict(id, it.key());
            emit hotkeyRegisterFailed(id, "快捷键已被其他功能占用");
            return -1;
        }
    }

    BOOL result = RegisterHotKey(nullptr, id, mod, vk);
    if (!result) {
        DWORD err = GetLastError();
        QString reason;
        if (err == ERROR_HOTKEY_ALREADY_REGISTERED) {
            reason = "快捷键已被其他程序注册";
        } else if (err == ERROR_ACCESS_DENIED) {
            reason = "权限不足";
        } else {
            reason = QString("注册失败，错误码: %1").arg(err);
        }
        LOG_ERROR(QString("注册热键失败: %1 (id=%2, err=%3)")
            .arg(hk.toString()).arg(id).arg(err));
        emit hotkeyRegisterFailed(id, reason);
        return -1;
    }

    HotkeyInfo registered = hk;
    registered.id = id;
    m_registeredHotkeys.insert(id, registered);

    LOG_INFO(QString("热键注册成功: %1 (id=%2)").arg(hk.toString()).arg(id));
    return id;
#else
    Q_UNUSED(hk);
    return -1;
#endif
}

bool WindowsHotkeyManager::unregisterHotkey(int id)
{
#ifdef Q_OS_WIN
    if (!m_registeredHotkeys.contains(id))
        return false;

    UnregisterHotKey(nullptr, id);
    m_registeredHotkeys.remove(id);
    LOG_INFO(QString("热键已注销 (id=%1)").arg(id));
    return true;
#else
    Q_UNUSED(id);
    return false;
#endif
}

void WindowsHotkeyManager::unregisterAll()
{
#ifdef Q_OS_WIN
    for (auto it = m_registeredHotkeys.begin(); it != m_registeredHotkeys.end(); ++it) {
        UnregisterHotKey(nullptr, it.key());
    }
    m_registeredHotkeys.clear();
    LOG_INFO("所有热键已注销");
#endif
}

bool WindowsHotkeyManager::isRegistered(int id) const
{
    return m_registeredHotkeys.contains(id);
}

HotkeyInfo WindowsHotkeyManager::hotkeyInfo(int id) const
{
    return m_registeredHotkeys.value(id);
}

bool WindowsHotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message,
                                              qintptr* result)
{
    Q_UNUSED(eventType)

#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        int id = static_cast<int>(msg->wParam);
        if (m_registeredHotkeys.contains(id)) {
            emit hotkeyPressed(id);
            if (result) *result = 0;
            return true;  // 事件已处理
        }
    }
#else
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}

uint32_t WindowsHotkeyManager::hotkeyToNativeMod(const HotkeyInfo& hk)
{
    uint32_t mod = 0;
#ifdef Q_OS_WIN
    if (hk.ctrl)  mod |= MOD_CONTROL;
    if (hk.shift) mod |= MOD_SHIFT;
    if (hk.alt)   mod |= MOD_ALT;
    if (hk.win)   mod |= MOD_WIN;
#endif
    return mod;
}
