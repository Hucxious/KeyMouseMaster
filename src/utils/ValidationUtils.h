#ifndef VALIDATIONUTILS_H
#define VALIDATIONUTILS_H

#include <QString>

// ============================================================================
// 参数校验工具
// ============================================================================
namespace ValidationUtils
{
    // 校验整数值在 [min, max] 范围内
    bool validateIntRange(int value, int min, int max, QString* errorMsg = nullptr,
                          const QString& fieldName = QString());

    // 校验点击间隔 (考虑单位)
    bool validateClickInterval(int value, int unitMs, QString* errorMsg = nullptr);

    // 校验按压时长
    bool validatePressDuration(int value, QString* errorMsg = nullptr);

    // 校验执行次数
    bool validateRepeatCount(int value, bool infinite, QString* errorMsg = nullptr);

    // 校验启动延迟
    bool validateStartDelay(int value, QString* errorMsg = nullptr);

    // 校验坐标 (允许负值，但必须在合理范围内)
    bool validateCoordinate(int x, int y, QString* errorMsg = nullptr);

    // 校验播放速度
    bool validatePlaybackSpeed(double speed, QString* errorMsg = nullptr);

    // 校验按压时长 < 按键间隔 (完整按键模式)
    bool validatePressVsInterval(int pressDuration, int intervalMs,
                                  QString* errorMsg = nullptr);

    // 校验热键有效性
    bool validateHotkey(int key, QString* errorMsg = nullptr);

    // 通用范围信息
    struct IntRange { int min; int max; };
    IntRange clickIntervalRange();
    IntRange pressDurationRange();
    IntRange repeatCountRange();
    IntRange startDelayRange();
}

#endif // VALIDATIONUTILS_H
