#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <QObject>
#include "elmtcpsocket.h"
#include "elmbluetoothmanager.h"
#include "settingsmanager.h"

enum ConnectionType {BlueTooth, Wifi, Serial, None};

class ConnectionManager : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionManager(QObject *parent = nullptr);
    static ConnectionManager* getInstance();
    void connectElm(const QString &bluetoothAddress = QString());
    void disConnectElm();
    bool send(const QString &);
    QString readData(const QString &command);
    ConnectionType getCType() const;
    bool isConnected() const;

    // Bluetooth specific methods
    void startBluetoothDiscovery();
    void stopBluetoothDiscovery();
    QList<QBluetoothDeviceInfo> getBluetoothDevices() const;
    void setConnectionType(ConnectionType type);
    void connectBluetooth(const QString &deviceAddress);

private:
    SettingsManager *m_settingsManager{};
    ElmTcpSocket *mElmTcpSocket{};
    ElmBluetoothManager *mElmBluetoothManager{};
    ConnectionType m_connectionType{Wifi}; // Default to WiFi
    bool m_connected{false};

signals:
    void dataReceived(QString);
    void stateChanged(QString);
    void connected();
    void disconnected();
    void bluetoothDeviceFound(const QString &name, const QString &address);
    void bluetoothDiscoveryCompleted();

public slots:
    // WiFi connection slots
    void conConnected();
    void conDisconnected();
    void conDataReceived(QString);
    void conStateChanged(QString);

    // Bluetooth connection slots
    void btConnected();
    void btDisconnected();
    void btDataReceived(QString);
    void btStateChanged(QString);
    void onBluetoothDeviceFound(const QString &name, const QString &address);
    void onBluetoothDiscoveryCompleted();

private:
    static ConnectionManager* theInstance_;
};

#endif // CONNECTIONMANAGER_H
