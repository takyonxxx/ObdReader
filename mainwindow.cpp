#include "mainwindow.h"
#include "global.h"
#include <QApplication>
#include <QScreen>
#include <QThread>
#include <QTimer>
#include <QDateTime>
#include <QMessageBox>

#include "elm.h"
#include "settingsmanager.h"
#include "connectionmanager.h"

// WJ Constants Implementation
const QString MainWindow::WJ_ECU_HEADER_ENGINE = WJ::Headers::ENGINE_EDC15;
const QString MainWindow::WJ_ECU_HEADER_TRANS = WJ::Headers::TRANSMISSION;
const QString MainWindow::WJ_ECU_HEADER_PCM = WJ::Headers::PCM;
const QString MainWindow::WJ_ECU_HEADER_ABS = WJ::Headers::ABS;
const int MainWindow::WJ_DEFAULT_TIMEOUT = WJ::Protocols::DEFAULT_TIMEOUT;
const int MainWindow::WJ_INIT_TIMEOUT = 30000;
const int MainWindow::WJ_PROTOCOL_SWITCH_TIMEOUT = WJ::Protocols::PROTOCOL_SWITCH_TIMEOUT;
const int MainWindow::WJ_MAX_RETRIES = 3;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , centralWidget(nullptr)
    , elm(nullptr)
    , settingsManager(nullptr)
    , connectionManager(nullptr)
    , currentInitState(STATE_DISCONNECTED)
    , currentInitStep(0)
    , initializationTimer(new QTimer(this))
    , continuousReadingTimer(new QTimer(this))
    , engineSecurityAccessGranted(false)
    , currentProtocol(PROTOCOL_UNKNOWN)
    , currentModule(MODULE_UNKNOWN)
    , protocolSwitchingInProgress(false)
    , connected(false)
    , initialized(false)
    , continuousReading(false)
    , readingInterval(1000)
{
    setWindowTitle("Enhanced Jeep WJ Diagnostic Tool - Dual Protocol Support");

    // Get screen geometry
    QScreen *screen = window()->screen();
    desktopRect = screen->availableGeometry();
    qDebug() << desktopRect.width() << desktopRect.height();

    // Setup WJ initialization commands
    setupWJInitializationCommands();

    // Setup UI
    setupUI();
    applyWJStyling();

    // Initialize components
    elm = ELM::getInstance();
    settingsManager = SettingsManager::getInstance();
    connectionManager = ConnectionManager::getInstance();

    // Setup connections
    setupConnections();

    // Setup timers
    initializationTimer->setSingleShot(true);
    initializationTimer->setInterval(WJ_INIT_TIMEOUT);
    connect(initializationTimer, &QTimer::timeout, this, &MainWindow::onInitializationTimeout);

    continuousReadingTimer->setInterval(readingInterval);
    connect(continuousReadingTimer, &QTimer::timeout, this, &MainWindow::performContinuousRead);

    // Initialize settings with platform-specific defaults
    initializeSettings();

    // Reset sensor data
    sensorData.reset();

    // Initial status
    logWJData("Enhanced Jeep WJ Diagnostic Tool Initialized");
    logWJData("Target: Jeep Grand Cherokee WJ 2.7 CRD (All Modules)");
    logWJData("Protocols: ISO 9141-2 (Engine) + J1850 VPW (Trans/PCM/ABS)");

    // Display connection settings
    if (settingsManager) {
        logWJData("WiFi IP: " + settingsManager->getWifiIp() + ":" +
                  QString::number(settingsManager->getWifiPort()));
    }

    logWJData("Resolution: " + QString::number(desktopRect.width()) + "x" + QString::number(desktopRect.height()));
}

MainWindow::~MainWindow() {
    if (connected) {
        disconnectFromWJ();
    }

    if (settingsManager) {
        settingsManager->saveSettings();
    }
}

void MainWindow::setupWJInitializationCommands() {
    // Start with ISO 9141-2 for engine module by default
    initializationCommands = WJCommands::getInitSequence(PROTOCOL_ISO9141_2);
}

void MainWindow::initializeSettings() {
#ifdef Q_OS_ANDROID
    // Android configuration
    QString ip = "192.168.0.10";
#elif defined(Q_OS_WINDOWS)
    // Windows configuration (elm -n 35000 -s car)
    QString ip = "192.168.1.4";
#else
    // Default configuration for other platforms
    QString ip = "192.168.0.10";
#endif

    quint16 wifiPort = 35000;

    if (settingsManager) {
        settingsManager->setWifiIp(ip);
        settingsManager->setWifiPort(wifiPort);
        settingsManager->setSerialPort("/dev/ttys001");
        settingsManager->setEngineDisplacement(2700);
        settingsManager->saveSettings();
    }
}

// UI Setup Methods
void MainWindow::setupUI() {
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // Setup different UI sections
    setupConnectionTypeControls();
    setupConnectionControls();
    setupProtocolControls();
    setupModuleSelection();
    setupTabWidget();
    setupTerminalDisplay();

    // Add layouts to main layout
    mainLayout->addLayout(connectionTypeLayout);
    mainLayout->addLayout(connectionLayout);
    mainLayout->addLayout(protocolLayout);
    mainLayout->addWidget(moduleTabWidget, 1); // Tabs take most space
    mainLayout->addWidget(terminalDisplay);
}

void MainWindow::setupConnectionTypeControls() {
    connectionTypeLayout = new QHBoxLayout();

    // Connection type selection
    connectionTypeLabel = new QLabel("Connection:");
    connectionTypeLabel->setMinimumHeight(40);

    connectionTypeCombo = new QComboBox();
    connectionTypeCombo->setMinimumHeight(44);
    connectionTypeCombo->addItem("WiFi");
    connectionTypeCombo->addItem("Bluetooth");
    connectionTypeCombo->setCurrentIndex(0); // Default to WiFi

    // Bluetooth device selection
    btDeviceLabel = new QLabel("BT Device:");
    btDeviceLabel->setMinimumHeight(40);
    btDeviceLabel->setVisible(false); // Hidden by default (WiFi selected)

    bluetoothDevicesCombo = new QComboBox();
    bluetoothDevicesCombo->setMinimumHeight(44);
    bluetoothDevicesCombo->addItem("Select device...");
    bluetoothDevicesCombo->setVisible(false); // Hidden by default

    // Scan bluetooth button
    scanBluetoothButton = new QPushButton("Scan BT");
    scanBluetoothButton->setMinimumHeight(44);
    scanBluetoothButton->setVisible(false);

    connectionTypeLayout->addWidget(connectionTypeLabel);
    connectionTypeLayout->addWidget(connectionTypeCombo);
    connectionTypeLayout->addWidget(btDeviceLabel);
    connectionTypeLayout->addWidget(bluetoothDevicesCombo, 1);
    connectionTypeLayout->addWidget(scanBluetoothButton);
    connectionTypeLayout->addStretch();
}

void MainWindow::setupConnectionControls() {
    connectionLayout = new QHBoxLayout();

    // Connection status
    connectionStatusLabel = new QLabel("Status: Disconnected");
    connectionStatusLabel->setMinimumHeight(40);

    protocolLabel = new QLabel("Protocol: Unknown");
    protocolLabel->setMinimumHeight(40);

    // Connection buttons
    connectButton = new QPushButton("Connect to WJ");
    connectButton->setMinimumHeight(50);
    connectButton->setMinimumWidth(150);

    disconnectButton = new QPushButton("Disconnect");
    disconnectButton->setMinimumHeight(50);
    disconnectButton->setMinimumWidth(120);
    disconnectButton->setEnabled(false);

    // Clear and exit buttons
    clearTerminalButton = new QPushButton("Reset");
    clearTerminalButton->setMinimumHeight(50);

    exitButton = new QPushButton("Exit");
    exitButton->setMinimumHeight(50);

    connectionLayout->addWidget(connectionStatusLabel);
    connectionLayout->addWidget(protocolLabel);
    connectionLayout->addStretch();
    connectionLayout->addWidget(connectButton);
    connectionLayout->addWidget(disconnectButton);
    connectionLayout->addWidget(clearTerminalButton);
    connectionLayout->addWidget(exitButton);
}

void MainWindow::setupProtocolControls() {
    protocolLayout = new QHBoxLayout();

    // Protocol selection
    QLabel* protocolSelectionLabel = new QLabel("Protocol:");
    protocolCombo = new QComboBox();
    protocolCombo->setMinimumHeight(40);
    protocolCombo->addItem("Auto Detect");
    protocolCombo->addItem("ISO 9141-2 (Engine)");
    protocolCombo->addItem("J1850 VPW (Trans/PCM/ABS)");
    protocolCombo->setCurrentIndex(0);

    autoDetectProtocolButton = new QPushButton("Auto Detect");
    autoDetectProtocolButton->setMinimumHeight(40);

    switchProtocolButton = new QPushButton("Switch");
    switchProtocolButton->setMinimumHeight(40);
    switchProtocolButton->setEnabled(false);

    currentProtocolLabel = new QLabel("Current: Unknown");
    currentProtocolLabel->setMinimumHeight(40);

    protocolLayout->addWidget(protocolSelectionLabel);
    protocolLayout->addWidget(protocolCombo);
    protocolLayout->addWidget(autoDetectProtocolButton);
    protocolLayout->addWidget(switchProtocolButton);
    protocolLayout->addWidget(currentProtocolLabel);
    protocolLayout->addStretch();
}

void MainWindow::setupModuleSelection() {
    moduleLayout = new QVBoxLayout();

    QLabel* moduleLabel = new QLabel("Active Module:");
    moduleCombo = new QComboBox();
    moduleCombo->setMinimumHeight(40);
    moduleCombo->addItem("Engine (EDC15) - ISO 9141-2");
    moduleCombo->addItem("Transmission - J1850 VPW");
    moduleCombo->addItem("PCM - J1850 VPW");
    moduleCombo->addItem("ABS - J1850 VPW");
    moduleCombo->setCurrentIndex(0);

    currentModuleLabel = new QLabel("Current: Engine (EDC15)");
    currentModuleLabel->setMinimumHeight(40);

    QHBoxLayout* moduleSelectionLayout = new QHBoxLayout();
    moduleSelectionLayout->addWidget(moduleLabel);
    moduleSelectionLayout->addWidget(moduleCombo);
    moduleSelectionLayout->addWidget(currentModuleLabel);
    moduleSelectionLayout->addStretch();

    moduleLayout->addLayout(moduleSelectionLayout);
}

void MainWindow::setupTabWidget() {
    moduleTabWidget = new QTabWidget();
    moduleTabWidget->setMinimumHeight(400);

    setupEngineTab();
    setupTransmissionTab();
    setupPCMTab();
    setupABSTab();
    setupMultiModuleTab();

    // Add tabs
    moduleTabWidget->addTab(engineTab, "Engine (ISO 9141-2)");
    moduleTabWidget->addTab(transmissionTab, "Transmission (J1850 VPW)");
    moduleTabWidget->addTab(pcmTab, "PCM (J1850 VPW)");
    moduleTabWidget->addTab(absTab, "ABS (J1850 VPW)");
    moduleTabWidget->addTab(multiModuleTab, "Multi-Module Operations");

    // Set engine tab as default
    moduleTabWidget->setCurrentIndex(0);
}

