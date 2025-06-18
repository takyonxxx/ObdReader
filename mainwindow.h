#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QSlider>
#include <QTextBrowser>
#include <QSpacerItem>
#include <QTimer>
#include <QScreen>
#include <QThread>
#include <QRegularExpression>
#include <QDebug>
#include <QApplication>
#include <QGroupBox>
#include <QMessageBox>
#include <QCoreApplication>
#include <QMap>
#include <QTabWidget>
#include <QProgressBar>
#include <QTreeWidget>

#ifdef Q_OS_ANDROID
#include <QBluetoothPermission>
#include <QLocationPermission>
#include <QPermission>
#endif

#include "global.h"

// Forward declarations
class ELM;
class SettingsManager;
class ConnectionManager;

enum LogLevel {
    LOG_MINIMAL,    // Only critical events
    LOG_NORMAL,     // Important events
    LOG_VERBOSE,    // All events (current behavior)
    LOG_DEBUG       // Everything including responses
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setLogLevel(LogLevel level) { currentLogLevel = level; }

private slots:
    // Connection type management
    void onConnectionTypeChanged(int index);
    void onScanBluetoothClicked();
    void onBluetoothDeviceSelected(int index);
    void onBluetoothDeviceFound(const QString& name, const QString& address);
    void onBluetoothDiscoveryCompleted();

    // Protocol and module management
    void onProtocolSwitchRequested(WJProtocol protocol);
    void onModuleSelectionChanged(int index);
    void onAutoDetectProtocolClicked();

    // Connection management
    void onConnectClicked();
    void onDisconnectClicked();

    // Engine diagnostic commands (ISO_14230_4_KWP_FAST)
    void onReadEngineMAFClicked();
    void onReadEngineRailPressureClicked();
    void onReadEngineMAPClicked();
    void onReadEngineInjectorCorrectionsClicked();
    void onReadEngineMiscDataClicked();
    void onReadEngineBatteryVoltageClicked();
    void onReadEngineAllSensorsClicked();
    void onReadEngineFaultCodesClicked();
    void onClearEngineFaultCodesClicked();

    // Transmission diagnostic commands (J1850 VPW)
    void onReadTransmissionDataClicked();
    void onReadTransmissionSolenoidsClicked();
    void onReadTransmissionSpeedsClicked();
    void onReadTransmissionFaultCodesClicked();
    void onClearTransmissionFaultCodesClicked();

    // PCM diagnostic commands (J1850 VPW)
    void onReadPCMDataClicked();
    void onReadPCMFuelTrimClicked();
    void onReadPCMO2SensorsClicked();
    void onReadPCMFaultCodesClicked();
    void onClearPCMFaultCodesClicked();

    // ABS diagnostic commands (J1850 VPW)
    void onReadABSWheelSpeedsClicked();
    void onReadABSStabilityDataClicked();
    void onReadABSFaultCodesClicked();
    void onClearABSFaultCodesClicked();

    // Multi-module operations
    void onReadAllModuleFaultCodesClicked();
    void onClearAllModuleFaultCodesClicked();
    void onReadAllSensorDataClicked();

    // Manual command sending
    void onSendCommandClicked();
    void onClearTerminalClicked();
    void onExitClicked();

    // Connection events
    void onConnected();
    void onDisconnected();
    void onDataReceived(const QString& data);
    void onConnectionStateChanged(const QString& state);
    void processDataLine(const QString& line);

    // WJ initialization timer
    void onInitializationTimeout();

    // Continuous reading
    void onContinuousReadingToggled(bool enabled);
    void onReadingIntervalChanged(int intervalMs);

    // Tab management
    void onTabChanged(int index);

private:
    // UI Setup methods
    void setupUI();
    void setupConnectionTypeControls();
    void setupConnectionControls();
    void setupProtocolControls();
    void setupModuleSelection();
    void setupTabWidget();
    void setupEngineTab();
    void setupTransmissionTab();
    void setupPCMTab();
    void setupABSTab();
    void setupMultiModuleTab();
    void setupTerminalDisplay();
    void setupConnections();
    void applyWJStyling();

    // Settings initialization
    void initializeSettings();

    // Connection type management
    void scanBluetoothDevices();
    QString getSelectedBluetoothDeviceAddress() const;

#ifdef Q_OS_ANDROID
    void requestBluetoothPermissions();
#endif

