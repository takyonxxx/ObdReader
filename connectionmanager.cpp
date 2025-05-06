#include "connectionmanager.h"

ConnectionManager* ConnectionManager::theInstance_ = nullptr;

ConnectionManager *ConnectionManager::getInstance()
{
    if (theInstance_ == nullptr)
    {
        theInstance_ = new ConnectionManager();
    }
    return theInstance_;
}

ConnectionManager::ConnectionManager(QObject *parent) : QObject(parent)
{
    // Initialize WiFi connection
    mElmTcpSocket = new ElmTcpSocket(this);
    if(mElmTcpSocket)
    {
        connect(mElmTcpSocket, &ElmTcpSocket::tcpConnected, this, &ConnectionManager::conConnected);
        connect(mElmTcpSocket, &ElmTcpSocket::tcpDisconnected, this, &ConnectionManager::conDisconnected);
        connect(mElmTcpSocket, &ElmTcpSocket::dataReceived, this, &ConnectionManager::conDataReceived);
        connect(mElmTcpSocket, &ElmTcpSocket::stateChanged, this, &ConnectionManager::conStateChanged);
    }

    // Initialize Bluetooth connection
    mElmBluetoothManager = new ElmBluetoothManager(this);
    if(mElmBluetoothManager)
    {
        connect(mElmBluetoothManager, &ElmBluetoothManager::btConnected, this, &ConnectionManager::btConnected);
        connect(mElmBluetoothManager, &ElmBluetoothManager::btDisconnected, this, &ConnectionManager::btDisconnected);
        connect(mElmBluetoothManager, &ElmBluetoothManager::dataReceived, this, &ConnectionManager::btDataReceived);
        connect(mElmBluetoothManager, &ElmBluetoothManager::stateChanged, this, &ConnectionManager::btStateChanged);
        connect(mElmBluetoothManager, &ElmBluetoothManager::deviceFound, this, &ConnectionManager::onBluetoothDeviceFound);
        connect(mElmBluetoothManager, &ElmBluetoothManager::deviceDiscoveryCompleted, this, &ConnectionManager::onBluetoothDiscoveryCompleted);
    }
}

bool ConnectionManager::send(const QString &command)
{
    switch(m_connectionType)
    {
    case Wifi:
        if(mElmTcpSocket)
        {
            return mElmTcpSocket->send(command);
        }
        break;

    case BlueTooth:
        if(mElmBluetoothManager)
        {
            return mElmBluetoothManager->send(command);
        }
        break;

    default:
        break;
    }

    return false;
}

QString ConnectionManager::readData(const QString &command)
{
    switch(m_connectionType)
    {
    case Wifi:
        if(mElmTcpSocket)
        {
            return mElmTcpSocket->readData(command);
        }
        break;

    case BlueTooth:
        if(mElmBluetoothManager)
        {
            return mElmBluetoothManager->readData(command);
        }
        break;

    default:
        break;
    }

    return QString();
}

void ConnectionManager::disConnectElm()
{
    switch(m_connectionType)
    {
    case Wifi:
        if(mElmTcpSocket && mElmTcpSocket->isConnected())
        {
            mElmTcpSocket->disconnectTcp();
        }
        break;

    case BlueTooth:
        if(mElmBluetoothManager && mElmBluetoothManager->isConnected())
        {
            mElmBluetoothManager->disconnectBluetooth();
        }
        break;

    default:
        break;
    }
}

void ConnectionManager::connectElm(const QString &bluetoothAddress)
{
    m_settingsManager = SettingsManager::getInstance();

    switch(m_connectionType)
    {
    case Wifi:
        if(mElmTcpSocket)
        {
            QString ip = m_settingsManager->getWifiIp();
            quint16 port = m_settingsManager->getWifiPort();
            mElmTcpSocket->connectTcp(ip, port);
        }
        break;

    case BlueTooth:
        if (!bluetoothAddress.isEmpty()) {
            // Connect directly using the provided address
            emit stateChanged("Connecting to Bluetooth device: " + bluetoothAddress);
            connectBluetooth(bluetoothAddress);
        } else {
            // No device address provided, start discovery
            emit stateChanged("Please select a Bluetooth device");
            startBluetoothDiscovery();
        }
        break;

    default:
        break;
    }
}

void ConnectionManager::connectBluetooth(const QString &deviceAddress)
{
    if(mElmBluetoothManager)
    {
        mElmBluetoothManager->connectBluetooth(deviceAddress);
    }
}

void ConnectionManager::startBluetoothDiscovery()
{
    if(mElmBluetoothManager)
    {
        mElmBluetoothManager->startDeviceDiscovery();
    }
}

void ConnectionManager::stopBluetoothDiscovery()
{
    if(mElmBluetoothManager)
    {
        mElmBluetoothManager->stopDeviceDiscovery();
    }
}

QList<QBluetoothDeviceInfo> ConnectionManager::getBluetoothDevices() const
{
    if(mElmBluetoothManager)
    {
        return mElmBluetoothManager->getDiscoveredDevices();
    }
    return QList<QBluetoothDeviceInfo>();
}

void ConnectionManager::setConnectionType(ConnectionType type)
{
    m_connectionType = type;
}

bool ConnectionManager::isConnected() const
{
    switch(m_connectionType)
    {
    case Wifi:
        if(mElmTcpSocket)
        {
            return mElmTcpSocket->isConnected();
        }
        break;

    case BlueTooth:
        if(mElmBluetoothManager)
        {
            return mElmBluetoothManager->isConnected();
        }
        break;

    default:
        break;
    }

    return false;
}

ConnectionType ConnectionManager::getCType() const
{
    return m_connectionType;
}

void ConnectionManager::conConnected()
{
    m_connected = true;
    emit connected();
}

void ConnectionManager::conDisconnected()
{
    m_connected = false;
    emit disconnected();
}

void ConnectionManager::conDataReceived(QString data)
{
    emit dataReceived(data);
}

void ConnectionManager::conStateChanged(QString state)
{
    emit stateChanged(state);
}

void ConnectionManager::btConnected()
{
    m_connected = true;
    emit connected();
}

void ConnectionManager::btDisconnected()
{
    m_connected = false;
    emit disconnected();
}

void ConnectionManager::btDataReceived(QString data)
{
    emit dataReceived(data);
}

void ConnectionManager::btStateChanged(QString state)
{
    emit stateChanged(state);
}

void ConnectionManager::onBluetoothDeviceFound(const QString &name, const QString &address)
{
    emit bluetoothDeviceFound(name, address);
}

void ConnectionManager::onBluetoothDiscoveryCompleted()
{
    emit bluetoothDiscoveryCompleted();
}
