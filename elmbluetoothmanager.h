#ifndef ELMBLUETOOTHMANAGER_H
#define ELMBLUETOOTHMANAGER_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothLocalDevice>
#include <QBluetoothSocket>
#include <QBluetoothDeviceInfo>
#include <QList>
#include <QTimer>

class ElmBluetoothManager : public QObject
{
    Q_OBJECT
public:
    explicit ElmBluetoothManager(QObject *parent = nullptr);
    ~ElmBluetoothManager();

    bool connectBluetooth(const QString &deviceAddress);
    void disconnectBluetooth();
    bool send(const QString &command);
    QString readData(const QString &command);
    bool isConnected() const;

    void startDeviceDiscovery();
    void stopDeviceDiscovery();
    QList<QBluetoothDeviceInfo> getDiscoveredDevices() const;

private:
    QBluetoothDeviceDiscoveryAgent *m_discoveryAgent{nullptr};
    QBluetoothLocalDevice *m_localDevice{nullptr};
    QBluetoothSocket *m_socket{nullptr};
    QList<QBluetoothDeviceInfo> m_discoveredDevices;
    bool m_connected{false};
    QTimer m_readTimer;
    int m_readTimeout{3000}; // 3 seconds timeout

private slots:
    void deviceDiscovered(const QBluetoothDeviceInfo &device);
    void deviceDiscoveryFinished();
    void deviceDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error error);
    void socketConnected();
    void socketDisconnected();
    void socketError(QBluetoothSocket::SocketError error);
    void socketStateChanged(QBluetoothSocket::SocketState state);
    void readyRead();
    void readTimerTimeout();

signals:
    void deviceDiscoveryCompleted();
    void deviceFound(const QString &name, const QString &address);
    void btConnected();
    void btDisconnected();
    void dataReceived(QString data);
    void stateChanged(QString state);
};

#endif // ELMBLUETOOTHMANAGER_H
