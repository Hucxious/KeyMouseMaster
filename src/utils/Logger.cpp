#include "Logger.h"
#include "AppTypes.h"
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QDebug>

Logger* Logger::s_instance = nullptr;

Logger::Logger(QObject* parent)
    : QObject(parent)
{
}

Logger::~Logger()
{
    shutdown();
}

Logger* Logger::instance()
{
    static Logger instance;
    return &instance;
}

void Logger::init(const QString& logDir)
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized) return;

    m_logDir = logDir;
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 使用带时间戳的日志文件名: Log_KMM_yyyy-MM-dd_HH-mm-ss.log
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString logFileName = QString("Log_KMM_%1.log").arg(timestamp);
    m_logFilePath = dir.filePath(logFileName);

    m_logFile.setFileName(m_logFilePath);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/log";
        dir = QDir(m_logDir); dir.mkpath(".");
        m_logFilePath = dir.filePath(logFileName);
        m_logFile.setFileName(m_logFilePath);
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning("KMM: cannot open log file"); return;
        }
    }

    m_initialized = true;

    QTextStream ts(&m_logFile);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    ts.setCodec("UTF-8");
#endif
    ts << "========== KeyMouseMaster 启动 " << QDateTime::currentDateTime().toString(Qt::ISODate) << " ==========\n";
    ts.flush();

    // 清理旧日志：只保留最近30个日志文件
    QStringList logFiles = dir.entryList({"Log_KMM_*.log"}, QDir::Files, QDir::Time);
    for (int i = 30; i < logFiles.size(); ++i) {
        QFile::remove(dir.absoluteFilePath(logFiles[i]));
    }
}

void Logger::shutdown()
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return;

    QTextStream ts(&m_logFile);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    ts.setCodec("UTF-8");
#endif
    ts << "========== KeyMouseMaster 退出 " << QDateTime::currentDateTime().toString(Qt::ISODate) << " ==========\n";
    ts.flush();

    m_logFile.close();
    m_initialized = false;
}

void Logger::log(Level level, const QString& message)
{
    QMutexLocker locker(&m_mutex);

    QString formatted = formatMessage(level, message);

    // 输出到调试控制台
    qDebug().noquote() << formatted;

    // 写入文件
    if (m_initialized && m_logFile.isOpen()) {
        QTextStream ts(&m_logFile);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        ts.setCodec("UTF-8");
#endif
        ts << formatted << "\n";
        ts.flush();

        // 轮转检查 (每100条日志检查一次)
        static int logCount = 0;
        if (++logCount % 100 == 0) {
            if (m_logFile.size() > AppConstants::MAX_LOG_SIZE) {
                rotateLogIfNeeded();
            }
        }
    }

    locker.unlock();
    emit newLogMessage(formatted);
}

void Logger::rotateLogIfNeeded()
{
    m_logFile.close();
    QString backupPath = m_logFilePath + ".old";
    QFile::remove(backupPath);
    QFile::rename(m_logFilePath, backupPath);
    m_logFile.setFileName(m_logFilePath);
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

QString Logger::formatMessage(Level level, const QString& message) const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    return QString("[%1] [%2] %3")
        .arg(timestamp)
        .arg(levelToString(level))
        .arg(message);
}

const char* Logger::levelToString(Logger::Level level)
{
    switch (level) {
    case Debug:    return "DEBUG";
    case Info:     return "INFO";
    case Warning:  return "WARN";
    case Error:    return "ERROR";
    case Critical: return "CRIT";
    }
    return "UNKN";
}
