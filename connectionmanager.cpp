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

ConnectionManager::ConnectionManager(QObject *parent)
{
    mElmTcpSocket = new ElmTcpSocket(this);
    if(mElmTcpSocket)
    {
        connect(mElmTcpSocket,&ElmTcpSocket::tcpConnected,this, &ConnectionManager::conConnected);
        connect(mElmTcpSocket,&ElmTcpSocket::tcpDisconnected,this,&ConnectionManager::conDisconnected);
        connect(mElmTcpSocket,&ElmTcpSocket::dataReceived,this,&ConnectionManager::conDataReceived);
        connect(mElmTcpSocket, &ElmTcpSocket::stateChanged, this, &ConnectionManager::conStateChanged);
    }
}

bool ConnectionManager::send(const QString &command)
{
    if(mElmTcpSocket)
    {
        return  mElmTcpSocket->send(command);
    }
    return false;
}

QString ConnectionManager::readData(const QString &command)
{   
    if(mElmTcpSocket)
    {
        return  mElmTcpSocket->readData(command);
    }
    return QString();
}

void ConnectionManager::disConnectElm()
{
    if(mElmTcpSocket && mElmTcpSocket->isConnected())
    {
        mElmTcpSocket->disconnectTcp();
    }
}

void ConnectionManager::connectElm()
{
    m_settingsManager = SettingsManager::getInstance();
    if(mElmTcpSocket)
    {
        QString ip = m_settingsManager->getWifiIp();
        quint16 port = m_settingsManager->getWifiPort();
        mElmTcpSocket->connectTcp(ip, port);
    }
}

bool ConnectionManager::isConnected() const
{
    return m_connected;
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