void MainWindow::setupEngineTab() {
    engineTab = new QWidget();
    QVBoxLayout* engineLayout = new QVBoxLayout(engineTab);

    // Engine diagnostic buttons
    QGridLayout* engineButtonLayout = new QGridLayout();

    readEngineMAFButton = new QPushButton("Read MAF");
    readEngineRailPressureButton = new QPushButton("Read Rail Pressure");
    readEngineMAPButton = new QPushButton("Read MAP");
    readEngineInjectorButton = new QPushButton("Read Injectors");
    readEngineMiscButton = new QPushButton("Read Misc Data");
    readEngineBatteryButton = new QPushButton("Read Battery");
    readEngineAllSensorsButton = new QPushButton("Read All Sensors");
    readEngineFaultCodesButton = new QPushButton("Read Fault Codes");
    clearEngineFaultCodesButton = new QPushButton("Clear Fault Codes");

    // Set minimum sizes and disable initially
    QList<QPushButton*> engineButtons = {
        readEngineMAFButton, readEngineRailPressureButton, readEngineMAPButton,
        readEngineInjectorButton, readEngineMiscButton, readEngineBatteryButton,
        readEngineAllSensorsButton, readEngineFaultCodesButton, clearEngineFaultCodesButton
    };

    for (auto* button : engineButtons) {
        button->setMinimumHeight(45);
        button->setEnabled(false);
    }

    // Arrange engine buttons
    engineButtonLayout->addWidget(readEngineMAFButton, 0, 0);
    engineButtonLayout->addWidget(readEngineRailPressureButton, 0, 1);
    engineButtonLayout->addWidget(readEngineMAPButton, 0, 2);
    engineButtonLayout->addWidget(readEngineInjectorButton, 1, 0);
    engineButtonLayout->addWidget(readEngineMiscButton, 1, 1);
    engineButtonLayout->addWidget(readEngineBatteryButton, 1, 2);
    engineButtonLayout->addWidget(readEngineAllSensorsButton, 2, 0);
    engineButtonLayout->addWidget(readEngineFaultCodesButton, 2, 1);
    engineButtonLayout->addWidget(clearEngineFaultCodesButton, 2, 2);

    // Engine sensor display
    QGroupBox* engineSensorGroup = new QGroupBox("Engine Sensor Data (EDC15)");
    QGridLayout* engineSensorLayout = new QGridLayout(engineSensorGroup);

    // Create engine sensor labels
    mafActualLabel = new QLabel("MAF Actual: -- g/s");
    mafSpecifiedLabel = new QLabel("MAF Specified: -- g/s");
    railPressureActualLabel = new QLabel("Rail Pressure: -- bar");
    railPressureSpecifiedLabel = new QLabel("Rail Pressure Spec: -- bar");
    mapActualLabel = new QLabel("MAP Actual: -- mbar");
    mapSpecifiedLabel = new QLabel("MAP Specified: -- mbar");
    coolantTempLabel = new QLabel("Coolant Temp: -- °C");
        intakeAirTempLabel = new QLabel("IAT: -- °C");
        throttlePositionLabel = new QLabel("TPS: -- %");
    rpmLabel = new QLabel("RPM: -- rpm");
    injectionQuantityLabel = new QLabel("IQ: -- mg");
    batteryVoltageLabel = new QLabel("Battery: -- V");

    injector1Label = new QLabel("Inj 1: -- mg");
    injector2Label = new QLabel("Inj 2: -- mg");
    injector3Label = new QLabel("Inj 3: -- mg");
    injector4Label = new QLabel("Inj 4: -- mg");
    injector5Label = new QLabel("Inj 5: -- mg");

    // Arrange engine sensors
    engineSensorLayout->addWidget(mafActualLabel, 0, 0);
    engineSensorLayout->addWidget(mafSpecifiedLabel, 0, 1);
    engineSensorLayout->addWidget(railPressureActualLabel, 1, 0);
    engineSensorLayout->addWidget(railPressureSpecifiedLabel, 1, 1);
    engineSensorLayout->addWidget(mapActualLabel, 2, 0);
    engineSensorLayout->addWidget(mapSpecifiedLabel, 2, 1);
    engineSensorLayout->addWidget(coolantTempLabel, 3, 0);
    engineSensorLayout->addWidget(intakeAirTempLabel, 3, 1);
    engineSensorLayout->addWidget(throttlePositionLabel, 4, 0);
    engineSensorLayout->addWidget(rpmLabel, 4, 1);
    engineSensorLayout->addWidget(injectionQuantityLabel, 5, 0);
    engineSensorLayout->addWidget(batteryVoltageLabel, 5, 1);

    engineSensorLayout->addWidget(injector1Label, 6, 0);
    engineSensorLayout->addWidget(injector2Label, 6, 1);
    engineSensorLayout->addWidget(injector3Label, 7, 0);
    engineSensorLayout->addWidget(injector4Label, 7, 1);
    engineSensorLayout->addWidget(injector5Label, 8, 0);

    engineLayout->addLayout(engineButtonLayout);
    engineLayout->addWidget(engineSensorGroup);
}

void MainWindow::setupTransmissionTab() {
    transmissionTab = new QWidget();
    QVBoxLayout* transmissionLayout = new QVBoxLayout(transmissionTab);

    // Transmission diagnostic buttons
    QGridLayout* transmissionButtonLayout = new QGridLayout();

    readTransmissionDataButton = new QPushButton("Read Trans Data");
    readTransmissionSolenoidsButton = new QPushButton("Read Solenoids");
    readTransmissionSpeedsButton = new QPushButton("Read Speeds");
    readTransmissionFaultCodesButton = new QPushButton("Read Fault Codes");
    clearTransmissionFaultCodesButton = new QPushButton("Clear Fault Codes");

    // Set minimum sizes and disable initially
    QList<QPushButton*> transmissionButtons = {
        readTransmissionDataButton, readTransmissionSolenoidsButton, readTransmissionSpeedsButton,
        readTransmissionFaultCodesButton, clearTransmissionFaultCodesButton
    };

    for (auto* button : transmissionButtons) {
        button->setMinimumHeight(45);
        button->setEnabled(false);
    }

    // Arrange transmission buttons
    transmissionButtonLayout->addWidget(readTransmissionDataButton, 0, 0);
    transmissionButtonLayout->addWidget(readTransmissionSolenoidsButton, 0, 1);
    transmissionButtonLayout->addWidget(readTransmissionSpeedsButton, 0, 2);
    transmissionButtonLayout->addWidget(readTransmissionFaultCodesButton, 1, 0);
    transmissionButtonLayout->addWidget(clearTransmissionFaultCodesButton, 1, 1);

    // Transmission sensor display
    QGroupBox* transmissionSensorGroup = new QGroupBox("Transmission Data (J1850 VPW)");
    QGridLayout* transmissionSensorLayout = new QGridLayout(transmissionSensorGroup);

    // Create transmission sensor labels
    transOilTempLabel = new QLabel("Oil Temp: -- °C");
        transInputSpeedLabel = new QLabel("Input Speed: -- rpm");
    transOutputSpeedLabel = new QLabel("Output Speed: -- rpm");
    transCurrentGearLabel = new QLabel("Current Gear: --");
    transLinePressureLabel = new QLabel("Line Pressure: -- psi");
    transSolenoidALabel = new QLabel("Solenoid A: -- %");
    transSolenoidBLabel = new QLabel("Solenoid B: -- %");
    transTCCSolenoidLabel = new QLabel("TCC Solenoid: -- %");
    transTorqueConverterLabel = new QLabel("Torque Converter: -- %");

    // Arrange transmission sensors
    transmissionSensorLayout->addWidget(transOilTempLabel, 0, 0);
    transmissionSensorLayout->addWidget(transInputSpeedLabel, 0, 1);
    transmissionSensorLayout->addWidget(transOutputSpeedLabel, 1, 0);
    transmissionSensorLayout->addWidget(transCurrentGearLabel, 1, 1);
    transmissionSensorLayout->addWidget(transLinePressureLabel, 2, 0);
    transmissionSensorLayout->addWidget(transSolenoidALabel, 2, 1);
    transmissionSensorLayout->addWidget(transSolenoidBLabel, 3, 0);
    transmissionSensorLayout->addWidget(transTCCSolenoidLabel, 3, 1);
    transmissionSensorLayout->addWidget(transTorqueConverterLabel, 4, 0);

    transmissionLayout->addLayout(transmissionButtonLayout);
    transmissionLayout->addWidget(transmissionSensorGroup);
}

void MainWindow::setupPCMTab() {
    pcmTab = new QWidget();
    QVBoxLayout* pcmLayout = new QVBoxLayout(pcmTab);

    // PCM diagnostic buttons
    QGridLayout* pcmButtonLayout = new QGridLayout();

    readPCMDataButton = new QPushButton("Read PCM Data");
    readPCMFuelTrimButton = new QPushButton("Read Fuel Trim");
    readPCMO2SensorsButton = new QPushButton("Read O2 Sensors");
    readPCMFaultCodesButton = new QPushButton("Read Fault Codes");
    clearPCMFaultCodesButton = new QPushButton("Clear Fault Codes");

    // Set minimum sizes and disable initially
    QList<QPushButton*> pcmButtons = {
        readPCMDataButton, readPCMFuelTrimButton, readPCMO2SensorsButton,
        readPCMFaultCodesButton, clearPCMFaultCodesButton
    };

    for (auto* button : pcmButtons) {
        button->setMinimumHeight(45);
        button->setEnabled(false);
    }

    // Arrange PCM buttons
    pcmButtonLayout->addWidget(readPCMDataButton, 0, 0);
    pcmButtonLayout->addWidget(readPCMFuelTrimButton, 0, 1);
    pcmButtonLayout->addWidget(readPCMO2SensorsButton, 0, 2);
    pcmButtonLayout->addWidget(readPCMFaultCodesButton, 1, 0);
    pcmButtonLayout->addWidget(clearPCMFaultCodesButton, 1, 1);

    // PCM sensor display
    QGroupBox* pcmSensorGroup = new QGroupBox("PCM Data (J1850 VPW)");
    QGridLayout* pcmSensorLayout = new QGridLayout(pcmSensorGroup);

    // Create PCM sensor labels
    vehicleSpeedLabel = new QLabel("Vehicle Speed: -- km/h");
    engineLoadLabel = new QLabel("Engine Load: -- %");
    fuelTrimSTLabel = new QLabel("Fuel Trim ST: -- %");
    fuelTrimLTLabel = new QLabel("Fuel Trim LT: -- %");
    o2Sensor1Label = new QLabel("O2 Sensor 1: -- V");
    o2Sensor2Label = new QLabel("O2 Sensor 2: -- V");
    timingAdvanceLabel = new QLabel("Timing Advance: -- °");
        barometricPressureLabel = new QLabel("Barometric Pressure: -- kPa");

    // Arrange PCM sensors
    pcmSensorLayout->addWidget(vehicleSpeedLabel, 0, 0);
    pcmSensorLayout->addWidget(engineLoadLabel, 0, 1);
    pcmSensorLayout->addWidget(fuelTrimSTLabel, 1, 0);
    pcmSensorLayout->addWidget(fuelTrimLTLabel, 1, 1);
    pcmSensorLayout->addWidget(o2Sensor1Label, 2, 0);
    pcmSensorLayout->addWidget(o2Sensor2Label, 2, 1);
    pcmSensorLayout->addWidget(timingAdvanceLabel, 3, 0);
    pcmSensorLayout->addWidget(barometricPressureLabel, 3, 1);

    pcmLayout->addLayout(pcmButtonLayout);
    pcmLayout->addWidget(pcmSensorGroup);
}

void MainWindow::setupABSTab() {
    absTab = new QWidget();
    QVBoxLayout* absLayout = new QVBoxLayout(absTab);

    // ABS diagnostic buttons
    QGridLayout* absButtonLayout = new QGridLayout();

    readABSWheelSpeedsButton = new QPushButton("Read Wheel Speeds");
    readABSStabilityDataButton = new QPushButton("Read Stability Data");
    readABSFaultCodesButton = new QPushButton("Read Fault Codes");
    clearABSFaultCodesButton = new QPushButton("Clear Fault Codes");

    // Set minimum sizes and disable initially
    QList<QPushButton*> absButtons = {
        readABSWheelSpeedsButton, readABSStabilityDataButton,
        readABSFaultCodesButton, clearABSFaultCodesButton
    };

    for (auto* button : absButtons) {
        button->setMinimumHeight(45);
        button->setEnabled(false);
    }

    // Arrange ABS buttons
    absButtonLayout->addWidget(readABSWheelSpeedsButton, 0, 0);
    absButtonLayout->addWidget(readABSStabilityDataButton, 0, 1);
    absButtonLayout->addWidget(readABSFaultCodesButton, 1, 0);
    absButtonLayout->addWidget(clearABSFaultCodesButton, 1, 1);

    // ABS sensor display
    QGroupBox* absSensorGroup = new QGroupBox("ABS Data (J1850 VPW)");
    QGridLayout* absSensorLayout = new QGridLayout(absSensorGroup);

    // Create ABS sensor labels
    wheelSpeedFLLabel = new QLabel("Front Left: -- km/h");
    wheelSpeedFRLabel = new QLabel("Front Right: -- km/h");
    wheelSpeedRLLabel = new QLabel("Rear Left: -- km/h");
    wheelSpeedRRLabel = new QLabel("Rear Right: -- km/h");
    yawRateLabel = new QLabel("Yaw Rate: -- deg/s");
    lateralAccelLabel = new QLabel("Lateral Accel: -- g");

    // Arrange ABS sensors
    absSensorLayout->addWidget(wheelSpeedFLLabel, 0, 0);
    absSensorLayout->addWidget(wheelSpeedFRLabel, 0, 1);
    absSensorLayout->addWidget(wheelSpeedRLLabel, 1, 0);
    absSensorLayout->addWidget(wheelSpeedRRLabel, 1, 1);
    absSensorLayout->addWidget(yawRateLabel, 2, 0);
    absSensorLayout->addWidget(lateralAccelLabel, 2, 1);

    absLayout->addLayout(absButtonLayout);
    absLayout->addWidget(absSensorGroup);
}

