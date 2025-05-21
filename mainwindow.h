#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QScreen>
#include <QRegularExpression>
#include <QTimer>
#include <QDebug>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QTextBrowser>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include "connectionmanager.h"
#include "settingsmanager.h"
#include "elm.h"

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#include <QJniEnvironment>
#endif

class ObdScan;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void applyStyles();
    QString getSelectedBluetoothDeviceAddress() const;

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
    void onReadAbsFaultClicked();
    void onClearAbsFaultClicked();
    void onBluetoothDeviceFound(const QString &name, const QString &address);
    void onBluetoothDiscoveryCompleted();
    void onBluetoothDeviceSelected(int index);

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
    void setupBluetoothUI();
    void saveSettings();
    QString send(const QString &command);
    QString getData(const QString &command);
    void analysData(const QString &dataReceived);
    void getPids();
    QString cleanData(const QString& input);
    bool isError(std::string msg);
    void scanBluetoothDevices();

    // UI components
    QWidget *centralWidget;
    QGridLayout *gridLayout;
    QVBoxLayout *verticalLayout;
    QGridLayout *gridLayoutElm;

    // Bluetooth UI components
    QLabel *connectionTypeLabel;
    QComboBox *m_connectionTypeCombo;
    QLabel *btDeviceLabel;
    QComboBox *m_bluetoothDevicesCombo;

    // Connection controls
    QPushButton *pushConnect;
    QCheckBox *checkSearchPids;

    // Fault reading buttons
    QPushButton *pushReadFault;
    QPushButton *pushClearFault;
    QPushButton *btnReadTransFault;
    QPushButton *btnClearTransFault;

    // Scan and Read buttons
    QPushButton *pushScan;
    QPushButton *pushRead;

    // Command input and send
    QLineEdit *sendEdit;
    QPushButton *pushSend;

    // Protocol selection
    QComboBox *protocolCombo;
    QPushButton *pushSetProtocol;
    QPushButton *pushGetProtocol;

    // Interval controls
    QSlider *intervalSlider;
    QLabel *labelInterval;

    // Terminal display
    QTextBrowser *textTerminal;

    // Bottom buttons
    QPushButton *pushClear;
    QPushButton *pushExit;

    // Spacers
    QSpacerItem *verticalSpacer;
    QSpacerItem *verticalSpacer2;
    QSpacerItem *verticalSpacer3;
    QSpacerItem *verticalSpacer4;

    // Member variables
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
    QMap<int, QString> m_deviceAddressMap; // Maps combo box index to device address

#ifdef Q_OS_ANDROID
    void requestBluetoothPermissions();
#endif
};

#endif // MAINWINDOW_H
