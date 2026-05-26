#ifndef SERIAL_MANAGER_H
#define SERIAL_MANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>

class SerialManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager();

    // 串口配置
    struct PortConfig {
        QString portName;
        qint32 baudRate = 115200;
        QSerialPort::DataBits dataBits = QSerialPort::Data8;
        QSerialPort::StopBits stopBits = QSerialPort::OneStop;
        QSerialPort::Parity parity = QSerialPort::NoParity;
        QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
    };

    bool openPort(const PortConfig &config);
    void closePort();
    bool isOpen() const;
    bool sendData(const QByteArray &data);
    QString portName() const;

    // 获取可用串口列表
    static QStringList availablePorts();

    // 信号线状态
    QSerialPort::PinoutSignals pinoutSignals() const;

signals:
    void errorOccurred(const QString &msg);
    void dataSent(const QByteArray &data);
    void dataReceived(const QByteArray &data);
    void connectionLost();

private:
    QSerialPort *m_serialPort;
    PortConfig m_config;
};

#endif // SERIAL_MANAGER_H
