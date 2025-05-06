#include "elmbluetoothmanager.h"
#include <QCoreApplication>
#include <QDebug>

ElmBluetoothManager::ElmBluetoothManager(QObject *parent) : QObject(parent)
{
    m_localDevice = new QBluetoothLocalDevice(this);
    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);

    // Configure discovery agent
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(5000); // 5 seconds

    // Connect discovery agent signals
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &ElmBluetoothManager::deviceDiscovered);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &ElmBluetoothManager::deviceDiscoveryFinished);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, &ElmBluetoothManager::deviceDiscoveryError);
    // Setup read timer
    m_readTimer.setSingleShot(true);
    connect(&m_readTimer, &QTimer::timeout, this, &ElmBluetoothManager::readTimerTimeout);
}

ElmBluetoothManager::~ElmBluetoothManager()
{
    stopDeviceDiscovery();
    disconnectBluetooth();

    if (m_discoveryAgent) {
        delete m_discoveryAgent;
        m_discoveryAgent = nullptr;
    }

    if (m_localDevice) {
        delete m_localDevice;
        m_localDevice = nullptr;
    }
}

void ElmBluetoothManager::startDeviceDiscovery()
{
    if (m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }

    m_discoveredDevices.clear();

    // Check if Bluetooth is available and enabled
    if (!m_localDevice->isValid()) {
        emit stateChanged("Bluetooth is not available on this device");
        return;
    }

    if (m_localDevice->hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        emit stateChanged("Bluetooth is powered off. Please turn it on.");
        return;
    }

    emit stateChanged("Scanning for Bluetooth devices...");
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::ClassicMethod);
}

void ElmBluetoothManager::stopDeviceDiscovery()
{
    if (m_discoveryAgent && m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
    }
}

QList<QBluetoothDeviceInfo> ElmBluetoothManager::getDiscoveredDevices() const
{
    return m_discoveredDevices;
}

void ElmBluetoothManager::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    // Get device name
    QString deviceName = device.name();

    // Check if this device is likely an OBD adapter
    bool isObdDevice = false;
    QStringList obdKeywords = {"OBD", "ELM", "OBDII", "OBD2", "OBD-II", "VGATE", "KONNWEI", "SCAN", "BLUETOOTH"};

    // Case-insensitive check for OBD-related terms in the device name
    for (const QString &keyword : obdKeywords) {
        if (deviceName.contains(keyword, Qt::CaseInsensitive)) {
            isObdDevice = true;
            break;
        }
    }

    // Add device to the list
    m_discoveredDevices.append(device);

    // Emit signal with device information
    emit deviceFound(deviceName, device.address().toString());

    // Log device discovery
    if (isObdDevice) {
        emit stateChanged("Found OBD device: " + deviceName);
    } else {
        emit stateChanged("Found device: " + deviceName);
    }

    // If this is an OBD device, we can stop scanning to save time and battery
    if (isObdDevice) {
        stopDeviceDiscovery();
        emit stateChanged("OBD device found. Scanning stopped.");
    }
}

void ElmBluetoothManager::deviceDiscoveryFinished()
{
    emit stateChanged("Device discovery completed. Found " +
                      QString::number(m_discoveredDevices.size()) + " devices.");
    emit deviceDiscoveryCompleted();
}

void ElmBluetoothManager::deviceDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    QString errorString;
    switch (error) {
    case QBluetoothDeviceDiscoveryAgent::PoweredOffError:
        errorString = "Bluetooth is powered off";
        break;
    case QBluetoothDeviceDiscoveryAgent::InputOutputError:
        errorString = "Bluetooth I/O error";
        break;
    case QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError:
        errorString = "Invalid Bluetooth adapter";
        break;
    case QBluetoothDeviceDiscoveryAgent::UnsupportedPlatformError:
        errorString = "Unsupported platform";
        break;
    case QBluetoothDeviceDiscoveryAgent::UnsupportedDiscoveryMethod:
        errorString = "Unsupported discovery method";
        break;
    default:
        errorString = "Unknown error";
        break;
    }

    emit stateChanged("Bluetooth error: " + errorString);
}

