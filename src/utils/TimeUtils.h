#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <QString>
#include <cstdint>

// ============================================================================
// 时间工具
// ============================================================================
namespace TimeUtils
{
    // 获取当前毫秒级时间戳 (用于脚本录制)
    int64_t currentTimeMs();

    // 计算剩余等待时间，若已超时则返回0
    int64_t remainingWaitMs(int64_t targetTimeMs, int64_t startTimeMs, double speedFactor = 1.0);

    // 格式化毫秒为可读字符串
    QString formatDurationMs(int64_t ms);

    // 格式化时间戳为 HH:MM:SS.mmm
    QString formatTimestampMs(int64_t ms);

    // 高精度睡眠 (可被中断)，返回是否被提前唤醒
    // 实际实现使用条件变量，这里声明接口
    bool interruptibleSleep(int64_t ms, const std::atomic_bool& stopFlag);
}

#endif // TIMEUTILS_H