void MainWindow::setupMultiModuleTab() {
    multiModuleTab = new QWidget();
    QVBoxLayout* multiModuleLayout = new QVBoxLayout(multiModuleTab);

    // Multi-module operation buttons
    QHBoxLayout* multiModuleButtonLayout = new QHBoxLayout();

    readAllModuleFaultCodesButton = new QPushButton("Read All Fault Codes");
    clearAllModuleFaultCodesButton = new QPushButton("Clear All Fault Codes");
    readAllSensorDataButton = new QPushButton("Read All Sensor Data");

    readAllModuleFaultCodesButton->setMinimumHeight(45);
    clearAllModuleFaultCodesButton->setMinimumHeight(45);
    readAllSensorDataButton->setMinimumHeight(45);

    readAllModuleFaultCodesButton->setEnabled(false);
    clearAllModuleFaultCodesButton->setEnabled(false);
    readAllSensorDataButton->setEnabled(false);

    multiModuleButtonLayout->addWidget(readAllModuleFaultCodesButton);
    multiModuleButtonLayout->addWidget(clearAllModuleFaultCodesButton);
    multiModuleButtonLayout->addWidget(readAllSensorDataButton);

    // Progress indicators
    progressLabel = new QLabel("Ready");
    progressBar = new QProgressBar();
    progressBar->setVisible(false);

    // Fault code tree widget
    faultCodeTree = new QTreeWidget();
    faultCodeTree->setHeaderLabels(QStringList() << "Module" << "Code" << "Description" << "Status");
    faultCodeTree->setMinimumHeight(300);

    // Continuous reading controls
    QHBoxLayout* continuousLayout = new QHBoxLayout();

    continuousReadingCheckBox = new QCheckBox("Continuous Reading");
    continuousReadingCheckBox->setMinimumHeight(40);

    QLabel* intervalLabelText = new QLabel("Interval:");
    readingIntervalSlider = new QSlider(Qt::Horizontal);
    readingIntervalSlider->setRange(500, 5000);
    readingIntervalSlider->setValue(1000);
    readingIntervalSlider->setMinimumHeight(40);

    intervalLabel = new QLabel("1000 ms");
    intervalLabel->setMinimumWidth(80);
    intervalLabel->setAlignment(Qt::AlignCenter);

    continuousLayout->addWidget(continuousReadingCheckBox);
    continuousLayout->addWidget(intervalLabelText);
    continuousLayout->addWidget(readingIntervalSlider);
    continuousLayout->addWidget(intervalLabel);
    continuousLayout->addStretch();

    // Manual command controls
    QHBoxLayout* commandLayout = new QHBoxLayout();

    QLabel* commandLabel = new QLabel("Manual Command:");
    commandLineEdit = new QLineEdit();
    commandLineEdit->setPlaceholderText("Enter command (e.g., 21 12 for engine misc data)");
    commandLineEdit->setMinimumHeight(40);

    sendCommandButton = new QPushButton("Send");
    sendCommandButton->setMinimumHeight(40);
    sendCommandButton->setMinimumWidth(80);
    sendCommandButton->setEnabled(false);

    commandLayout->addWidget(commandLabel);
    commandLayout->addWidget(commandLineEdit, 1);
    commandLayout->addWidget(sendCommandButton);

    multiModuleLayout->addLayout(multiModuleButtonLayout);
    multiModuleLayout->addWidget(progressLabel);
    multiModuleLayout->addWidget(progressBar);
    multiModuleLayout->addWidget(faultCodeTree);
    multiModuleLayout->addLayout(continuousLayout);
    multiModuleLayout->addLayout(commandLayout);
}

void MainWindow::setupTerminalDisplay() {
    terminalDisplay = new QTextBrowser();
    terminalDisplay->setMinimumHeight(200);
    terminalDisplay->setMaximumHeight(250);
    terminalDisplay->setFont(QFont("Consolas", 10));
}

// Setup Connections
void MainWindow::setupConnections() {
    // Connection type selection
    connect(connectionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onConnectionTypeChanged);

    // Bluetooth device selection
    connect(bluetoothDevicesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onBluetoothDeviceSelected);

    // Scan bluetooth button
    connect(scanBluetoothButton, &QPushButton::clicked, this, &MainWindow::onScanBluetoothClicked);

    // Protocol and module management
    connect(protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                WJProtocol protocol = PROTOCOL_AUTO_DETECT;
                if (index == 1) protocol = PROTOCOL_ISO9141_2;
                else if (index == 2) protocol = PROTOCOL_J1850_VPW;
                onProtocolSwitchRequested(protocol);
            });

    connect(moduleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModuleSelectionChanged);

    connect(autoDetectProtocolButton, &QPushButton::clicked, this, &MainWindow::onAutoDetectProtocolClicked);
    connect(switchProtocolButton, &QPushButton::clicked, this, [this]() {
        int index = protocolCombo->currentIndex();
        WJProtocol protocol = PROTOCOL_AUTO_DETECT;
        if (index == 1) protocol = PROTOCOL_ISO9141_2;
        else if (index == 2) protocol = PROTOCOL_J1850_VPW;
        onProtocolSwitchRequested(protocol);
    });

    // Connection management
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(clearTerminalButton, &QPushButton::clicked, this, &MainWindow::onClearTerminalClicked);
    connect(exitButton, &QPushButton::clicked, this, &MainWindow::onExitClicked);

    // Engine diagnostic commands
    connect(readEngineMAFButton, &QPushButton::clicked, this, &MainWindow::onReadEngineMAFClicked);
    connect(readEngineRailPressureButton, &QPushButton::clicked, this, &MainWindow::onReadEngineRailPressureClicked);
    connect(readEngineMAPButton, &QPushButton::clicked, this, &MainWindow::onReadEngineMAPClicked);
    connect(readEngineInjectorButton, &QPushButton::clicked, this, &MainWindow::onReadEngineInjectorCorrectionsClicked);
    connect(readEngineMiscButton, &QPushButton::clicked, this, &MainWindow::onReadEngineMiscDataClicked);
    connect(readEngineBatteryButton, &QPushButton::clicked, this, &MainWindow::onReadEngineBatteryVoltageClicked);
    connect(readEngineAllSensorsButton, &QPushButton::clicked, this, &MainWindow::onReadEngineAllSensorsClicked);
    connect(readEngineFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onReadEngineFaultCodesClicked);
    connect(clearEngineFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onClearEngineFaultCodesClicked);

    // Transmission diagnostic commands
    connect(readTransmissionDataButton, &QPushButton::clicked, this, &MainWindow::onReadTransmissionDataClicked);
    connect(readTransmissionSolenoidsButton, &QPushButton::clicked, this, &MainWindow::onReadTransmissionSolenoidsClicked);
    connect(readTransmissionSpeedsButton, &QPushButton::clicked, this, &MainWindow::onReadTransmissionSpeedsClicked);
    connect(readTransmissionFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onReadTransmissionFaultCodesClicked);
    connect(clearTransmissionFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onClearTransmissionFaultCodesClicked);

    // PCM diagnostic commands
    connect(readPCMDataButton, &QPushButton::clicked, this, &MainWindow::onReadPCMDataClicked);
    connect(readPCMFuelTrimButton, &QPushButton::clicked, this, &MainWindow::onReadPCMFuelTrimClicked);
    connect(readPCMO2SensorsButton, &QPushButton::clicked, this, &MainWindow::onReadPCMO2SensorsClicked);
    connect(readPCMFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onReadPCMFaultCodesClicked);
    connect(clearPCMFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onClearPCMFaultCodesClicked);

    // ABS diagnostic commands
    connect(readABSWheelSpeedsButton, &QPushButton::clicked, this, &MainWindow::onReadABSWheelSpeedsClicked);
    connect(readABSStabilityDataButton, &QPushButton::clicked, this, &MainWindow::onReadABSStabilityDataClicked);
    connect(readABSFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onReadABSFaultCodesClicked);
    connect(clearABSFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onClearABSFaultCodesClicked);

    // Multi-module operations
    connect(readAllModuleFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onReadAllModuleFaultCodesClicked);
    connect(clearAllModuleFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onClearAllModuleFaultCodesClicked);
    connect(readAllSensorDataButton, &QPushButton::clicked, this, &MainWindow::onReadAllSensorDataClicked);

    // Manual command and continuous reading
    connect(sendCommandButton, &QPushButton::clicked, this, &MainWindow::onSendCommandClicked);
    connect(continuousReadingCheckBox, &QCheckBox::toggled, this, &MainWindow::onContinuousReadingToggled);
    connect(readingIntervalSlider, &QSlider::valueChanged, this, &MainWindow::onReadingIntervalChanged);
    connect(commandLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendCommandClicked);

    // Tab management
    connect(moduleTabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    // Connection manager signals
    if (connectionManager) {
        connect(connectionManager, &ConnectionManager::connected, this, &MainWindow::onConnected);
        connect(connectionManager, &ConnectionManager::disconnected, this, &MainWindow::onDisconnected);
        connect(connectionManager, &ConnectionManager::dataReceived, this, &MainWindow::onDataReceived);
        connect(connectionManager, &ConnectionManager::stateChanged, this, &MainWindow::onConnectionStateChanged);

        // Bluetooth-specific signals
        connect(connectionManager, &ConnectionManager::bluetoothDeviceFound,
                this, &MainWindow::onBluetoothDeviceFound);
        connect(connectionManager, &ConnectionManager::bluetoothDiscoveryCompleted,
                this, &MainWindow::onBluetoothDiscoveryCompleted);
    }
}

