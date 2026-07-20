#include "Logger.h"
#include "AppTypes.h"
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QDebug>
#include <QFileInfo>

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
    if (!s_instance) {
        s_instance = new Logger();
    }
    return s_instance;
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

    m_logFilePath = dir.filePath("keymousemaster.log");

    // 检查文件大小，如果超过上限则轮转
    QFileInfo fi(m_logFilePath);
    if (fi.exists() && fi.size() > AppConstants::MAX_LOG_SIZE) {
        QString backupPath = m_logFilePath + ".old";
        QFile::remove(backupPath);
        QFile::rename(m_logFilePath, backupPath);
    }

    m_logFile.setFileName(m_logFilePath);
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

    m_initialized = true;

    QTextStream ts(&m_logFile);
    ts << "\n========== KeyMouseMaster 启动 " << QDateTime::currentDateTime().toString(Qt::ISODate) << " ==========\n";
    ts.flush();
}

void Logger::shutdown()
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) return;

    QTextStream ts(&m_logFile);
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
        ts << formatted << "\n";
        ts.flush();

        // 轮转检查 (每100条日志检查一次)
        static int logCount = 0;
        if (++logCount % 100 == 0) {
            if (m_logFile.size() > AppConstants::MAX_LOG_SIZE) {
                locker.unlock();
                rotateLogIfNeeded();
            }
        }
    }

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
