#include "InputStateManager.h"
#include "Logger.h"

InputStateManager::InputStateManager(QObject* parent)
    : QObject(parent)
{
}

void InputStateManager::trackKeyPress(uint32_t winVk)
{
    QMutexLocker locker(&m_mutex);
    if (!m_pressedKeys.contains(winVk))
        m_pressedKeys.append(winVk);
}

void InputStateManager::trackKeyRelease(uint32_t winVk)
{
    QMutexLocker locker(&m_mutex);
    m_pressedKeys.removeAll(winVk);
}

void InputStateManager::trackMousePress(uint32_t button)
{
    QMutexLocker locker(&m_mutex);
    if (!m_pressedMouseButtons.contains(button))
        m_pressedMouseButtons.append(button);
}

void InputStateManager::trackMouseRelease(uint32_t button)
{
    QMutexLocker locker(&m_mutex);
    m_pressedMouseButtons.removeAll(button);
}

bool InputStateManager::hasPressedInputs() const
{
    QMutexLocker locker(&m_mutex);
    return !m_pressedKeys.isEmpty() || !m_pressedMouseButtons.isEmpty();
}

QVector<uint32_t> InputStateManager::pressedKeys() const
{
    QMutexLocker locker(&m_mutex);
    return m_pressedKeys;
}

QVector<uint32_t> InputStateManager::pressedMouseButtons() const
{
    QMutexLocker locker(&m_mutex);
    return m_pressedMouseButtons;
}

void InputStateManager::clearAll()
{
    QMutexLocker locker(&m_mutex);
    m_pressedKeys.clear();
    m_pressedMouseButtons.clear();
}

InputStateManager::InputResetList InputStateManager::getResetList()
{
    QMutexLocker locker(&m_mutex);
    InputResetList list;
    list.keys = m_pressedKeys;
    list.mouseButtons = m_pressedMouseButtons;
    m_pressedKeys.clear();
    m_pressedMouseButtons.clear();
    return list;
}