// Apply WJ Styling
void MainWindow::applyWJStyling() {
    // Enhanced WJ color scheme - professional automotive look
    const QString PRIMARY_COLOR = "#1E3A8A";      // Deep blue
    const QString SECONDARY_COLOR = "#1E40AF";    // Blue
    const QString ACCENT_COLOR = "#3B82F6";       // Light blue
    const QString SUCCESS_COLOR = "#059669";      // Green
    const QString WARNING_COLOR = "#D97706";      // Orange
    const QString DANGER_COLOR = "#DC2626";       // Red
    const QString TEXT_COLOR = "#F8FAFC";         // Light text
    const QString BACKGROUND_COLOR = "#0F172A";   // Dark background
    const QString SURFACE_COLOR = "#1E293B";      // Surface color

    // Main window style
    setStyleSheet(QString(
                      "QMainWindow {"
                      "    background-color: %1;"
                      "    color: %2;"
                      "}"
                      "QLabel {"
                      "    color: %2;"
                      "    font-weight: bold;"
                      "    font-size: 12pt;"
                      "}"
                      "QPushButton {"
                      "    background-color: %3;"
                      "    color: %2;"
                      "    border: 1px solid %4;"
                      "    border-radius: 6px;"
                      "    padding: 8px 16px;"
                      "    font-weight: bold;"
                      "    font-size: 11pt;"
                      "}"
                      "QPushButton:hover {"
                      "    background-color: %4;"
                      "}"
                      "QPushButton:pressed {"
                      "    background-color: %1;"
                      "}"
                      "QPushButton:disabled {"
                      "    background-color: #374151;"
                      "    color: #9CA3AF;"
                      "}"
                      "QLineEdit, QComboBox {"
                      "    background-color: %8;"
                      "    color: %2;"
                      "    border: 1px solid %3;"
                      "    border-radius: 4px;"
                      "    padding: 8px;"
                      "    font-size: 11pt;"
                      "}"
                      "QComboBox::drop-down {"
                      "    border: none;"
                      "    width: 30px;"
                      "}"
                      "QComboBox::down-arrow {"
                      "    width: 16px;"
                      "    height: 16px;"
                      "}"
                      "QComboBox QAbstractItemView {"
                      "    background-color: %8;"
                      "    border: 1px solid %3;"
                      "    border-radius: 6px;"
                      "    selection-background-color: %3;"
                      "    color: %2;"
                      "    padding: 4px;"
                      "}"
                      "QTextBrowser {"
                      "    background-color: %8;"
                      "    color: %2;"
                      "    border: 1px solid %3;"
                      "    border-radius: 4px;"
                      "    font-family: 'Consolas', monospace;"
                      "    font-size: 10pt;"
                      "}"
                      "QGroupBox {"
                      "    font-weight: bold;"
                      "    border: 2px solid %3;"
                      "    border-radius: 8px;"
                      "    margin-top: 1ex;"
                      "    padding-top: 10px;"
                      "}"
                      "QGroupBox::title {"
                      "    subcontrol-origin: margin;"
                      "    left: 10px;"
                      "    padding: 0 5px 0 5px;"
                      "}"
                      "QTabWidget::pane {"
                      "    border: 1px solid %3;"
                      "    border-radius: 4px;"
                      "}"
                      "QTabBar::tab {"
                      "    background-color: %8;"
                      "    color: %2;"
                      "    padding: 8px 16px;"
                      "    margin-right: 2px;"
                      "    border-top-left-radius: 4px;"
                      "    border-top-right-radius: 4px;"
                      "}"
                      "QTabBar::tab:selected {"
                      "    background-color: %3;"
                      "}"
                      "QCheckBox {"
                      "    font-weight: bold;"
                      "    font-size: 11pt;"
                      "}"
                      "QSlider::groove:horizontal {"
                      "    border: 1px solid %3;"
                      "    height: 8px;"
                      "    background: %8;"
                      "    border-radius: 4px;"
                      "}"
                      "QSlider::handle:horizontal {"
                      "    background: %4;"
                      "    border: 1px solid %3;"
                      "    width: 18px;"
                      "    height: 18px;"
                      "    border-radius: 9px;"
                      "    margin: -5px 0;"
                      "}"
                      "QTreeWidget {"
                      "    background-color: %8;"
                      "    color: %2;"
                      "    border: 1px solid %3;"
                      "    border-radius: 4px;"
                      "    font-size: 10pt;"
                      "}"
                      "QProgressBar {"
                      "    border: 1px solid %3;"
                      "    border-radius: 4px;"
                      "    background-color: %8;"
                      "}"
                      "QProgressBar::chunk {"
                      "    background-color: %4;"
                      "    border-radius: 2px;"
                      "}"
                      ).arg(BACKGROUND_COLOR, TEXT_COLOR, PRIMARY_COLOR, ACCENT_COLOR,
                           SUCCESS_COLOR, WARNING_COLOR, DANGER_COLOR, SURFACE_COLOR));

    // Special styling for diagnostic buttons by module
    QString engineButtonStyle = QString(
                                    "QPushButton {"
                                    "    background-color: %1;"
                                    "    min-width: 140px;"
                                    "}"
                                    ).arg(SUCCESS_COLOR);

    QString transmissionButtonStyle = QString(
                                          "QPushButton {"
                                          "    background-color: %1;"
                                          "    min-width: 140px;"
                                          "}"
                                          ).arg("#8B5A2B"); // Brown for transmission

    QString pcmButtonStyle = QString(
                                 "QPushButton {"
                                 "    background-color: %1;"
                                 "    min-width: 140px;"
                                 "}"
                                 ).arg("#6B46C1"); // Purple for PCM

    QString absButtonStyle = QString(
                                 "QPushButton {"
                                 "    background-color: %1;"
                                 "    min-width: 140px;"
                                 "}"
                                 ).arg("#DC2626"); // Red for ABS

    QString faultButtonStyle = QString(
                                   "QPushButton {"
                                   "    background-color: %1;"
                                   "    min-width: 140px;"
                                   "}"
                                   ).arg(WARNING_COLOR);

    QString clearFaultButtonStyle = QString(
                                        "QPushButton {"
                                        "    background-color: %1;"
                                        "    min-width: 140px;"
                                        "}"
                                        ).arg(DANGER_COLOR);

    // Apply engine button styles
    QList<QPushButton*> engineButtons = {
        readEngineMAFButton, readEngineRailPressureButton, readEngineMAPButton,
        readEngineInjectorButton, readEngineMiscButton, readEngineBatteryButton,
        readEngineAllSensorsButton
    };
    for (auto* button : engineButtons) {
        button->setStyleSheet(engineButtonStyle);
    }

    // Apply transmission button styles
    QList<QPushButton*> transmissionButtons = {
        readTransmissionDataButton, readTransmissionSolenoidsButton, readTransmissionSpeedsButton
    };
    for (auto* button : transmissionButtons) {
        button->setStyleSheet(transmissionButtonStyle);
    }

    // Apply PCM button styles
    QList<QPushButton*> pcmButtons = {
        readPCMDataButton, readPCMFuelTrimButton, readPCMO2SensorsButton
    };
    for (auto* button : pcmButtons) {
        button->setStyleSheet(pcmButtonStyle);
    }

    // Apply ABS button styles
    QList<QPushButton*> absButtons = {
        readABSWheelSpeedsButton, readABSStabilityDataButton
    };
    for (auto* button : absButtons) {
        button->setStyleSheet(absButtonStyle);
    }

    // Apply fault code button styles
    QList<QPushButton*> faultButtons = {
        readEngineFaultCodesButton, readTransmissionFaultCodesButton,
        readPCMFaultCodesButton, readABSFaultCodesButton, readAllModuleFaultCodesButton
    };
    for (auto* button : faultButtons) {
        button->setStyleSheet(faultButtonStyle);
    }

    QList<QPushButton*> clearFaultButtons = {
        clearEngineFaultCodesButton, clearTransmissionFaultCodesButton,
        clearPCMFaultCodesButton, clearABSFaultCodesButton, clearAllModuleFaultCodesButton
    };
    for (auto* button : clearFaultButtons) {
        button->setStyleSheet(clearFaultButtonStyle);
    }

    // Connect button styling
    connectButton->setStyleSheet(QString(
                                     "QPushButton {"
                                     "    background-color: %1;"
                                     "    font-size: 12pt;"
                                     "    min-width: 150px;"
                                     "}"
                                     ).arg(SUCCESS_COLOR));
}

// Connection Type Management
void MainWindow::onConnectionTypeChanged(int index) {
    if (!connectionManager) return;

    if (index == 0) { // WiFi
        connectionManager->setConnectionType(Wifi);
        btDeviceLabel->setVisible(false);
        bluetoothDevicesCombo->setVisible(false);
        scanBluetoothButton->setVisible(false);

        logWJData("→ Connection type set to WiFi");
        if (settingsManager) {
            logWJData("→ WiFi target: " + settingsManager->getWifiIp() + ":" +
                     QString::number(settingsManager->getWifiPort()));
        }
    }
    else if (index == 1) { // Bluetooth
        connectionManager->setConnectionType(BlueTooth);
        btDeviceLabel->setVisible(true);
        bluetoothDevicesCombo->setVisible(true);
        scanBluetoothButton->setVisible(true);

        logWJData("→ Connection type set to Bluetooth");

#ifdef Q_OS_ANDROID
        requestBluetoothPermissions();
#else
        // Auto-scan on desktop platforms
        scanBluetoothDevices();
#endif
    }
}

void MainWindow::onScanBluetoothClicked() {
    if (connectionTypeCombo->currentIndex() == 1) { // Bluetooth selected
        logWJData("→ Scanning for Bluetooth devices...");
        scanBluetoothDevices();
    }
}

void MainWindow::scanBluetoothDevices() {
    // Clear previous devices
    bluetoothDevicesCombo->clear();
    bluetoothDevicesCombo->addItem("Select device...");
    deviceAddressMap.clear();

    // Process events to make sure UI updates before starting scan
    QCoreApplication::processEvents();

    // Start scanning
    if (connectionManager) {
        connectionManager->startBluetoothDiscovery();
    }
}

void MainWindow::onBluetoothDeviceFound(const QString& name, const QString& address) {
    // Add device to combo box and map
    int index = bluetoothDevicesCombo->count();
    bluetoothDevicesCombo->addItem(name + " (" + address + ")");
    deviceAddressMap[index] = address;

    // Always auto-select the latest found device
    bluetoothDevicesCombo->setCurrentIndex(index);
    logWJData("→ Auto-selected device: " + name + " (" + address + ")");

    // Display in terminal
    logWJData("→ Found device: " + name + " (" + address + ")");

    // Process events to update UI immediately
    QCoreApplication::processEvents();
}

void MainWindow::onBluetoothDiscoveryCompleted() {
    if (bluetoothDevicesCombo->count() <= 1) {
        logWJData("→ No Bluetooth devices found");
    } else {
        logWJData("→ Found " + QString::number(bluetoothDevicesCombo->count() - 1) + " Bluetooth devices");
    }

    // Process events to update UI
    QCoreApplication::processEvents();
}

void MainWindow::onBluetoothDeviceSelected(int index) {
    if (index <= 0) {
        return; // "Select device..." option
    }

    if (deviceAddressMap.contains(index)) {
        QString selectedAddress = deviceAddressMap[index];
        logWJData("→ Selected device with address: " + selectedAddress);
    }
}

QString MainWindow::getSelectedBluetoothDeviceAddress() const {
    int index = bluetoothDevicesCombo->currentIndex();
    if (index > 0 && deviceAddressMap.contains(index)) {
        return deviceAddressMap[index];
    }
    return QString();
}

// Continuation of MainWindow.cpp - Remaining Implementation

