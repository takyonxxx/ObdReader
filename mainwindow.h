#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScreen>
#include <QRegularExpression>
#include <QTimer>
#include <QComboBox>
#include <QPushButton>
#include <QFormLayout>
#include <QDebug>
#include "connectionmanager.h"
#include "settingsmanager.h"
#include "elm.h"

// #ifdef Q_OS_ANDROID
// #include <QCoreApplication>
// #include <QtCore/qjnienvironment.h>
// #include <QtCore/qjniobject.h>
// #include <QMessageBox>
// #endif

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class ObdScan;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void applyStyles();

public slots:
    // Event handlers for UI interactions
    void onConnectClicked();
    void onSendClicked();
    void onReadClicked();
    void onSetProtocolClicked();
    void onGetProtocolClicked();
    void onClearClicked();
    void onReadFaultClicked();
    void onClearFaultClicked();
    void onScanClicked();
    void onIntervalSliderChanged(int value);
    void onExitClicked();
    void onSearchPidsStateChanged(int state);
    void onReadTransFaultClicked();
    void onClearTransFaultClicked();
    void onReadAirbagFaultClicked();
    void onClearAirbagFaultClicked();

private slots:
    // Communication event handlers
    void connected();
    void disconnected();
    void dataReceived(QString data);
    void stateChanged(QString state);

private:
    // Helper methods
    void setupUi();
    void setupConnections();
    void setupIntervalSlider();
    void saveSettings();
    QString send(const QString &command);
    QString getData(const QString &command);
    void analysData(const QString &dataReceived);
    void getPids();
    QString cleanData(const QString& input);
    bool isError(std::string msg);

    // Command batch processing
    void sendNextCommand();
    void processCommand(const QString &command);   

    // Member variables
    Ui::MainWindow *ui;
    ConnectionManager *m_connectionManager{nullptr};
    SettingsManager *m_settingsManager{nullptr};
    ELM *elm{nullptr};

    QStringList runtimeCommands;     // Commands for regular polling

    int commandOrder{0};             // Current position in command sequence
    int interval{100};               // Refresh interval in ms

    bool m_connected{false};         // ELM connection state
    bool m_initialized{false};       // Initialization complete
    bool m_reading{false};           // Reading in progress
    bool m_searchPidsEnable{false};  // Auto-detect PIDs
    bool m_autoRefresh{false};       // Auto-refresh enabled

    QRect desktopRect;               // Screen dimensions

    QMap<int, QString> m_deviceAddressMap;  // Maps combo box index to device address

    // Bluetooth connection handling
    void setupBluetoothUI();
    void scanBluetoothDevices();
    void onBluetoothDeviceSelected(int index);
    void onBluetoothDeviceFound(const QString &name, const QString &address);
    void onBluetoothDiscoveryCompleted();
};

#endif // MAINWINDOW_H
