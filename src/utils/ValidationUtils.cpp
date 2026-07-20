#include "ValidationUtils.h"
#include "AppTypes.h"

bool ValidationUtils::validateIntRange(int value, int min, int max,
                                        QString* errorMsg, const QString& fieldName)
{
    if (value < min || value > max) {
        if (errorMsg) {
            *errorMsg = QString("%1 值 %2 超出允许范围 [%3, %4]")
                .arg(fieldName.isEmpty() ? "参数" : fieldName)
                .arg(value).arg(min).arg(max);
        }
        return false;
    }
    return true;
}

bool ValidationUtils::validateClickInterval(int value, int unitMs, QString* errorMsg)
{
    int valueMs = value * unitMs;
    return validateIntRange(valueMs, AppConstants::MIN_CLICK_INTERVAL_MS,
                            AppConstants::MAX_CLICK_INTERVAL_MS, errorMsg, "点击间隔");
}

bool ValidationUtils::validatePressDuration(int value, QString* errorMsg)
{
    return validateIntRange(value, AppConstants::MIN_PRESS_DURATION_MS,
                            AppConstants::MAX_PRESS_DURATION_MS, errorMsg, "按压时长");
}

bool ValidationUtils::validateRepeatCount(int value, bool infinite, QString* errorMsg)
{
    if (infinite) return true;
    return validateIntRange(value, AppConstants::MIN_REPEAT_COUNT,
                            AppConstants::MAX_REPEAT_COUNT, errorMsg, "执行次数");
}

bool ValidationUtils::validateStartDelay(int value, QString* errorMsg)
{
    return validateIntRange(value, AppConstants::MIN_START_DELAY_MS,
                            AppConstants::MAX_START_DELAY_MS, errorMsg, "启动延迟");
}

bool ValidationUtils::validateCoordinate(int x, int y, QString* errorMsg)
{
    // Windows虚拟桌面坐标范围约为 -32768 ~ 32767 (实际取决于显示器配置)
    // 这里使用较宽松的范围
    constexpr int MIN_COORD = -65536;
    constexpr int MAX_COORD = 65536;
    if (x < MIN_COORD || x > MAX_COORD || y < MIN_COORD || y > MAX_COORD) {
        if (errorMsg)
            *errorMsg = QString("坐标 (%1, %2) 超出合理范围").arg(x).arg(y);
        return false;
    }
    return true;
}

bool ValidationUtils::validatePlaybackSpeed(double speed, QString* errorMsg)
{
    constexpr double MIN_SPEED = 0.1;
    constexpr double MAX_SPEED = 10.0;
    if (speed < MIN_SPEED || speed > MAX_SPEED) {
        if (errorMsg)
            *errorMsg = QString("播放速度 %1x 超出范围 [%2, %3]")
                .arg(speed).arg(MIN_SPEED).arg(MAX_SPEED);
        return false;
    }
    return true;
}

bool ValidationUtils::validatePressVsInterval(int pressDuration, int intervalMs,
                                               QString* errorMsg)
{
    if (pressDuration >= intervalMs) {
        if (errorMsg)
            *errorMsg = QString("按压时长(%1ms) 必须小于 按键间隔(%2ms)")
                .arg(pressDuration).arg(intervalMs);
        return false;
    }
    return true;
}

bool ValidationUtils::validateHotkey(int key, QString* errorMsg)
{
    if (key == 0) {
        if (errorMsg) *errorMsg = "快捷键未设置";
        return false;
    }
    return true;
}

ValidationUtils::IntRange ValidationUtils::clickIntervalRange()
{
    return {AppConstants::MIN_CLICK_INTERVAL_MS, AppConstants::MAX_CLICK_INTERVAL_MS};
}

ValidationUtils::IntRange ValidationUtils::pressDurationRange()
{
    return {AppConstants::MIN_PRESS_DURATION_MS, AppConstants::MAX_PRESS_DURATION_MS};
}

ValidationUtils::IntRange ValidationUtils::repeatCountRange()
{
    return {AppConstants::MIN_REPEAT_COUNT, AppConstants::MAX_REPEAT_COUNT};
}

ValidationUtils::IntRange ValidationUtils::startDelayRange()
{
    return {AppConstants::MIN_START_DELAY_MS, AppConstants::MAX_START_DELAY_MS};
}
