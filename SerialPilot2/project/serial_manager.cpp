#include "serial_manager.h"
#include "logger.h"

SerialManager::SerialManager(QObject *parent)
    : QObject(parent)
    , m_serialPort(new QSerialPort(this))
{
    // 连接 readyRead 信号，自动接收数据
    connect(m_serialPort, &QSerialPort::readyRead, this, [this]() {
        QByteArray data = m_serialPort->readAll();
        if (!data.isEmpty()) {
            AppLogger::instance().serialEvent(m_serialPort->portName(), "接收", data);
            emit dataReceived(data);
        }
    });

    connect(m_serialPort, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error != QSerialPort::NoError) {
            QString errorMsg = m_serialPort->errorString();
            AppLogger::instance().error("串口错误: " + errorMsg);
            emit errorOccurred(errorMsg);

            if (error == QSerialPort::ResourceError) {
                emit connectionLost();
            }
        }
    });
}

SerialManager::~SerialManager()
{
    closePort();
}

bool SerialManager::openPort(const PortConfig &config)
{
    if (m_serialPort->isOpen()) {
        closePort();
    }

    m_config = config;
    m_serialPort->setPortName(config.portName);
    m_serialPort->setBaudRate(config.baudRate);
    m_serialPort->setDataBits(config.dataBits);
    m_serialPort->setStopBits(config.stopBits);
    m_serialPort->setParity(config.parity);
    m_serialPort->setFlowControl(config.flowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        QString error = QString("无法打开串口 %1: %2").arg(config.portName, m_serialPort->errorString());
        AppLogger::instance().error(error);
        emit errorOccurred(error);
        return false;
    }

    AppLogger::instance().operation("打开串口",
        QString("端口=%1, 波特率=%2, 数据位=%3, 停止位=%4, 校验=%5, 流控=%6")
            .arg(config.portName)
            .arg(config.baudRate)
            .arg(config.dataBits)
            .arg(config.stopBits)
            .arg(config.parity)
            .arg(config.flowControl));

    return true;
}

void SerialManager::closePort()
{
    if (m_serialPort->isOpen()) {
        QString portName = m_serialPort->portName();
        m_serialPort->close();
        AppLogger::instance().operation("关闭串口", "端口=" + portName);
    }
}

bool SerialManager::isOpen() const
{
    return m_serialPort->isOpen();
}

bool SerialManager::sendData(const QByteArray &data)
{
    if (!m_serialPort->isOpen()) {
        emit errorOccurred(tr("串口未打开"));
        return false;
    }

    qint64 bytesWritten = m_serialPort->write(data);
    if (bytesWritten == -1) {
        QString error = QString("发送数据失败: %1").arg(m_serialPort->errorString());
        AppLogger::instance().error(error);
        emit errorOccurred(error);
        return false;
    }

    if (!m_serialPort->waitForBytesWritten(50)) {
        QString error = QString("发送超时: %1").arg(m_serialPort->errorString());
        AppLogger::instance().warning(error);
        emit errorOccurred(error);
        return false;
    }

    AppLogger::instance().serialEvent(m_serialPort->portName(), "发送", data);
    emit dataSent(data);
    return true;
}

QString SerialManager::portName() const
{
    return m_serialPort->portName();
}

QStringList SerialManager::availablePorts()
{
    QStringList ports;
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        ports.append(QString("%1 - %2").arg(info.portName(), info.description()));
    }
    return ports;
}

QSerialPort::PinoutSignals SerialManager::pinoutSignals() const
{
    return m_serialPort->pinoutSignals();
}