#ifdef Q_OS_ANDROID
void MainWindow::requestBluetoothPermissions() {
    QBluetoothPermission bluetoothPermission;
    bluetoothPermission.setCommunicationModes(QBluetoothPermission::Access);

    switch (qApp->checkPermission(bluetoothPermission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(bluetoothPermission, this,
                                [this](const QPermission &permission) {
                                    if (qApp->checkPermission(permission) == Qt::PermissionStatus::Granted) {
                                        scanBluetoothDevices();
                                    } else {
                                        logWJData("❌ Bluetooth permission denied. Cannot proceed.");
                                    }
                                });
        break;
    case Qt::PermissionStatus::Granted:
        scanBluetoothDevices();
        break;
    case Qt::PermissionStatus::Denied:
        logWJData("❌ Bluetooth permission denied. Please enable in Settings.");
        break;
    }

    // Also check for location permission which is required for Bluetooth scanning
    QLocationPermission locationPermission;
    locationPermission.setAccuracy(QLocationPermission::Approximate);

    switch (qApp->checkPermission(locationPermission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(locationPermission, this,
                                [this](const QPermission &permission) {
                                    if (qApp->checkPermission(permission) != Qt::PermissionStatus::Granted) {
                                        logWJData("⚠️ Location permission denied. Bluetooth scanning may not work properly.");
                                    }
                                });
        break;
    case Qt::PermissionStatus::Granted:
        break;
    case Qt::PermissionStatus::Denied:
        logWJData("⚠️ Location permission denied. Bluetooth scanning may not work properly.");
        break;
    }
}
#endif

// Protocol and Module Management
void MainWindow::onProtocolSwitchRequested(WJProtocol protocol) {
    if (!connected) {
        logWJData("❌ Not connected - cannot switch protocol");
        return;
    }

    if (switchToProtocol(protocol)) {
        currentProtocol = protocol;
        currentProtocolLabel->setText("Current: " + WJUtils::getProtocolName(protocol));
        logWJData("✓ Switched to protocol: " + WJUtils::getProtocolName(protocol));
    } else {
        logWJData("❌ Failed to switch to protocol: " + WJUtils::getProtocolName(protocol));
    }
}

void MainWindow::onModuleSelectionChanged(int index) {
    if (!connected) return;

    WJModule module = MODULE_UNKNOWN;
    switch (index) {
        case 0: module = MODULE_ENGINE_EDC15; break;
        case 1: module = MODULE_TRANSMISSION; break;
        case 2: module = MODULE_PCM; break;
        case 3: module = MODULE_ABS; break;
        default: return;
    }

    if (switchToModule(module)) {
        currentModule = module;
        currentModuleLabel->setText("Current: " + WJUtils::getModuleName(module));
        updateUIForCurrentModule();

        // Switch to appropriate tab
        moduleTabWidget->setCurrentIndex(index);

        logWJData("✓ Switched to module: " + WJUtils::getModuleName(module));
    } else {
        logWJData("❌ Failed to switch to module: " + WJUtils::getModuleName(module));
    }
}

void MainWindow::onAutoDetectProtocolClicked() {
    if (!connected) {
        logWJData("❌ Not connected - cannot detect protocols");
        return;
    }

    logWJData("→ Auto-detecting available protocols...");

    // Try ISO 9141-2 first (engine)
    if (switchToProtocol(PROTOCOL_ISO9141_2)) {
        logWJData("✓ ISO 9141-2 protocol detected and available");
        currentProtocol = PROTOCOL_ISO9141_2;
        protocolCombo->setCurrentIndex(1);
    }

    // Try J1850 VPW (other modules)
    QTimer::singleShot(1000, [this]() {
        if (switchToProtocol(PROTOCOL_J1850_VPW)) {
            logWJData("✓ J1850 VPW protocol detected and available");
        }
        logWJData("→ Protocol detection completed");
    });
}

bool MainWindow::switchToProtocol(WJProtocol protocol) {
    if (!connected || !connectionManager) {
        return false;
    }

    if (currentProtocol == protocol) {
        return true; // Already on this protocol
    }

    protocolSwitchingInProgress = true;

    // Get protocol switch commands
    QList<WJCommand> switchCommands = WJCommands::getProtocolSwitchCommands(currentProtocol, protocol);

    for (const WJCommand& cmd : switchCommands) {
        sendWJCommand(cmd.command);
        QThread::msleep(cmd.timeoutMs / 10); // Short delay between commands
    }

    protocolSwitchingInProgress = false;
    return true;
}

bool MainWindow::switchToModule(WJModule module) {
    if (!connected) {
        return false;
    }

    // Switch protocol if needed
    WJProtocol requiredProtocol = WJUtils::getProtocolFromModule(module);
    if (!switchToProtocol(requiredProtocol)) {
        return false;
    }

    // Get module initialization commands
    QList<WJCommand> moduleCommands = WJCommands::getModuleInitCommands(module);

    for (const WJCommand& cmd : moduleCommands) {
        sendWJCommand(cmd.command, module);
        QThread::msleep(cmd.timeoutMs / 20); // Short delay
    }

    currentModule = module;
    return true;
}

void MainWindow::updateUIForCurrentModule() {
    // Enable/disable controls based on current module
    enableDisableControlsForModule(MODULE_ENGINE_EDC15, currentModule == MODULE_ENGINE_EDC15);
    enableDisableControlsForModule(MODULE_TRANSMISSION, currentModule == MODULE_TRANSMISSION);
    enableDisableControlsForModule(MODULE_PCM, currentModule == MODULE_PCM);
    enableDisableControlsForModule(MODULE_ABS, currentModule == MODULE_ABS);
}

void MainWindow::enableDisableControlsForModule(WJModule module, bool enabled) {
    switch (module) {
        case MODULE_ENGINE_EDC15: {
            QList<QPushButton*> engineButtons = {
                readEngineMAFButton, readEngineRailPressureButton, readEngineMAPButton,
                readEngineInjectorButton, readEngineMiscButton, readEngineBatteryButton,
                readEngineAllSensorsButton, readEngineFaultCodesButton, clearEngineFaultCodesButton
            };
            for (auto* button : engineButtons) {
                button->setEnabled(enabled && connected && initialized);
            }
            break;
        }
        case MODULE_TRANSMISSION: {
            QList<QPushButton*> transmissionButtons = {
                readTransmissionDataButton, readTransmissionSolenoidsButton, readTransmissionSpeedsButton,
                readTransmissionFaultCodesButton, clearTransmissionFaultCodesButton
            };
            for (auto* button : transmissionButtons) {
                button->setEnabled(enabled && connected && initialized);
            }
            break;
        }
        case MODULE_PCM: {
            QList<QPushButton*> pcmButtons = {
                readPCMDataButton, readPCMFuelTrimButton, readPCMO2SensorsButton,
                readPCMFaultCodesButton, clearPCMFaultCodesButton
            };
            for (auto* button : pcmButtons) {
                button->setEnabled(enabled && connected && initialized);
            }
            break;
        }
        case MODULE_ABS: {
            QList<QPushButton*> absButtons = {
                readABSWheelSpeedsButton, readABSStabilityDataButton,
                readABSFaultCodesButton, clearABSFaultCodesButton
            };
            for (auto* button : absButtons) {
                button->setEnabled(enabled && connected && initialized);
            }
            break;
        }
        default:
            break;
    }
}

// Connection Management
void MainWindow::onConnectClicked() {
    if (!connected) {
        connectToWJ();
    }
}

void MainWindow::onDisconnectClicked() {
    disconnectFromWJ();
}

bool MainWindow::connectToWJ() {
    if (connected) {
        return true;
    }

    logWJData("→ Attempting to connect to Jeep WJ...");

    if (!connectionManager) {
        logWJData("❌ Connection manager not available");
        return false;
    }

    // Set connection type based on UI selection
    if (connectionTypeCombo->currentIndex() == 0) {
        connectionManager->setConnectionType(Wifi);
        logWJData("→ Using WiFi connection");
    } else {
        connectionManager->setConnectionType(BlueTooth);
        logWJData("→ Using Bluetooth connection");

        // Get selected Bluetooth device address
        QString deviceAddress = getSelectedBluetoothDeviceAddress();
        if (deviceAddress.isEmpty()) {
            logWJData("❌ No Bluetooth device selected");
            return false;
        }
        logWJData("→ Target device: " + deviceAddress);
    }

    // Start connection
    if (connectionTypeCombo->currentIndex() == 1) { // Bluetooth
        QString deviceAddress = getSelectedBluetoothDeviceAddress();
        if (!deviceAddress.isEmpty()) {
            connectionManager->connectElm(deviceAddress);
        } else {
            connectionManager->connectElm(); // Will start discovery
        }
    } else {
        connectionManager->connectElm(); // WiFi connection
    }

    connectButton->setEnabled(false);
    connectionStatusLabel->setText("Status: Connecting...");
    initializationTimer->start();

    return true;
}

void MainWindow::disconnectFromWJ() {
    if (!connected) {
        return;
    }

    stopContinuousReading();

    if (connectionManager) {
        connectionManager->disConnectElm();
    }

    connected = false;
    initialized = false;
    currentInitState = STATE_DISCONNECTED;
    currentProtocol = PROTOCOL_UNKNOWN;
    currentModule = MODULE_UNKNOWN;

    connectButton->setEnabled(true);
    disconnectButton->setEnabled(false);
    connectionStatusLabel->setText("Status: Disconnected");
    protocolLabel->setText("Protocol: Unknown");
    currentProtocolLabel->setText("Current: Unknown");
    currentModuleLabel->setText("Current: None");

    // Disable all diagnostic buttons
    enableDisableControlsForModule(MODULE_ENGINE_EDC15, false);
    enableDisableControlsForModule(MODULE_TRANSMISSION, false);
    enableDisableControlsForModule(MODULE_PCM, false);
    enableDisableControlsForModule(MODULE_ABS, false);

    // Disable multi-module buttons
    readAllModuleFaultCodesButton->setEnabled(false);
    clearAllModuleFaultCodesButton->setEnabled(false);
    readAllSensorDataButton->setEnabled(false);
    sendCommandButton->setEnabled(false);

    logWJData("✗ Disconnected from Jeep WJ");
}

void MainWindow::resetWJConnection() {
    if (connected) {
        disconnectFromWJ();
        QTimer::singleShot(2000, [this]() {
            connectToWJ();
        });
    }
}

void MainWindow::onConnected() {
    connected = true;
    disconnectButton->setEnabled(true);
    connectionStatusLabel->setText("Status: Connected - Initializing...");
    logWJData("✓ Physical connection established");

    // Start WJ initialization
    if (!initializeWJCommunication()) {
        logWJData("❌ Failed to start WJ initialization");
        disconnectFromWJ();
    }
}

void MainWindow::onDisconnected() {
    disconnectFromWJ();
}

void MainWindow::onConnectionStateChanged(const QString& state) {
    logWJData("→ Connection state: " + state);
    if(state.contains("not connected"))
        connectButton->setEnabled(true);
}

// WJ Communication Methods
bool MainWindow::initializeWJCommunication() {
    if (!connected || initializationCommands.isEmpty()) {
        return false;
    }

    currentInitStep = 0;
    currentInitState = STATE_CONNECTING;
    initialized = false;
    engineSecurityAccessGranted = false;

    logWJData("→ Starting WJ multi-protocol initialization...");
    logWJData("→ Target: Jeep Grand Cherokee WJ 2.7 CRD (All Modules)");

    // Start with engine module (ISO 9141-2) initialization
    const WJCommand& firstCmd = initializationCommands[0];
    logWJData("→ " + firstCmd.description + ": " + firstCmd.command);

    lastSentCommand = firstCmd.command;
    connectionManager->send(firstCmd.command);

    return true;
}

void MainWindow::processWJInitResponse(const QString& response) {
    if (currentInitStep >= initializationCommands.size()) {
        // Initialization complete
        currentInitState = STATE_READY_ISO9141;
        initialized = true;
        connectionStatusLabel->setText("Status: Ready (Multi-Protocol)");

        logWJData("✓ WJ multi-protocol initialization completed!");
        logWJData(QString("→ Engine security access: %1").arg(engineSecurityAccessGranted ? "Granted" : "Limited"));
        logWJData("→ Ready for multi-module diagnostics");

        // Enable all diagnostic buttons
        enableDisableControlsForModule(MODULE_ENGINE_EDC15, true);
        enableDisableControlsForModule(MODULE_TRANSMISSION, true);
        enableDisableControlsForModule(MODULE_PCM, true);
        enableDisableControlsForModule(MODULE_ABS, true);

        // Enable multi-module buttons
        readAllModuleFaultCodesButton->setEnabled(true);
        clearAllModuleFaultCodesButton->setEnabled(true);
        readAllSensorDataButton->setEnabled(true);
        sendCommandButton->setEnabled(true);
        switchProtocolButton->setEnabled(true);

        initializationTimer->stop();

        // Set initial protocol and module
        currentProtocol = PROTOCOL_ISO9141_2;
        currentModule = MODULE_ENGINE_EDC15;
        currentProtocolLabel->setText("Current: " + WJUtils::getProtocolName(currentProtocol));
        currentModuleLabel->setText("Current: " + WJUtils::getModuleName(currentModule));
        protocolLabel->setText("Protocol: Multi-Protocol Ready");

        // Test communication with engine
        QTimer::singleShot(1000, [this]() {
            onReadEngineBatteryVoltageClicked();
        });

        return;
    }

    const WJCommand& currentCmd = initializationCommands[currentInitStep];
    QString cleanResponse = removeCommandEcho(response).trimmed().toUpper();
    bool responseOK = false;
    bool shouldAdvanceStep = true;

    // Handle delayed "no data" responses that don't belong to current command
    if ((cleanResponse == "." || cleanResponse.isEmpty()) &&
        !currentCmd.command.startsWith("27") &&
        !currentCmd.command.startsWith("31") &&
        currentCmd.command != "81") {
        logWJData("← Delayed response ignored: " + (cleanResponse.isEmpty() ? "empty" : cleanResponse));
        return; // Don't advance to next command
    }

    // Handle STOPPED errors first
    if (isWJError(cleanResponse) && cleanResponse.contains("STOPPED")) {
        logWJData("❌ ECU rejected command: " + currentCmd.command + " - " + cleanResponse);

        // For security access commands, try to recover
        if (currentCmd.command.startsWith("27")) {
            logWJData("→ Security access blocked - continuing with limited access");
            engineSecurityAccessGranted = false;
            responseOK = true; // Continue with limited access
        } else if (currentCmd.command.startsWith("31")) {
            logWJData("→ Diagnostic routine blocked - continuing anyway");
            responseOK = true; // Continue anyway
        } else {
            responseOK = false;
        }
    }
    // Normal response processing
    else if (currentCmd.command == "ATZ") {
        currentInitState = STATE_RESETTING;
        if (cleanResponse.contains("ELM327") || cleanResponse.isEmpty()) {
            logWJData("✓ ELM327 reset successful: " + (cleanResponse.isEmpty() ? "OK" : cleanResponse));
            responseOK = true;
        }
    }
    else if (currentCmd.command == "ATE0") {
        if (cleanResponse.contains("ELM327") || cleanResponse.contains("OK")) {
            logWJData("✓ Echo off - " + (cleanResponse.isEmpty() ? "OK" : cleanResponse));
            responseOK = true;
        }
    }
    else if (currentCmd.command == "ATSP3") {
        currentInitState = STATE_CONFIGURING_PROTOCOL;
        if (cleanResponse.contains("OK")) {
            logWJData("✓ Protocol set to ISO 9141-2");
            responseOK = true;
        }
    }
    else if (currentCmd.command.startsWith("ATWM")) {
        if (cleanResponse.contains("OK")) {
            logWJData("✓ Wakeup message configured");
            responseOK = true;
        }
    }
    else if (currentCmd.command.startsWith("ATSH")) {
        currentInitState = STATE_SETTING_HEADER;
        if (cleanResponse.contains("OK")) {
            logWJData("✓ ECU header set to " + WJ_ECU_HEADER_ENGINE);
            currentECUHeader = WJ_ECU_HEADER_ENGINE;
            responseOK = true;
        }
    }
    else if (currentCmd.command == "ATWS" || currentCmd.command == "ATFI") {
        currentInitState = STATE_FAST_INIT;
        if (cleanResponse.contains("ELM327") || cleanResponse.contains("BUS INIT") || cleanResponse.contains("OK")) {
            logWJData("✓ Bus initialization successful: " + cleanResponse);
            responseOK = true;
        }
    }
    else if (currentCmd.command == "ATDP") {
        if (cleanResponse.contains("ISO") || cleanResponse.contains("9141")) {
            logWJData("✓ Protocol verified: " + cleanResponse);
            responseOK = true;
        }
    }
    else if (currentCmd.command == "81") {
        currentInitState = STATE_START_COMMUNICATION;
        if (cleanResponse.contains("BUS INIT") || cleanResponse.contains("C1") || cleanResponse.length() >= 4) {
            logWJData("✓ Engine communication established: " + cleanResponse);
            responseOK = true;
        }
        // For start communication, ignore delayed "." responses
        else if (cleanResponse == ".") {
            logWJData("← Delayed no-data response from start communication");
            shouldAdvanceStep = false; // Don't advance, wait for real response
            return;
        }
    }
    else if (currentCmd.command == "27 01") {
        currentInitState = STATE_SECURITY_ACCESS;
        if (cleanResponse.contains("67 01") || cleanResponse.startsWith("67")) {
            logWJData("✓ Security access requested: " + cleanResponse);
            responseOK = true;
        } else if (cleanResponse == ".") {
            logWJData("✓ Security access request sent (no immediate response)");
            responseOK = true;
        } else if (cleanResponse.contains("7F")) {
            logWJData("⚠️ Security access request denied: " + cleanResponse);
            responseOK = true; // Continue anyway
        }
    }
    else if (currentCmd.command == "27 02 CD 46") {
        if (cleanResponse.contains("67 02")) {
            logWJData("✓ Security access granted: " + cleanResponse);
            engineSecurityAccessGranted = true;
            responseOK = true;
        } else if (cleanResponse.contains("7F 27")) {
            logWJData("⚠️ Security access denied (expected): " + cleanResponse);
            engineSecurityAccessGranted = false;
            responseOK = true; // This is often expected
        } else if (cleanResponse.contains("BUS INIT") || cleanResponse == ".") {
            logWJData("⚠️ Security access response: " + (cleanResponse == "." ? "No data" : cleanResponse));
            engineSecurityAccessGranted = false;
            responseOK = true; // Continue anyway
        }
    }
    else if (currentCmd.command == "31 25 00") {
        currentInitState = STATE_DIAGNOSTIC_ROUTINE;
        if (cleanResponse.contains("71 25") || cleanResponse.startsWith("71")) {
            logWJData("✓ Diagnostic routine started: " + cleanResponse);
            responseOK = true;
        } else if (cleanResponse == ".") {
            logWJData("✓ Diagnostic routine sent (no immediate response)");
            responseOK = true;
        } else if (cleanResponse.contains("7F")) {
            logWJData("⚠️ Diagnostic routine denied: " + cleanResponse);
            responseOK = true; // Continue anyway
        }
    }
    else {
        // Standard commands expecting OK
        if (cleanResponse.contains("OK") || cleanResponse.isEmpty()) {
            logWJData("✓ " + currentCmd.description + " - OK");
            responseOK = true;
        }
    }

    if (!responseOK && !isWJError(cleanResponse)) {
        logWJData("⚠️ Unexpected response for " + currentCmd.command + ": " + cleanResponse);
        // Continue anyway for most commands
        responseOK = true;
    }

    // Only advance if we should and response was OK
    if (shouldAdvanceStep && responseOK) {
        // Move to next command
        currentInitStep++;

        if (currentInitStep < initializationCommands.size()) {
            const WJCommand& nextCmd = initializationCommands[currentInitStep];

            // Calculate delay - add extra time for critical commands
            int delay = nextCmd.timeoutMs;
            if (nextCmd.command.startsWith("27") || nextCmd.command.startsWith("31")) {
                delay += 1000; // Extra delay for security/diagnostic commands
            }

            QTimer::singleShot(delay, [this, nextCmd]() {
                if (connected && connectionManager) {
                    logWJData("→ " + nextCmd.description + ": " + nextCmd.command);
                    lastSentCommand = nextCmd.command;
                    connectionManager->send(nextCmd.command);
                }
            });
        }
    } else if (!shouldAdvanceStep) {
        // Log that we're waiting for the real response
        logWJData("→ Waiting for response to: " + currentCmd.command);
    }
}

void MainWindow::onInitializationTimeout() {
    logWJData("❌ WJ initialization timeout");
    logWJData("→ Current state: " + QString::number(currentInitState));
    logWJData("→ Current step: " + QString::number(currentInitStep));

    if (currentInitStep < initializationCommands.size()) {
        logWJData("→ Failed at: " + initializationCommands[currentInitStep].command);
    }

    disconnectFromWJ();
}

void MainWindow::sendWJCommand(const QString& command, WJModule targetModule) {
    if (!connected || !connectionManager) {
        return;
    }

    QString cleanCommand = cleanWJData(command);

    // Switch to target module if specified
    if (targetModule != MODULE_UNKNOWN && targetModule != currentModule) {
        if (!switchToModule(targetModule)) {
            logWJData("❌ Failed to switch to target module");
            return;
        }
    }

    // Set appropriate ECU header based on current module
    if (!command.startsWith("AT") && !currentECUHeader.isEmpty()) {
        QString headerCommand = "ATSH" + currentECUHeader;
        connectionManager->send(headerCommand);
        QThread::msleep(50);
    }

    lastSentCommand = cleanCommand;
    logWJData("→ " + cleanCommand);
    connectionManager->send(cleanCommand);
}

void MainWindow::onDataReceived(const QString& data) {
    if (data.isEmpty()) {
        return;
    }

    // Clean the data
    QString cleanData = data;
    cleanData.remove("\r");
    cleanData.remove(">");
    cleanData.remove("?");
    cleanData.remove(QChar(0xFFFD));
    cleanData.remove(QChar(0x00));

    // Check for errors
    if (isWJError(cleanData)) {
        logWJData("❌ WJ Error: " + cleanData);
        return;
    }

    // Remove echo and log response
    QString response = removeCommandEcho(cleanData);
    if (!response.isEmpty() && !protocolSwitchingInProgress) {
        logWJData("← " + response);
    }

    // Handle initialization or data processing
    if (!initialized) {
        processWJInitResponse(cleanData);
    } else {
        parseWJResponse(response);
    }
}

void MainWindow::parseWJResponse(const QString& response) {
    QString cleanResponse = cleanWJData(response);

    // Determine which module's data this is based on current module and response format
    switch (currentModule) {
        case MODULE_ENGINE_EDC15:
            parseEngineData(cleanResponse);
            break;
        case MODULE_TRANSMISSION:
            parseTransmissionData(cleanResponse);
            break;
        case MODULE_PCM:
            parsePCMData(cleanResponse);
            break;
        case MODULE_ABS:
            parseABSData(cleanResponse);
            break;
        default:
            // Try to determine from response format
            if (cleanResponse.startsWith("61 20") || cleanResponse.startsWith("61 12") ||
                cleanResponse.startsWith("61 28") || cleanResponse.startsWith("C1")) {
                parseEngineData(cleanResponse);
            } else if (cleanResponse.startsWith("41")) {
                // Could be any J1850 module - determine by context
                if (currentModule == MODULE_TRANSMISSION) parseTransmissionData(cleanResponse);
                else if (currentModule == MODULE_PCM) parsePCMData(cleanResponse);
                else if (currentModule == MODULE_ABS) parseABSData(cleanResponse);
            } else if (cleanResponse.startsWith("43")) {
                // Fault codes - determine by current module
                parseFaultCodes(cleanResponse, currentModule);
            }
            break;
    }

    // Update displays
    updateSensorDisplays();
}

// Missing functions implementation for MainWindow.cpp

// Engine Diagnostic Commands (ISO 9141-2)
void MainWindow::onReadEngineMAFClicked() {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        logWJData("❌ Failed to switch to engine module");
        return;
    }

    logWJData("→ Reading engine MAF data...");
    sendWJCommand(WJ::Engine::READ_MAF_DATA, MODULE_ENGINE_EDC15);
}

void MainWindow::onReadEngineRailPressureClicked() {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        logWJData("❌ Failed to switch to engine module");
        return;
    }

    logWJData("→ Reading engine rail pressure...");
    sendWJCommand(WJ::Engine::READ_RAIL_PRESSURE_ACTUAL, MODULE_ENGINE_EDC15);

    // Also read specified rail pressure
    QTimer::singleShot(500, [this]() {
        sendWJCommand(WJ::Engine::READ_RAIL_PRESSURE_SPEC, MODULE_ENGINE_EDC15);
    });
}

