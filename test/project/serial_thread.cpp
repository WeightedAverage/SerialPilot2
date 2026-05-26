#include "serial_thread.h"
#include "serial_manager.h"
#include "logger.h"
#include <QDateTime>

SerialThread::SerialThread(SerialManager *manager, QObject *parent)
    : QThread(parent)
    , m_manager(manager)
    , m_running(false)
    , m_timerEnabled(false)
    , m_timerInterval(1000)
    , m_lastTimerTime(0)
{
    connect(m_manager, &SerialManager::errorOccurred, this, &SerialThread::errorOccurred);
    connect(m_manager, &SerialManager::connectionLost, this, [this]() {
        m_running = false;
        emit connectionLost();
    });
}

SerialThread::~SerialThread()
{
    stop();
    wait();
}

void SerialThread::stop()
{
    QMutexLocker locker(&m_mutex);
    m_running = false;
}

void SerialThread::setTimerEnabled(bool enabled, int intervalMs, const QByteArray &data)
{
    QMutexLocker locker(&m_mutex);
    m_timerEnabled = enabled;
    m_timerInterval = intervalMs;
    m_timerData = data;
    m_lastTimerTime = 0;
}

void SerialThread::run()
{
    AppLogger::instance().info("串口读取线程启动");
    m_running = true;

    while (m_running) {
        // 读取数据
        QByteArray data = m_manager->readData();
        if (!data.isEmpty()) {
            emit dataReceived(data);
        }

        // 定时发送
        {
            QMutexLocker locker(&m_mutex);
            if (m_timerEnabled && !m_timerData.isEmpty()) {
                qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
                if (currentTime - m_lastTimerTime >= m_timerInterval) {
                    m_manager->sendData(m_timerData);
                    m_lastTimerTime = currentTime;
                }
            }
        }

        msleep(5); // 5ms 循环间隔
    }

    AppLogger::instance().info("串口读取线程停止");
}
