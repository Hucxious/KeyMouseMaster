#ifndef INPUTSTATEMANAGER_H
#define INPUTSTATEMANAGER_H

#include <QObject>
#include <QVector>
#include <QMutex>
#include <cstdint>

// ============================================================================
// 输入状态管理器
// 追踪所有被程序按下的键盘按键和鼠标按钮，确保退出时能够完全释放
// ============================================================================
class InputStateManager : public QObject
{
    Q_OBJECT

public:
    explicit InputStateManager(QObject* parent = nullptr);

    // 记录/清除按键状态
    void trackKeyPress(uint32_t winVk);
    void trackKeyRelease(uint32_t winVk);
    void trackMousePress(uint32_t button);
    void trackMouseRelease(uint32_t button);

    // 查询
    bool hasPressedInputs() const;
    QVector<uint32_t> pressedKeys() const;
    QVector<uint32_t> pressedMouseButtons() const;

    // 清除所有状态
    void clearAll();

    // 紧急复位: 返回所有需要释放的按键/按钮列表
    struct InputResetList {
        QVector<uint32_t> keys;
        QVector<uint32_t> mouseButtons;
    };
    InputResetList getResetList();

signals:
    void inputsNeedRelease(const QVector<uint32_t>& keys,
                           const QVector<uint32_t>& mouseButtons);

private:
    mutable QMutex m_mutex;
    QVector<uint32_t> m_pressedKeys;
    QVector<uint32_t> m_pressedMouseButtons;
};

#endif // INPUTSTATEMANAGER_H
