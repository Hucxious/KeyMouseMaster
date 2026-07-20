#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QFile>
#include <QMutex>
#include <QDateTime>

// ============================================================================
// 基础日志系统
// 支持文件日志、轮转、多级别
// ============================================================================
class Logger : public QObject
{
    Q_OBJECT

public:
    enum Level {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };

    static Logger* instance();

    void init(const QString& logDir);
    void shutdown();

    void log(Level level, const QString& message);
    void debug(const QString& msg)   { log(Debug, msg); }
    void info(const QString& msg)    { log(Info, msg); }
    void warning(const QString& msg) { log(Warning, msg); }
    void error(const QString& msg)   { log(Error, msg); }
    void critical(const QString& msg){ log(Critical, msg); }

    QString logFilePath() const { return m_logFilePath; }

signals:
    void newLogMessage(const QString& formatted);

private:
    explicit Logger(QObject* parent = nullptr);
    ~Logger() override;

    static Logger* s_instance;

    void rotateLogIfNeeded();
    QString formatMessage(Level level, const QString& message) const;
    static const char* levelToString(Level level);

    QFile m_logFile;
    QMutex m_mutex;
    QString m_logFilePath;
    QString m_logDir;
    bool m_initialized = false;
};

// 便捷宏
#define LOG_DEBUG(msg)    Logger::instance()->debug(msg)
#define LOG_INFO(msg)     Logger::instance()->info(msg)
#define LOG_WARNING(msg)  Logger::instance()->warning(msg)
#define LOG_ERROR(msg)    Logger::instance()->error(msg)
#define LOG_CRITICAL(msg) Logger::instance()->critical(msg)

#endif // LOGGER_H
