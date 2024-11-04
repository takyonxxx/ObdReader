#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QtCore>
#include <QSettings>

class SettingsManager
{
public:
    SettingsManager();

    static SettingsManager* getInstance();

    void loadSettings();
    void saveSettings();

    void setEngineDisplacement(unsigned int value);
    unsigned int getEngineDisplacement() const;

    void setWifiIp(const QString &value);
    QString getWifiIp() const;

    void setWifiPort(const quint16 &value);
    quint16 getWifiPort() const;   

    void setSerialPort(const QString &value);
    QString getSerialPort() const;

private:
    static SettingsManager* theInstance_;
    QString m_sSettingsFile{};
    unsigned int EngineDisplacement{2700};
    QString WifiIp{"192.168.1.16"};
    quint16 WifiPort{35000};
    QString SerialPort{};

};

#endif // SETTINGSMANAGER_H