    // Protocol and module management
    bool switchToProtocol(WJProtocol protocol);
    bool switchToModule(WJModule module);
    void updateUIForCurrentModule();
    void enableDisableControlsForModule(WJModule module, bool enabled);

    // WJ Communication methods
    bool initializeWJCommunication();
    void setupWJInitializationCommands();
    void processWJInitResponse(const QString& response);
    void sendWJCommand(const QString& command, WJModule targetModule = MODULE_UNKNOWN);
    void parseWJResponse(const QString& response);

    // Enhanced data parsing methods
    void parseEngineData(const QString& data);
    void parseTransmissionData(const QString& data);
    void parsePCMData(const QString& data);
    void parseABSData(const QString& data);
    void parseFaultCodes(const QString& data, WJModule module);
    QString formatDTCCode(int byte1, int byte2);

    // Utility methods
    QString cleanWJData(const QString& input);
    QString removeCommandEcho(const QString& data);
    bool isWJError(const QString& response);
    bool validateWJResponse(const QString& response, const QString& expectedStart);
    void logWJData(const QString& message);
    bool isImportantMessage(const QString& message);
    void updateSensorDisplays();
    void updateEngineDisplay();
    void updateTransmissionDisplay();
    void updatePCMDisplay();
    void updateABSDisplay();
    QString formatSensorValue(double value, const QString& unit, int decimals = 1);

    // Connection management
    bool connectToWJ();
    void disconnectFromWJ();
    void resetWJConnection();

    // Continuous reading
    void startContinuousReading();
    void stopContinuousReading();
    void performContinuousRead();

    // Fault code management
    void displayFaultCodes(const QList<WJ_DTC>& dtcs, const QString& moduleLabel);
    void clearFaultCodesForModule(WJModule module);

private:
    // UI Components
    QWidget* centralWidget;
    QVBoxLayout* mainLayout;
    QGridLayout* controlsLayout;
    QHBoxLayout* connectionTypeLayout;
    QVBoxLayout* connectionLayout;
    QHBoxLayout* protocolLayout;
    QVBoxLayout* moduleLayout;

    // Connection type controls
    QLabel* connectionTypeLabel;
    QComboBox* connectionTypeCombo;
    QLabel* btDeviceLabel;
    QComboBox* bluetoothDevicesCombo;
    QPushButton* scanBluetoothButton;
    QMap<int, QString> deviceAddressMap;

    // Connection controls
    QPushButton* connectButton;
    QPushButton* disconnectButton;
    QLabel* connectionStatusLabel;
    QLabel* protocolLabel;

    // Protocol and module controls
    QComboBox* protocolCombo;
    QComboBox* moduleCombo;
    QPushButton* autoDetectProtocolButton;
    QPushButton* switchProtocolButton;
    QLabel* currentProtocolLabel;
    QLabel* currentModuleLabel;

    // Tab widget for different modules
    QTabWidget* moduleTabWidget;

    // Engine tab (ISO_14230_4_KWP_FAST) controls
    QWidget* engineTab;
    QPushButton* readEngineMAFButton;
    QPushButton* readEngineRailPressureButton;
    QPushButton* readEngineMAPButton;
    QPushButton* readEngineInjectorButton;
    QPushButton* readEngineMiscButton;
    QPushButton* readEngineBatteryButton;
    QPushButton* readEngineAllSensorsButton;
    QPushButton* readEngineFaultCodesButton;
    QPushButton* clearEngineFaultCodesButton;

    // Engine sensor display labels
    QLabel* mafActualLabel;
    QLabel* mafSpecifiedLabel;
    QLabel* railPressureActualLabel;
    QLabel* railPressureSpecifiedLabel;
    QLabel* mapActualLabel;
    QLabel* mapSpecifiedLabel;
    QLabel* coolantTempLabel;
    QLabel* intakeAirTempLabel;
    QLabel* throttlePositionLabel;
    QLabel* rpmLabel;
    QLabel* injectionQuantityLabel;
    QLabel* injector1Label;
    QLabel* injector2Label;
    QLabel* injector3Label;
    QLabel* injector4Label;
    QLabel* injector5Label;
    QLabel* batteryVoltageLabel;

