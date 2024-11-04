#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <QObject>
#include "elmtcpsocket.h"
#include "settingsmanager.h"

enum ConnectionType {BlueTooth, Wifi, Serial, None};

class ConnectionManager : public QObject
{
      Q_OBJECT

public:
    explicit ConnectionManager(QObject *parent = nullptr);
    static ConnectionManager* getInstance();

    void connectElm();
    void disConnectElm();

    bool send(const QString &);
    QString readData(const QString &command);
    ConnectionType getCType() const;

    bool isConnected() const;

private:
    SettingsManager *m_settingsManager{};
    ElmTcpSocket *mElmTcpSocket{};
    bool m_connected{false};

signals:
    void dataReceived(QString);
    void stateChanged(QString);
    void connected();
    void disconnected();

public slots:
    void conConnected();
    void conDisconnected();
    void conDataReceived(QString);
    void conStateChanged(QString);

private:
     static ConnectionManager* theInstance_;

};

#endif // CONNECTIONMANAGER_H
