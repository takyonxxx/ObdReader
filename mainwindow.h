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
#include <QProgressBar>
#include <QListWidget>
#include <QFrame>
#include <QSysInfo>
#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QThread>

#ifdef Q_OS_ANDROID
// Qt 6.9 Android includes
#include <QJniObject>
#include <QJniEnvironment>
#include <QtCore/qjniobject.h>
#include <QtCore/qjnienvironment.h>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <jni.h>
#endif


// Bluetooth permissions (Qt 6.9 style)
#include <QBluetoothPermission>
#include <QLocationPermission>
#include <QPermission>

#include "global.h"


#ifdef Q_OS_WIN
#include <windows.h>
#endif

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

    // Connection management
    void onConnectClicked();
    void onDisconnectClicked();

    // Simplified diagnostic commands - unified for all modules
    void onReadAllSensorsClicked();
    void onReadFaultCodesClicked();
    void onClearFaultCodesClicked();

    // Manual command sending (kept for advanced users)
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

private:
    // UI Setup methods - simplified for car stereo
    void setupUI();
    void setupLeftPanel();
    void setupCenterPanel();
    void setupRightPanel();
    void setupConnections();
    void applyCarStereoStyling();

    // Helper method for sensor display creation
    void createSensorDisplay(const QString& name, const QString& defaultValue,
                             QGridLayout* layout, int row, int col, QLabel*& label);

    // Settings initialization
    void initializeSettings();

    // Device information logging
    void logDeviceInformation();

    // Connection type management
    void scanBluetoothDevices();
    QString getSelectedBluetoothDeviceAddress() const;

#ifdef Q_OS_ANDROID
    void requestBluetoothPermissions();
#endif

    // Protocol and module management
    bool switchToProtocol(WJProtocol protocol);
    bool switchToModule(WJModule module);
    void updateControlsForConnection(bool connected);
    void updateSensorLayoutForModule(WJModule module);
    void updateSensorLabelsForModule(WJModule module);

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

    // Fault code management - simplified
    void displayFaultCodes(const QList<WJ_DTC>& dtcs, const QString& moduleLabel);
    void clearFaultCodesForModule(WJModule module);

    // Simplified command execution methods
    void executeEngineCommands();
    void executeTransmissionCommands();
    void executePCMCommands();
    void executeABSCommands();

private:
    // Main UI Panels - Car Stereo Layout
    QWidget* centralWidget;
    QWidget* leftPanel;      // Connection and controls (300px)
    QWidget* centerPanel;    // Sensor data display (stretch)
    QWidget* rightPanel;     // Fault codes and log (360px)

    // Left Panel Components
    QComboBox* connectionTypeCombo;
    QLabel* connectionStatusLabel;
    QPushButton* connectButton;
    QPushButton* disconnectButton;
    QComboBox* moduleCombo;
    QLabel* currentModuleLabel;
    QPushButton* readAllSensorsButton;
    QPushButton* readFaultCodesButton;
    QPushButton* clearFaultCodesButton;

    // Connection type controls (Bluetooth)
    QLabel* btDeviceLabel;
    QComboBox* bluetoothDevicesCombo;
    QPushButton* scanBluetoothButton;
    QMap<int, QString> deviceAddressMap;

    // Center Panel Components
    QLabel* moduleTitle;
    QLabel* protocolLabel;

    // Universal sensor display labels (reused for different modules)
    QLabel* sensor1Label;    // MAF/Oil Temp/Vehicle Speed/Wheel Speed FL
    QLabel* sensor2Label;    // MAF Spec/Input Speed/Engine Load/Wheel Speed FR
    QLabel* sensor3Label;    // Rail Pressure/Output Speed/Fuel Trim ST/Wheel Speed RL
    QLabel* sensor4Label;    // MAP/Current Gear/Fuel Trim LT/Wheel Speed RR
    QLabel* sensor5Label;    // Coolant Temp/Line Pressure/O2 Sensor 1/Yaw Rate
    QLabel* sensor6Label;    // Intake Temp/Solenoid A/O2 Sensor 2/Lateral Accel
    QLabel* sensor7Label;    // Throttle Pos/Solenoid B/Timing Advance/Reserved
    QLabel* sensor8Label;    // RPM/TCC Solenoid/Barometric Pressure/Reserved
    QLabel* sensor9Label;    // Injection Qty/Torque Converter/Reserved/Reserved
    QLabel* sensor10Label;   // Battery/Reserved/Reserved/Reserved
    QLabel* sensor11Label;   // Vehicle Speed (from PCM)/Reserved/Reserved/Reserved
    QLabel* sensor12Label;   // Engine Load (from Engine)/Reserved/Reserved/Reserved

    // Mapped sensor pointers for compatibility
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
    QLabel* batteryVoltageLabel;
    QLabel* injector1Label;
    QLabel* injector2Label;
    QLabel* injector3Label;
    QLabel* injector4Label;
    QLabel* injector5Label;
    QLabel* transOilTempLabel;
    QLabel* transInputSpeedLabel;
    QLabel* transOutputSpeedLabel;
    QLabel* transCurrentGearLabel;
    QLabel* transLinePressureLabel;
    QLabel* transSolenoidALabel;
    QLabel* transSolenoidBLabel;
    QLabel* transTCCSolenoidLabel;
    QLabel* transTorqueConverterLabel;
    QLabel* vehicleSpeedLabel;
    QLabel* engineLoadLabel;
    QLabel* fuelTrimSTLabel;
    QLabel* fuelTrimLTLabel;
    QLabel* o2Sensor1Label;
    QLabel* o2Sensor2Label;
    QLabel* timingAdvanceLabel;
    QLabel* barometricPressureLabel;
    QLabel* wheelSpeedFLLabel;
    QLabel* wheelSpeedFRLabel;
    QLabel* wheelSpeedRLLabel;
    QLabel* wheelSpeedRRLabel;
    QLabel* yawRateLabel;
    QLabel* lateralAccelLabel;

    // Right Panel Components
    QListWidget* faultCodeList;
    QCheckBox* continuousReadingCheckBox;
    QSlider* readingIntervalSlider;
    QLabel* intervalLabel;
    QTextBrowser* terminalDisplay;

    // Advanced/Manual Controls (shown in right panel)
    QLineEdit* commandLineEdit;
    QPushButton* sendCommandButton;
    QPushButton* clearTerminalButton;
    QPushButton* exitButton;

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

    // Car stereo specific settings
    bool showAdvancedControls = false;
    bool autoRefreshEnabled = false;

    // Constants
    static const QString WJ_ECU_HEADER_ENGINE;
    static const QString WJ_ECU_HEADER_TRANS;
    static const QString WJ_ECU_HEADER_PCM;
    static const QString WJ_ECU_HEADER_ABS;
    static const int WJ_DEFAULT_TIMEOUT;
    static const int WJ_INIT_TIMEOUT;
    static const int WJ_PROTOCOL_SWITCH_TIMEOUT;
    static const int WJ_MAX_RETRIES;

    // Car stereo UI constants
    static const int LEFT_PANEL_WIDTH = 300;
    static const int RIGHT_PANEL_WIDTH = 360;
    static const int SENSOR_DISPLAY_WIDTH = 140;
    static const int SENSOR_DISPLAY_HEIGHT = 100;
    static const int MIN_BUTTON_HEIGHT = 40;
    static const int MIN_TOUCH_TARGET = 44; // Minimum touch target size
};

#endif // MAINWINDOW_H