void MainWindow::onReadEngineMAPClicked() {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        logWJData("❌ Failed to switch to engine module");
        return;
    }

    logWJData("→ Reading engine MAP data...");
    sendWJCommand(WJ::Engine::READ_MAP_DATA, MODULE_ENGINE_EDC15);
}

void MainWindow::onReadEngineInjectorCorrectionsClicked() {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        logWJData("❌ Failed to switch to engine module");
        return;
    }

    logWJData("→ Reading engine injector corrections...");
    sendWJCommand(WJ::Engine::READ_INJECTOR_DATA, MODULE_ENGINE_EDC15);
}

void MainWindow::onReadEngineMiscDataClicked() {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        logWJData("❌ Failed to switch to engine module");
        return;
    }

    logWJData("→ Reading engine miscellaneous data...");
    sendWJCommand(WJ::Engine::READ_MISC_DATA, MODULE_ENGINE_EDC15);
}

void MainWindow::onReadEngineBatteryVoltageClicked() {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        logWJData("❌ Failed to switch to engine module");
        return;
    }

    logWJData("→ Reading battery voltage...");
    sendWJCommand(WJ::Engine::READ_BATTERY_VOLTAGE, MODULE_ENGINE_EDC15);
}

void MainWindow::onReadEngineAllSensorsClicked() {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        logWJData("❌ Failed to switch to engine module");
        return;
    }

    logWJData("→ Reading all engine sensors...");

    // Read all engine data in sequence
    QStringList commands = {
        WJ::Engine::READ_MAF_DATA,
        WJ::Engine::READ_RAIL_PRESSURE_ACTUAL,
        WJ::Engine::READ_RAIL_PRESSURE_SPEC,
        WJ::Engine::READ_MAP_DATA,
        WJ::Engine::READ_INJECTOR_DATA,
        WJ::Engine::READ_MISC_DATA,
        WJ::Engine::READ_BATTERY_VOLTAGE
    };

    for (int i = 0; i < commands.size(); ++i) {
        QTimer::singleShot(i * 300, [this, commands, i]() {
            sendWJCommand(commands[i], MODULE_ENGINE_EDC15);
        });
    }
}

void MainWindow::onReadEngineFaultCodesClicked() {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        logWJData("❌ Failed to switch to engine module");
        return;
    }

    logWJData("→ Reading engine fault codes...");
    sendWJCommand(WJ::Engine::READ_DTC, MODULE_ENGINE_EDC15);
}

void MainWindow::onClearEngineFaultCodesClicked() {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        logWJData("❌ Failed to switch to engine module");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Clear Engine Fault Codes",
        "Are you sure you want to clear all engine fault codes?\n\nThis action cannot be undone.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        logWJData("→ Clearing engine fault codes...");
        sendWJCommand(WJ::Engine::CLEAR_DTC, MODULE_ENGINE_EDC15);
    }
}

// Transmission Diagnostic Commands (J1850 VPW)
void MainWindow::onReadTransmissionDataClicked() {
    if (!switchToModule(MODULE_TRANSMISSION)) {
        logWJData("❌ Failed to switch to transmission module");
        return;
    }

    logWJData("→ Reading transmission data...");
    sendWJCommand(WJ::Transmission::READ_TRANS_DATA, MODULE_TRANSMISSION);
}

void MainWindow::onReadTransmissionSolenoidsClicked() {
    if (!switchToModule(MODULE_TRANSMISSION)) {
        logWJData("❌ Failed to switch to transmission module");
        return;
    }

    logWJData("→ Reading transmission solenoids...");
    sendWJCommand(WJ::Transmission::READ_SOLENOID_STATUS, MODULE_TRANSMISSION);
}

void MainWindow::onReadTransmissionSpeedsClicked() {
    if (!switchToModule(MODULE_TRANSMISSION)) {
        logWJData("❌ Failed to switch to transmission module");
        return;
    }

    logWJData("→ Reading transmission speeds...");
    sendWJCommand(WJ::Transmission::READ_SPEED_DATA, MODULE_TRANSMISSION);
}

void MainWindow::onReadTransmissionFaultCodesClicked() {
    if (!switchToModule(MODULE_TRANSMISSION)) {
        logWJData("❌ Failed to switch to transmission module");
        return;
    }

    logWJData("→ Reading transmission fault codes...");
    sendWJCommand(WJ::Transmission::READ_DTC, MODULE_TRANSMISSION);
}

void MainWindow::onClearTransmissionFaultCodesClicked() {
    if (!switchToModule(MODULE_TRANSMISSION)) {
        logWJData("❌ Failed to switch to transmission module");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Clear Transmission Fault Codes",
        "Are you sure you want to clear all transmission fault codes?\n\nThis action cannot be undone.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        logWJData("→ Clearing transmission fault codes...");
        sendWJCommand(WJ::Transmission::CLEAR_DTC, MODULE_TRANSMISSION);
    }
}

// PCM Diagnostic Commands (J1850 VPW)
void MainWindow::onReadPCMDataClicked() {
    if (!switchToModule(MODULE_PCM)) {
        logWJData("❌ Failed to switch to PCM module");
        return;
    }

    logWJData("→ Reading PCM data...");
    sendWJCommand(WJ::PCM::READ_LIVE_DATA, MODULE_PCM);
}

