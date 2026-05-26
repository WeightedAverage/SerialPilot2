#ifndef SERIAL_THREAD_H
#define SERIAL_THREAD_H

#include <QThread>
#include <QByteArray>
#include <QMutex>
#include <QWaitCondition>

class SerialManager;

class SerialThread : public QThread
{
    Q_OBJECT

public:
    explicit SerialThread(SerialManager *manager, QObject *parent = nullptr);
    ~SerialThread();

    void stop();
    void setTimerEnabled(bool enabled, int intervalMs = 1000, const QByteArray &data = QByteArray());

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &msg);
    void connectionLost();

protected:
    void run() override;

private:
    SerialManager *m_manager;
    bool m_running;
    bool m_timerEnabled;
    int m_timerInterval;
    QByteArray m_timerData;
    qint64 m_lastTimerTime;
    QMutex m_mutex;
};

#endif // SERIAL_THREAD_H
