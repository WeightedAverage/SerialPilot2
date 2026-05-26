#include "logger.h"
#include <QDateTime>
#include <QFileInfo>
#include <QCoreApplication>

AppLogger::AppLogger(QObject *parent)
    : QObject(parent)
    , m_logLevel(0)
    , m_maxSize(10 * 1024 * 1024) // 10MB
    , m_maxBackups(5)
{
    QDir dir(logDir());
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    m_file.setFileName(currentLogPath());
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    m_stream.setDevice(&m_file);
}

AppLogger::~AppLogger()
{
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

AppLogger& AppLogger::instance()
{
    static AppLogger logger;
    return logger;
}

void AppLogger::setLogLevel(int level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

QString AppLogger::logDir() const
{
    return QCoreApplication::applicationDirPath() + "/logs";
}

QString AppLogger::currentLogPath() const
{
    QString date = QDateTime::currentDateTime().toString("yyyyMMdd");
    return logDir() + "/assistant_" + date + ".log";
}

void AppLogger::rotateIfNeeded()
{
    if (!m_file.isOpen()) return;

    if (m_file.size() >= m_maxSize) {
        m_stream.flush();
        m_file.close();

        // 删除最旧的备份
        for (int i = m_maxBackups; i > 0; --i) {
            QString oldPath = currentLogPath() + "." + QString::number(i);
            if (i == m_maxBackups) {
                QFile::remove(oldPath);
            }
            QString prevPath = (i == 1) ? currentLogPath() : currentLogPath() + "." + QString::number(i - 1);
            QFile::rename(prevPath, oldPath);
        }

        m_file.setFileName(currentLogPath());
        m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        m_stream.setDevice(&m_file);
    }
}

void AppLogger::writeLog(const QString &level, const QString &msg)
{
    QMutexLocker locker(&m_mutex);

    rotateIfNeeded();

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    m_stream << timestamp << " | " << level.leftJustified(8) << " | SerialAssistant | " << msg << "\n";
    m_stream.flush();
}

void AppLogger::debug(const QString &msg)
{
    if (m_logLevel <= 0) writeLog("DEBUG", msg);
}

void AppLogger::info(const QString &msg)
{
    if (m_logLevel <= 1) writeLog("INFO", msg);
}

void AppLogger::warning(const QString &msg)
{
    if (m_logLevel <= 2) writeLog("WARNING", msg);
}

void AppLogger::error(const QString &msg)
{
    if (m_logLevel <= 3) writeLog("ERROR", msg);
}

void AppLogger::critical(const QString &msg)
{
    if (m_logLevel <= 4) writeLog("CRITICAL", msg);
}

void AppLogger::operation(const QString &op, const QString &details)
{
    info("[操作] " + op + " - " + details);
}

void AppLogger::serialEvent(const QString &port, const QString &event, const QByteArray &data)
{
    QString msg = "[串口:" + port + "] " + event;
    if (!data.isEmpty()) {
        msg += ": " + QString::number(data.size()) + " bytes";
        msg += " | 数据: " + data.toHex(' ').toUpper();
    }
    info(msg);
}