void MainWindow::onReadPCMFuelTrimClicked() {
    if (!switchToModule(MODULE_PCM)) {
        logWJData("❌ Failed to switch to PCM module");
        return;
    }

    logWJData("→ Reading PCM fuel trim...");
    sendWJCommand(WJ::PCM::READ_FUEL_TRIM, MODULE_PCM);
}

void MainWindow::onReadPCMO2SensorsClicked() {
    if (!switchToModule(MODULE_PCM)) {
        logWJData("❌ Failed to switch to PCM module");
        return;
    }

    logWJData("→ Reading PCM O2 sensors...");
    sendWJCommand(WJ::PCM::READ_O2_SENSORS, MODULE_PCM);
}

void MainWindow::onReadPCMFaultCodesClicked() {
    if (!switchToModule(MODULE_PCM)) {
        logWJData("❌ Failed to switch to PCM module");
        return;
    }

    logWJData("→ Reading PCM fault codes...");
    sendWJCommand(WJ::PCM::READ_DTC, MODULE_PCM);
}

void MainWindow::onClearPCMFaultCodesClicked() {
    if (!switchToModule(MODULE_PCM)) {
        logWJData("❌ Failed to switch to PCM module");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Clear PCM Fault Codes",
        "Are you sure you want to clear all PCM fault codes?\n\nThis action cannot be undone.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        logWJData("→ Clearing PCM fault codes...");
        sendWJCommand(WJ::PCM::CLEAR_DTC, MODULE_PCM);
    }
}

// ABS Diagnostic Commands (J1850 VPW)
void MainWindow::onReadABSWheelSpeedsClicked() {
    if (!switchToModule(MODULE_ABS)) {
        logWJData("❌ Failed to switch to ABS module");
        return;
    }

    logWJData("→ Reading ABS wheel speeds...");
    sendWJCommand(WJ::ABS::READ_WHEEL_SPEEDS, MODULE_ABS);
}

void MainWindow::onReadABSStabilityDataClicked() {
    if (!switchToModule(MODULE_ABS)) {
        logWJData("❌ Failed to switch to ABS module");
        return;
    }

    logWJData("→ Reading ABS stability data...");
    sendWJCommand(WJ::ABS::READ_STABILITY_DATA, MODULE_ABS);
}

void MainWindow::onReadABSFaultCodesClicked() {
    if (!switchToModule(MODULE_ABS)) {
        logWJData("❌ Failed to switch to ABS module");
        return;
    }

    logWJData("→ Reading ABS fault codes...");
    sendWJCommand(WJ::ABS::READ_DTC, MODULE_ABS);
}

void MainWindow::onClearABSFaultCodesClicked() {
    if (!switchToModule(MODULE_ABS)) {
        logWJData("❌ Failed to switch to ABS module");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Clear ABS Fault Codes",
        "Are you sure you want to clear all ABS fault codes?\n\nThis action cannot be undone.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        logWJData("→ Clearing ABS fault codes...");
        sendWJCommand(WJ::ABS::CLEAR_DTC, MODULE_ABS);
    }
}

// Multi-Module Operations
void MainWindow::onReadAllModuleFaultCodesClicked() {
    if (!connected || !initialized) {
        logWJData("❌ Not connected or initialized");
        return;
    }

    logWJData("→ Reading fault codes from all modules...");
    progressBar->setVisible(true);
    progressBar->setValue(0);
    progressBar->setMaximum(4);
    progressLabel->setText("Reading fault codes from all modules...");

    faultCodeTree->clear();

    // Read fault codes from all modules in sequence
    QList<WJModule> modules = {MODULE_ENGINE_EDC15, MODULE_TRANSMISSION, MODULE_PCM, MODULE_ABS};
    QStringList moduleNames = {"Engine (EDC15)", "Transmission", "PCM", "ABS"};

    for (int i = 0; i < modules.size(); ++i) {
        QTimer::singleShot(i * 2000, [this, modules, moduleNames, i]() {
            progressBar->setValue(i);
            progressLabel->setText("Reading " + moduleNames[i] + " fault codes...");

            if (switchToModule(modules[i])) {
                switch (modules[i]) {
                    case MODULE_ENGINE_EDC15:
                        sendWJCommand(WJ::Engine::READ_DTC, modules[i]);
                        break;
                    case MODULE_TRANSMISSION:
                        sendWJCommand(WJ::Transmission::READ_DTC, modules[i]);
                        break;
                    case MODULE_PCM:
                        sendWJCommand(WJ::PCM::READ_DTC, modules[i]);
                        break;
                    case MODULE_ABS:
                        sendWJCommand(WJ::ABS::READ_DTC, modules[i]);
                        break;
                    default:
                        break;
                }
            }

            if (i == modules.size() - 1) {
                QTimer::singleShot(1500, [this]() {
                    progressBar->setValue(4);
                    progressLabel->setText("All fault codes read");
                    QTimer::singleShot(2000, [this]() {
                        progressBar->setVisible(false);
                        progressLabel->setText("Ready");
                    });
                });
            }
        });
    }
}

void MainWindow::onClearAllModuleFaultCodesClicked() {
    if (!connected || !initialized) {
        logWJData("❌ Not connected or initialized");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Clear All Fault Codes",
        "Are you sure you want to clear ALL fault codes from ALL modules?\n\n"
        "This will clear fault codes from:\n"
        "- Engine (EDC15)\n"
        "- Transmission\n"
        "- PCM\n"
        "- ABS\n\n"
        "This action cannot be undone.",
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    logWJData("→ Clearing fault codes from all modules...");
    progressBar->setVisible(true);
    progressBar->setValue(0);
    progressBar->setMaximum(4);
    progressLabel->setText("Clearing fault codes from all modules...");

    // Clear fault codes from all modules in sequence
    QList<WJModule> modules = {MODULE_ENGINE_EDC15, MODULE_TRANSMISSION, MODULE_PCM, MODULE_ABS};
    QStringList moduleNames = {"Engine (EDC15)", "Transmission", "PCM", "ABS"};

    for (int i = 0; i < modules.size(); ++i) {
        QTimer::singleShot(i * 2000, [this, modules, moduleNames, i]() {
            progressBar->setValue(i);
            progressLabel->setText("Clearing " + moduleNames[i] + " fault codes...");

            if (switchToModule(modules[i])) {
                switch (modules[i]) {
                    case MODULE_ENGINE_EDC15:
                        sendWJCommand(WJ::Engine::CLEAR_DTC, modules[i]);
                        break;
                    case MODULE_TRANSMISSION:
                        sendWJCommand(WJ::Transmission::CLEAR_DTC, modules[i]);
                        break;
                    case MODULE_PCM:
                        sendWJCommand(WJ::PCM::CLEAR_DTC, modules[i]);
                        break;
                    case MODULE_ABS:
                        sendWJCommand(WJ::ABS::CLEAR_DTC, modules[i]);
                        break;
                    default:
                        break;
                }
            }

            if (i == modules.size() - 1) {
                QTimer::singleShot(1500, [this]() {
                    progressBar->setValue(4);
                    progressLabel->setText("All fault codes cleared");
                    faultCodeTree->clear();
                    QTimer::singleShot(2000, [this]() {
                        progressBar->setVisible(false);
                        progressLabel->setText("Ready");
                    });
                });
            }
        });
    }
}

void MainWindow::onReadAllSensorDataClicked() {
    if (!connected || !initialized) {
        logWJData("❌ Not connected or initialized");
        return;
    }

    logWJData("→ Reading sensor data from all modules...");
    progressBar->setVisible(true);
    progressBar->setValue(0);
    progressBar->setMaximum(10);
    progressLabel->setText("Reading all sensor data...");

    // Read all sensor data in sequence
    QStringList commands = {
        WJ::Engine::READ_MAF_DATA,
        WJ::Engine::READ_RAIL_PRESSURE_ACTUAL,
        WJ::Engine::READ_INJECTOR_DATA,
        WJ::Engine::READ_MISC_DATA,
        WJ::Transmission::READ_TRANS_DATA,
        WJ::Transmission::READ_SPEED_DATA,
        WJ::PCM::READ_LIVE_DATA,
        WJ::PCM::READ_FUEL_TRIM,
        WJ::ABS::READ_WHEEL_SPEEDS,
        WJ::ABS::READ_STABILITY_DATA
    };

    QList<WJModule> modules = {
        MODULE_ENGINE_EDC15, MODULE_ENGINE_EDC15, MODULE_ENGINE_EDC15, MODULE_ENGINE_EDC15,
        MODULE_TRANSMISSION, MODULE_TRANSMISSION,
        MODULE_PCM, MODULE_PCM,
        MODULE_ABS, MODULE_ABS
    };

    for (int i = 0; i < commands.size(); ++i) {
        QTimer::singleShot(i * 800, [this, commands, modules, i]() {
            progressBar->setValue(i);

            if (switchToModule(modules[i])) {
                sendWJCommand(commands[i], modules[i]);
            }

            if (i == commands.size() - 1) {
                QTimer::singleShot(500, [this]() {
                    progressBar->setValue(10);
                    progressLabel->setText("All sensor data read");
                    QTimer::singleShot(2000, [this]() {
                        progressBar->setVisible(false);
                        progressLabel->setText("Ready");
                    });
                });
            }
        });
    }
}

// Manual Command and UI Controls
void MainWindow::onSendCommandClicked() {
    if (!connected || !connectionManager) {
        logWJData("❌ Not connected");
        return;
    }

    QString command = commandLineEdit->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    logWJData("→ Manual command: " + command);
    sendWJCommand(command);

    // Clear the input field
    commandLineEdit->clear();
}

void MainWindow::onClearTerminalClicked() {
    connectButton->setEnabled(true);
    disconnectButton->setEnabled(false);
    terminalDisplay->clear();
    disconnectFromWJ();
    logWJData("Terminal cleared");
}

void MainWindow::onExitClicked() {
    if (connected) {
        disconnectFromWJ();
    }
    QApplication::quit();
}

// Continuous Reading
void MainWindow::onContinuousReadingToggled(bool enabled) {
    if (!connected || !initialized) {
        continuousReadingCheckBox->setChecked(false);
        return;
    }

    continuousReading = enabled;

    if (enabled) {
        startContinuousReading();
    } else {
        stopContinuousReading();
    }
}

void MainWindow::onReadingIntervalChanged(int intervalMs) {
    readingInterval = intervalMs;
    intervalLabel->setText(QString::number(intervalMs) + " ms");

    if (continuousReadingTimer->isActive()) {
        continuousReadingTimer->setInterval(intervalMs);
    }
}

void MainWindow::startContinuousReading() {
    if (!connected || !initialized) {
        return;
    }

    logWJData("→ Starting continuous reading...");
    continuousReadingTimer->setInterval(readingInterval);
    continuousReadingTimer->start();
}

void MainWindow::stopContinuousReading() {
    if (continuousReadingTimer->isActive()) {
        continuousReadingTimer->stop();
        logWJData("→ Stopped continuous reading");
    }
}

void MainWindow::performContinuousRead() {
    if (!connected || !initialized) {
        stopContinuousReading();
        return;
    }

    // Read data based on current active tab
    int currentTab = moduleTabWidget->currentIndex();

    switch (currentTab) {
        case 0: // Engine tab
            if (currentModule == MODULE_ENGINE_EDC15) {
                sendWJCommand(WJ::Engine::READ_MAF_DATA, MODULE_ENGINE_EDC15);
            }
            break;
        case 1: // Transmission tab
            if (currentModule == MODULE_TRANSMISSION) {
                sendWJCommand(WJ::Transmission::READ_TRANS_DATA, MODULE_TRANSMISSION);
            }
            break;
        case 2: // PCM tab
            if (currentModule == MODULE_PCM) {
                sendWJCommand(WJ::PCM::READ_LIVE_DATA, MODULE_PCM);
            }
            break;
        case 3: // ABS tab
            if (currentModule == MODULE_ABS) {
                sendWJCommand(WJ::ABS::READ_WHEEL_SPEEDS, MODULE_ABS);
            }
            break;
        default:
            break;
    }
}

// Tab Management
void MainWindow::onTabChanged(int index) {
    if (!connected || !initialized) {
        return;
    }

    // Switch to appropriate module based on tab
    WJModule targetModule = MODULE_UNKNOWN;

    switch (index) {
        case 0: targetModule = MODULE_ENGINE_EDC15; break;
        case 1: targetModule = MODULE_TRANSMISSION; break;
        case 2: targetModule = MODULE_PCM; break;
        case 3: targetModule = MODULE_ABS; break;
        case 4: return; // Multi-module tab - no specific module
        default: return;
    }

    // Update module combo box
    moduleCombo->setCurrentIndex(index);

    // Switch to the module
    if (switchToModule(targetModule)) {
        currentModule = targetModule;
        currentModuleLabel->setText("Current: " + WJUtils::getModuleName(targetModule));
        updateUIForCurrentModule();
    }
}

// Data Parsing Methods
void MainWindow::parseEngineData(const QString& data) {
    if (data.isEmpty()) return;

    // Use the WJDataParser to parse engine data
    if (WJDataParser::parseEngineMAFData(data, sensorData)) {
        updateEngineDisplay();
    } else if (WJDataParser::parseEngineRailPressureData(data, sensorData)) {
        updateEngineDisplay();
    } else if (WJDataParser::parseEngineMAPData(data, sensorData)) {
        updateEngineDisplay();
    } else if (WJDataParser::parseEngineInjectorData(data, sensorData)) {
        updateEngineDisplay();
    } else if (WJDataParser::parseEngineMiscData(data, sensorData)) {
        updateEngineDisplay();
    } else if (WJDataParser::parseEngineBatteryVoltage(data, sensorData)) {
        updateEngineDisplay();
    } else if (data.startsWith("43")) {
        // Engine fault codes
        parseFaultCodes(data, MODULE_ENGINE_EDC15);
    }
}

void MainWindow::parseTransmissionData(const QString& data) {
    if (data.isEmpty()) return;

    // Use the WJDataParser to parse transmission data
    if (WJDataParser::parseTransmissionData(data, sensorData)) {
        updateTransmissionDisplay();
    } else if (WJDataParser::parseTransmissionSpeeds(data, sensorData)) {
        updateTransmissionDisplay();
    } else if (WJDataParser::parseTransmissionSolenoids(data, sensorData)) {
        updateTransmissionDisplay();
    } else if (data.startsWith("43")) {
        // Transmission fault codes
        parseFaultCodes(data, MODULE_TRANSMISSION);
    }
}

void MainWindow::parsePCMData(const QString& data) {
    if (data.isEmpty()) return;

    // Use the WJDataParser to parse PCM data
    if (WJDataParser::parsePCMData(data, sensorData)) {
        updatePCMDisplay();
    } else if (WJDataParser::parsePCMFuelTrim(data, sensorData)) {
        updatePCMDisplay();
    } else if (WJDataParser::parsePCMO2Sensors(data, sensorData)) {
        updatePCMDisplay();
    } else if (data.startsWith("43")) {
        // PCM fault codes
        parseFaultCodes(data, MODULE_PCM);
    }
}

void MainWindow::parseABSData(const QString& data) {
    if (data.isEmpty()) return;

    // Use the WJDataParser to parse ABS data
    if (WJDataParser::parseABSWheelSpeeds(data, sensorData)) {
        updateABSDisplay();
    } else if (WJDataParser::parseABSStabilityData(data, sensorData)) {
        updateABSDisplay();
    } else if (data.startsWith("43")) {
        // ABS fault codes
        parseFaultCodes(data, MODULE_ABS);
    }
}

void MainWindow::parseFaultCodes(const QString& data, WJModule module) {
    QList<WJ_DTC> dtcs = WJDataParser::parseGenericFaultCodes(data, module, WJUtils::getProtocolFromModule(module));

    if (dtcs.isEmpty()) {
        QString moduleNameStr = WJUtils::getModuleName(module);
        logWJData("✓ No fault codes found in " + moduleNameStr);
        return;
    }

    displayFaultCodes(dtcs, WJUtils::getModuleName(module));
}

// Utility Methods
QString MainWindow::cleanWJData(const QString& input) {
    return WJUtils::cleanData(input, currentProtocol);
}

QString MainWindow::removeCommandEcho(const QString& data) {
    QString cleaned = data;

    // Remove the last sent command from the beginning of the response
    if (!lastSentCommand.isEmpty() && cleaned.startsWith(lastSentCommand)) {
        cleaned = cleaned.mid(lastSentCommand.length()).trimmed();
    }

    return cleaned;
}

bool MainWindow::isWJError(const QString& response) {
    return WJUtils::isError(response, currentProtocol);
}

bool MainWindow::validateWJResponse(const QString& response, const QString& expectedStart) {
    return response.trimmed().toUpper().startsWith(expectedStart.toUpper());
}

void MainWindow::logWJData(const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString logMessage = QString("[%1] %2").arg(timestamp, message);

    terminalDisplay->append(logMessage);
    terminalDisplay->moveCursor(QTextCursor::End);

    // Limit terminal content to prevent memory issues
    if (terminalDisplay->document()->blockCount() > 1000) {
        QTextCursor cursor = terminalDisplay->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 100);
        cursor.removeSelectedText();
    }
}

