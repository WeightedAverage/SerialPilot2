#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDir>

class AppLogger : public QObject
{
    Q_OBJECT

public:
    static AppLogger& instance();

    void debug(const QString &msg);
    void info(const QString &msg);
    void warning(const QString &msg);
    void error(const QString &msg);
    void critical(const QString &msg);

    void operation(const QString &op, const QString &details);
    void serialEvent(const QString &port, const QString &event, const QByteArray &data = QByteArray());

    void setLogLevel(int level);
    QString logDir() const;

private:
    explicit AppLogger(QObject *parent = nullptr);
    ~AppLogger();
    AppLogger(const AppLogger&) = delete;
    AppLogger& operator=(const AppLogger&) = delete;

    void writeLog(const QString &level, const QString &msg);
    void rotateIfNeeded();
    QString currentLogPath() const;

    QFile m_file;
    QTextStream m_stream;
    QMutex m_mutex;
    int m_logLevel; // 0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR, 4=CRITICAL
    qint64 m_maxSize;
    int m_maxBackups;
};

#endif // LOGGER_H