    // Transmission tab (J1850 VPW) controls
    QWidget* transmissionTab;
    QPushButton* readTransmissionDataButton;
    QPushButton* readTransmissionSolenoidsButton;
    QPushButton* readTransmissionSpeedsButton;
    QPushButton* readTransmissionFaultCodesButton;
    QPushButton* clearTransmissionFaultCodesButton;

    // Transmission sensor display labels
    QLabel* transOilTempLabel;
    QLabel* transInputSpeedLabel;
    QLabel* transOutputSpeedLabel;
    QLabel* transCurrentGearLabel;
    QLabel* transLinePressureLabel;
    QLabel* transSolenoidALabel;
    QLabel* transSolenoidBLabel;
    QLabel* transTCCSolenoidLabel;
    QLabel* transTorqueConverterLabel;

    // PCM tab (J1850 VPW) controls
    QWidget* pcmTab;
    QPushButton* readPCMDataButton;
    QPushButton* readPCMFuelTrimButton;
    QPushButton* readPCMO2SensorsButton;
    QPushButton* readPCMFaultCodesButton;
    QPushButton* clearPCMFaultCodesButton;

    // PCM sensor display labels
    QLabel* vehicleSpeedLabel;
    QLabel* engineLoadLabel;
    QLabel* fuelTrimSTLabel;
    QLabel* fuelTrimLTLabel;
    QLabel* o2Sensor1Label;
    QLabel* o2Sensor2Label;
    QLabel* timingAdvanceLabel;
    QLabel* barometricPressureLabel;

    // ABS tab (J1850 VPW) controls
    QWidget* absTab;
    QPushButton* readABSWheelSpeedsButton;
    QPushButton* readABSStabilityDataButton;
    QPushButton* readABSFaultCodesButton;
    QPushButton* clearABSFaultCodesButton;

    // ABS sensor display labels
    QLabel* wheelSpeedFLLabel;
    QLabel* wheelSpeedFRLabel;
    QLabel* wheelSpeedRLLabel;
    QLabel* wheelSpeedRRLabel;
    QLabel* yawRateLabel;
    QLabel* lateralAccelLabel;

    // Multi-module tab controls
    QWidget* multiModuleTab;
    QPushButton* readAllModuleFaultCodesButton;
    QPushButton* clearAllModuleFaultCodesButton;
    QPushButton* readAllSensorDataButton;
    QTreeWidget* faultCodeTree;
    QProgressBar* progressBar;
    QLabel* progressLabel;

    // Manual command controls
    QLineEdit* commandLineEdit;
    QPushButton* sendCommandButton;
    QPushButton* clearTerminalButton;
    QPushButton* exitButton;

    // Continuous reading controls
    QCheckBox* continuousReadingCheckBox;
    QSlider* readingIntervalSlider;
    QLabel* intervalLabel;

    // Terminal display
    QTextBrowser* terminalDisplay;

    // Core components
    ELM* elm;
    SettingsManager* settingsManager;
    ConnectionManager* connectionManager;

    // WJ specific members
    QList<WJCommand> initializationCommands;
    WJInitState currentInitState;
    int currentInitStep;
    QTimer* initializationTimer;
    QTimer* continuousReadingTimer;
    QString lastSentCommand;
    QString currentECUHeader;
    bool engineSecurityAccessGranted;
    WJSensorData sensorData;

    // Protocol and module state
    WJProtocol currentProtocol;
    WJModule currentModule;
    bool protocolSwitchingInProgress;

    // Connection state
    bool connected;
    bool initialized;
    bool continuousReading;
    int readingInterval;

    // Screen properties
    QRect desktopRect;
    LogLevel currentLogLevel = LOG_MINIMAL;
    QString dataBuffer;

    // Constants
    static const QString WJ_ECU_HEADER_ENGINE;
    static const QString WJ_ECU_HEADER_TRANS;
    static const QString WJ_ECU_HEADER_PCM;
    static const QString WJ_ECU_HEADER_ABS;
    static const int WJ_DEFAULT_TIMEOUT;
    static const int WJ_INIT_TIMEOUT;
    static const int WJ_PROTOCOL_SWITCH_TIMEOUT;
    static const int WJ_MAX_RETRIES;
};

#endif // MAINWINDOW_H