// Display Update Methods
void MainWindow::updateSensorDisplays() {
    updateEngineDisplay();
    updateTransmissionDisplay();
    updatePCMDisplay();
    updateABSDisplay();
}

void MainWindow::updateEngineDisplay() {
    if (sensorData.engine.dataValid) {
        mafActualLabel->setText(formatSensorValue(sensorData.engine.mafActual, "g/s", 1));
        mafSpecifiedLabel->setText(formatSensorValue(sensorData.engine.mafSpecified, "g/s", 1));
        railPressureActualLabel->setText(formatSensorValue(sensorData.engine.railPressureActual, "bar", 1));
        railPressureSpecifiedLabel->setText(formatSensorValue(sensorData.engine.railPressureSpecified, "bar", 1));
        mapActualLabel->setText(formatSensorValue(sensorData.engine.mapActual, "mbar", 0));
        mapSpecifiedLabel->setText(formatSensorValue(sensorData.engine.mapSpecified, "mbar", 0));
        coolantTempLabel->setText(formatSensorValue(sensorData.engine.coolantTemp, "°C", 1));
        intakeAirTempLabel->setText(formatSensorValue(sensorData.engine.intakeAirTemp, "°C", 1));
        throttlePositionLabel->setText(formatSensorValue(sensorData.engine.throttlePosition, "%", 1));
        rpmLabel->setText(formatSensorValue(sensorData.engine.engineRPM, "rpm", 0));
        injectionQuantityLabel->setText(formatSensorValue(sensorData.engine.injectionQuantity, "mg", 1));
        batteryVoltageLabel->setText(formatSensorValue(sensorData.engine.batteryVoltage, "V", 2));

        injector1Label->setText(formatSensorValue(sensorData.engine.injector1Correction, "mg", 2));
        injector2Label->setText(formatSensorValue(sensorData.engine.injector2Correction, "mg", 2));
        injector3Label->setText(formatSensorValue(sensorData.engine.injector3Correction, "mg", 2));
        injector4Label->setText(formatSensorValue(sensorData.engine.injector4Correction, "mg", 2));
        injector5Label->setText(formatSensorValue(sensorData.engine.injector5Correction, "mg", 2));
    }
}

void MainWindow::updateTransmissionDisplay() {
    if (sensorData.transmission.dataValid) {
        transOilTempLabel->setText(formatSensorValue(sensorData.transmission.oilTemp, "°C", 1));
        transInputSpeedLabel->setText(formatSensorValue(sensorData.transmission.inputSpeed, "rpm", 0));
        transOutputSpeedLabel->setText(formatSensorValue(sensorData.transmission.outputSpeed, "rpm", 0));
        transCurrentGearLabel->setText(formatSensorValue(sensorData.transmission.currentGear, "", 0));
        transLinePressureLabel->setText(formatSensorValue(sensorData.transmission.linePresssure, "psi", 1));
        transSolenoidALabel->setText(formatSensorValue(sensorData.transmission.shiftSolenoidA, "%", 1));
        transSolenoidBLabel->setText(formatSensorValue(sensorData.transmission.shiftSolenoidB, "%", 1));
        transTCCSolenoidLabel->setText(formatSensorValue(sensorData.transmission.tccSolenoid, "%", 1));
        transTorqueConverterLabel->setText(formatSensorValue(sensorData.transmission.torqueConverter, "%", 1));
    }
}

void MainWindow::updatePCMDisplay() {
    if (sensorData.pcm.dataValid) {
        vehicleSpeedLabel->setText(formatSensorValue(sensorData.pcm.vehicleSpeed, "km/h", 0));
        engineLoadLabel->setText(formatSensorValue(sensorData.pcm.engineLoad, "%", 1));
        fuelTrimSTLabel->setText(formatSensorValue(sensorData.pcm.fuelTrimST, "%", 2));
        fuelTrimLTLabel->setText(formatSensorValue(sensorData.pcm.fuelTrimLT, "%", 2));
        o2Sensor1Label->setText(formatSensorValue(sensorData.pcm.o2Sensor1, "V", 3));
        o2Sensor2Label->setText(formatSensorValue(sensorData.pcm.o2Sensor2, "V", 3));
        timingAdvanceLabel->setText(formatSensorValue(sensorData.pcm.timingAdvance, "°", 1));
        barometricPressureLabel->setText(formatSensorValue(sensorData.pcm.barometricPressure, "kPa", 1));
    }
}

void MainWindow::updateABSDisplay() {
    if (sensorData.abs.dataValid) {
        wheelSpeedFLLabel->setText(formatSensorValue(sensorData.abs.wheelSpeedFL, "km/h", 1));
        wheelSpeedFRLabel->setText(formatSensorValue(sensorData.abs.wheelSpeedFR, "km/h", 1));
        wheelSpeedRLLabel->setText(formatSensorValue(sensorData.abs.wheelSpeedRL, "km/h", 1));
        wheelSpeedRRLabel->setText(formatSensorValue(sensorData.abs.wheelSpeedRR, "km/h", 1));
        yawRateLabel->setText(formatSensorValue(sensorData.abs.yawRate, "deg/s", 2));
        lateralAccelLabel->setText(formatSensorValue(sensorData.abs.lateralAccel, "g", 3));
    }
}

QString MainWindow::formatSensorValue(double value, const QString& unit, int decimals) {
    if (value == 0.0 && unit.isEmpty()) {
        return "--";
    }

    QString formattedValue = QString::number(value, 'f', decimals);

    if (unit.isEmpty()) {
        return formattedValue;
    }

    return formattedValue + " " + unit;
}

// Fault Code Management
void MainWindow::displayFaultCodes(const QList<WJ_DTC>& dtcs, const QString& moduleLabel) {
    if (dtcs.isEmpty()) {
        return;
    }

    // Find or create module item in tree
    QTreeWidgetItem* moduleItem = nullptr;
    for (int i = 0; i < faultCodeTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = faultCodeTree->topLevelItem(i);
        if (item->text(0) == moduleLabel) {
            moduleItem = item;
            break;
        }
    }

    if (!moduleItem) {
        moduleItem = new QTreeWidgetItem(faultCodeTree);
        moduleItem->setText(0, moduleLabel);
        moduleItem->setExpanded(true);

        // Set module item styling
        QFont font = moduleItem->font(0);
        font.setBold(true);
        moduleItem->setFont(0, font);
    }

    // Clear existing fault codes for this module
    while (moduleItem->childCount() > 0) {
        delete moduleItem->takeChild(0);
    }

    // Add fault codes
    for (const WJ_DTC& dtc : dtcs) {
        QTreeWidgetItem* dtcItem = new QTreeWidgetItem(moduleItem);
        dtcItem->setText(0, "");  // Module column empty for DTC items
        dtcItem->setText(1, dtc.code);
        dtcItem->setText(2, dtc.description);

        QString status = dtc.confirmed ? "Confirmed" : "Pending";
        if (WJ_DTCs::isCriticalDTC(dtc.code, dtc.sourceModule)) {
            status += " [CRITICAL]";
            // Set red color for critical DTCs
            dtcItem->setForeground(1, QColor(220, 38, 38)); // Red
            dtcItem->setForeground(2, QColor(220, 38, 38));
            dtcItem->setForeground(3, QColor(220, 38, 38));
        }
        dtcItem->setText(3, status);
    }

    // Update terminal log
    logWJData(QString("✓ Found %1 fault code(s) in %2:").arg(dtcs.size()).arg(moduleLabel));
    for (const WJ_DTC& dtc : dtcs) {
        QString criticality = WJ_DTCs::isCriticalDTC(dtc.code, dtc.sourceModule) ? " [CRITICAL]" : "";
        logWJData(QString("  %1: %2%3").arg(dtc.code, dtc.description, criticality));
    }
}

void MainWindow::clearFaultCodesForModule(WJModule module) {
    QString moduleLabel = WJUtils::getModuleName(module);

    // Find and remove module item from tree
    for (int i = 0; i < faultCodeTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = faultCodeTree->topLevelItem(i);
        if (item->text(0) == moduleLabel) {
            delete faultCodeTree->takeTopLevelItem(i);
            break;
        }
    }

    logWJData("✓ Cleared fault codes display for " + moduleLabel);
}

// Additional utility function for DTC code formatting
QString MainWindow::formatDTCCode(int byte1, int byte2) {
    return WJUtils::formatDTCCode(byte1, byte2, currentProtocol);
}