bool ElmBluetoothManager::connectBluetooth(const QString &deviceAddress)
{
    disconnectBluetooth(); // Disconnect any existing connection

    m_socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);

    // Connect socket signals
    connect(m_socket, &QBluetoothSocket::connected, this, &ElmBluetoothManager::socketConnected);
    connect(m_socket, &QBluetoothSocket::disconnected, this, &ElmBluetoothManager::socketDisconnected);
    connect(m_socket, &QBluetoothSocket::errorOccurred,
            this, &ElmBluetoothManager::socketError);
    connect(m_socket, &QBluetoothSocket::readyRead, this, &ElmBluetoothManager::readyRead);
    connect(m_socket, &QBluetoothSocket::stateChanged, this, &ElmBluetoothManager::socketStateChanged);

    // ELM327 typically uses the SPP (Serial Port Profile) which is on UUID {00001101-0000-1000-8000-00805F9B34FB}
    const QBluetoothUuid uuid(QStringLiteral("00001101-0000-1000-8000-00805F9B34FB"));

    emit stateChanged("Connecting to device: " + deviceAddress);

    // Convert string address to QBluetoothAddress
    QBluetoothAddress address(deviceAddress);

    m_socket->connectToService(address, uuid);

    return true;
}

void ElmBluetoothManager::disconnectBluetooth()
{
    if (m_socket) {
        if (m_socket && m_socket->isOpen()) {
            m_socket->disconnectFromService();
        }

        m_socket->deleteLater();
        m_socket = nullptr;
    }

    m_connected = false;
}

bool ElmBluetoothManager::send(const QString &command)
{
    if (!m_socket || !m_socket->isOpen()) {
        return false;
    }

    // Add carriage return to the command
    QString cmdWithCR = command + "\r";
    QByteArray data = cmdWithCR.toUtf8();

    qint64 bytesWritten = m_socket->write(data);

    return (bytesWritten == data.size());
}

QString ElmBluetoothManager::readData(const QString &command)
{
    if (!m_socket || !m_socket->isOpen()) {
        return QString();
    }

    // Clear any existing data
    m_socket->readAll();

    // Send the command
    if (!send(command)) {
        return QString();
    }

    // Wait for response with timeout
    QByteArray response;
    m_readTimer.start(m_readTimeout);

    // Read loop
    while (m_readTimer.isActive()) {
        if (m_socket->waitForReadyRead(100)) {
            response += m_socket->readAll();

            // Check if response is complete (ends with prompt character '>')
            if (response.contains('>')) {
                m_readTimer.stop();
                break;
            }
        }

        QCoreApplication::processEvents();
    }

    return QString::fromUtf8(response);
}

bool ElmBluetoothManager::isConnected() const
{
    return m_connected;
}

void ElmBluetoothManager::socketConnected()
{
    m_connected = true;
    emit stateChanged("Bluetooth connected");
    emit btConnected();
}

void ElmBluetoothManager::socketDisconnected()
{
    m_connected = false;
    emit stateChanged("Bluetooth disconnected");
    emit btDisconnected();
}

void ElmBluetoothManager::socketError(QBluetoothSocket::SocketError error)
{
    QString errorString = "Bluetooth socket error: ";

    // In Qt 6.9, we can use m_socket->errorString() directly instead of mapping enum values
    if (m_socket) {
        errorString += m_socket->errorString();
    } else {
        errorString += "Unknown error";
    }

    emit stateChanged(errorString);
}

void ElmBluetoothManager::socketStateChanged(QBluetoothSocket::SocketState state)
{
    QString stateString = "Bluetooth socket state: ";

    if (m_socket) {
        if (m_socket->isOpen()) {
            stateString += "Connected";
        } else {
            stateString += "Disconnected";
        }
    } else {
        stateString += "Unknown";
    }

    emit stateChanged(stateString);
}

void ElmBluetoothManager::readyRead()
{
    if (!m_socket) {
        return;
    }

    QByteArray data = m_socket->readAll();
    QString strData = QString::fromUtf8(data);

    if (!strData.isEmpty()) {
        emit dataReceived(strData);
    }
}

void ElmBluetoothManager::readTimerTimeout()
{
    emit stateChanged("Read operation timed out");
}
