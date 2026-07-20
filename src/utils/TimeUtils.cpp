#include "TimeUtils.h"
#include <QDateTime>
#include <QThread>
#include <QElapsedTimer>

int64_t TimeUtils::currentTimeMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

int64_t TimeUtils::remainingWaitMs(int64_t targetTimeMs, int64_t startTimeMs, double speedFactor)
{
    if (speedFactor <= 0.0) speedFactor = 1.0;

    int64_t elapsed = currentTimeMs() - startTimeMs;
    int64_t adjustedTarget = static_cast<int64_t>(targetTimeMs / speedFactor);
    int64_t remaining = adjustedTarget - elapsed;
    return (remaining > 0) ? remaining : 0;
}

QString TimeUtils::formatDurationMs(int64_t ms)
{
    if (ms < 1000)
        return QString("%1ms").arg(ms);
    if (ms < 60000) {
        double sec = ms / 1000.0;
        return QString("%1s").arg(sec, 0, 'f', 1);
    }
    int minutes = ms / 60000;
    int secs = (ms % 60000) / 1000;
    return QString("%1m%2s").arg(minutes).arg(secs);
}

QString TimeUtils::formatTimestampMs(int64_t ms)
{
    int hours = ms / 3600000;
    int mins = (ms % 3600000) / 60000;
    int secs = (ms % 60000) / 1000;
    int millis = ms % 1000;
    return QString("%1:%2:%3.%4")
        .arg(hours, 2, 10, QChar('0'))
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'))
        .arg(millis, 3, 10, QChar('0'));
}

bool TimeUtils::interruptibleSleep(int64_t ms, const std::atomic_bool& stopFlag)
{
    if (ms <= 0) return false;

    // 每 10ms 检查一次停止标志，使停止响应及时
    constexpr int64_t CHECK_INTERVAL_MS = 10;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < ms) {
        if (stopFlag.load(std::memory_order_acquire))
            return true; // 被中断

        int64_t remaining = ms - timer.elapsed();
        int64_t sleepMs = qMin(remaining, CHECK_INTERVAL_MS);
        QThread::msleep(static_cast<unsigned long>(sleepMs));
    }
    return false;
}
