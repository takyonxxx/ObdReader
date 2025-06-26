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
    , leftPanel(nullptr)
    , centerPanel(nullptr)
    , rightPanel(nullptr)
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
    // Initialize ALL sensor label pointers to nullptr for safety
    , sensor1Label(nullptr)
    , sensor2Label(nullptr)
    , sensor3Label(nullptr)
    , sensor4Label(nullptr)
    , sensor5Label(nullptr)
    , sensor6Label(nullptr)
    , sensor7Label(nullptr)
    , sensor8Label(nullptr)
    , sensor9Label(nullptr)
    , sensor10Label(nullptr)
    , sensor11Label(nullptr)
    , sensor12Label(nullptr)
    // Initialize mapped sensor pointers
    , mafActualLabel(nullptr)
    , mafSpecifiedLabel(nullptr)
    , railPressureActualLabel(nullptr)
    , railPressureSpecifiedLabel(nullptr)
    , mapActualLabel(nullptr)
    , mapSpecifiedLabel(nullptr)
    , coolantTempLabel(nullptr)
    , intakeAirTempLabel(nullptr)
    , throttlePositionLabel(nullptr)
    , rpmLabel(nullptr)
    , injectionQuantityLabel(nullptr)
    , batteryVoltageLabel(nullptr)
    , injector1Label(nullptr)
    , injector2Label(nullptr)
    , injector3Label(nullptr)
    , injector4Label(nullptr)
    , injector5Label(nullptr)
    , transOilTempLabel(nullptr)
    , transInputSpeedLabel(nullptr)
    , transOutputSpeedLabel(nullptr)
    , transCurrentGearLabel(nullptr)
    , transLinePressureLabel(nullptr)
    , transSolenoidALabel(nullptr)
    , transSolenoidBLabel(nullptr)
    , transTCCSolenoidLabel(nullptr)
    , transTorqueConverterLabel(nullptr)
    , vehicleSpeedLabel(nullptr)
    , engineLoadLabel(nullptr)
    , fuelTrimSTLabel(nullptr)
    , fuelTrimLTLabel(nullptr)
    , o2Sensor1Label(nullptr)
    , o2Sensor2Label(nullptr)
    , timingAdvanceLabel(nullptr)
    , barometricPressureLabel(nullptr)
    , wheelSpeedFLLabel(nullptr)
    , wheelSpeedFRLabel(nullptr)
    , wheelSpeedRLLabel(nullptr)
    , wheelSpeedRRLabel(nullptr)
    , yawRateLabel(nullptr)
    , lateralAccelLabel(nullptr)
{
    setWindowTitle("Jeep WJ Diagnostic Tool - Car Stereo Edition");

    // Get screen geometry for car stereo (1280x720)
    QScreen *screen = window()->screen();
    desktopRect = screen->availableGeometry();
    qDebug() << "Screen resolution:" << desktopRect.width() << "x" << desktopRect.height();

    // Setup WJ initialization commands
    setupWJInitializationCommands();

    // Setup UI FIRST - terminalDisplay burada oluşturuluyor
    setupUI();
    applyCarStereoStyling();

    // Then initialize components
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

    // Log device information - artık çalışacak
    logDeviceInformation();

    // Display connection settings
    if (settingsManager) {
        logWJData("WiFi: " + settingsManager->getWifiIp() + ":" +
                  QString::number(settingsManager->getWifiPort()));
    }
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
    // Start with ISO_14230_4_KWP_FAST for engine module by default
    initializationCommands = WJCommands::getInitSequence(PROTOCOL_ISO_14230_4_KWP_FAST);
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

// Car Stereo UI Setup Methods
void MainWindow::setupUI() {
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Main horizontal layout - divide screen into sections for 1280x720
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // Left panel: Connection and controls (300px)
    setupLeftPanel();

    // Center panel: Current module data (stretch)
    setupCenterPanel();

    // Right panel: Quick actions and status (360px)
    setupRightPanel();

    mainLayout->addWidget(leftPanel, 0);      // Fixed width
    mainLayout->addWidget(centerPanel, 1);    // Stretch
    mainLayout->addWidget(rightPanel, 0);     // Fixed width
}

void MainWindow::setupLeftPanel() {
    leftPanel = new QWidget();
    leftPanel->setFixedWidth(LEFT_PANEL_WIDTH);
    leftPanel->setStyleSheet("QWidget { background-color: #1E293B; border-radius: 8px; }");

    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setSpacing(8);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    // Connection Section
    QGroupBox* connectionGroup = new QGroupBox("Connection");
    connectionGroup->setFixedHeight(200);
    QVBoxLayout* connLayout = new QVBoxLayout(connectionGroup);

    // Connection type combo
    connectionTypeCombo = new QComboBox();
    connectionTypeCombo->addItem("WiFi");
    connectionTypeCombo->addItem("Bluetooth");
    connectionTypeCombo->setFixedHeight(MIN_BUTTON_HEIGHT);
    connLayout->addWidget(connectionTypeCombo);

    // Bluetooth device selection (initially hidden)
    btDeviceLabel = new QLabel("BT Device:");
    btDeviceLabel->setVisible(false);
    bluetoothDevicesCombo = new QComboBox();
    bluetoothDevicesCombo->addItem("Select device...");
    bluetoothDevicesCombo->setFixedHeight(MIN_BUTTON_HEIGHT);
    bluetoothDevicesCombo->setVisible(false);
    scanBluetoothButton = new QPushButton("Scan");
    scanBluetoothButton->setFixedHeight(MIN_BUTTON_HEIGHT);
    scanBluetoothButton->setVisible(false);

    connLayout->addWidget(btDeviceLabel);
    connLayout->addWidget(bluetoothDevicesCombo);
    connLayout->addWidget(scanBluetoothButton);

    // Status label
    connectionStatusLabel = new QLabel("Status: Disconnected");
    connectionStatusLabel->setWordWrap(true);
    connectionStatusLabel->setFixedHeight(30);
    connLayout->addWidget(connectionStatusLabel);

    // Connect/Disconnect buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    connectButton = new QPushButton("Connect");
    disconnectButton = new QPushButton("Disconnect");
    connectButton->setFixedHeight(MIN_BUTTON_HEIGHT + 5);
    disconnectButton->setFixedHeight(MIN_BUTTON_HEIGHT + 5);
    disconnectButton->setEnabled(false);

    buttonLayout->addWidget(connectButton);
    buttonLayout->addWidget(disconnectButton);
    connLayout->addLayout(buttonLayout);

    leftLayout->addWidget(connectionGroup);

    // Module Selection
    QGroupBox* moduleGroup = new QGroupBox("Module");
    moduleGroup->setFixedHeight(120);
    QVBoxLayout* moduleLayout = new QVBoxLayout(moduleGroup);

    moduleCombo = new QComboBox();
    moduleCombo->addItem("Engine (EDC15)");
    moduleCombo->addItem("Transmission");
    moduleCombo->addItem("PCM");
    moduleCombo->addItem("ABS");
    moduleCombo->setFixedHeight(MIN_BUTTON_HEIGHT);
    moduleLayout->addWidget(moduleCombo);

    currentModuleLabel = new QLabel("Current: Engine");
    currentModuleLabel->setFixedHeight(30);
    moduleLayout->addWidget(currentModuleLabel);

    leftLayout->addWidget(moduleGroup);

    // Quick Actions
    QGroupBox* actionsGroup = new QGroupBox("Quick Actions");
    QVBoxLayout* actionsLayout = new QVBoxLayout(actionsGroup);

    readAllSensorsButton = new QPushButton("Read All Sensors");
    readFaultCodesButton = new QPushButton("Read Fault Codes");
    clearFaultCodesButton = new QPushButton("Clear Fault Codes");

    readAllSensorsButton->setFixedHeight(MIN_BUTTON_HEIGHT);
    readFaultCodesButton->setFixedHeight(MIN_BUTTON_HEIGHT);
    clearFaultCodesButton->setFixedHeight(MIN_BUTTON_HEIGHT);

    readAllSensorsButton->setEnabled(false);
    readFaultCodesButton->setEnabled(false);
    clearFaultCodesButton->setEnabled(false);

    actionsLayout->addWidget(readAllSensorsButton);
    actionsLayout->addWidget(readFaultCodesButton);
    actionsLayout->addWidget(clearFaultCodesButton);

    leftLayout->addWidget(actionsGroup);

    leftLayout->addStretch(); // Push everything up
}

void MainWindow::setupCenterPanel() {
    centerPanel = new QWidget();
    centerPanel->setStyleSheet("QWidget { background-color: #0F172A; border-radius: 8px; }");

    QVBoxLayout* centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setSpacing(8);
    centerLayout->setContentsMargins(12, 12, 12, 12);

    // Module title
    moduleTitle = new QLabel("Engine Control Module");
    moduleTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #3B82F6; padding: 8px;");
    moduleTitle->setAlignment(Qt::AlignCenter);
    moduleTitle->setFixedHeight(40);
    centerLayout->addWidget(moduleTitle);

    // Sensor data in 3x4 grid
    QGroupBox* sensorGroup = new QGroupBox("Live Data");
    sensorGroup->setFixedHeight(480);
    QGridLayout* sensorLayout = new QGridLayout(sensorGroup);
    sensorLayout->setSpacing(8);

    // Create sensor displays - using larger, more readable format
    createSensorDisplay("MAF Actual", "-- g/s", sensorLayout, 0, 0, sensor1Label);
    createSensorDisplay("MAF Specified", "-- g/s", sensorLayout, 0, 1, sensor2Label);
    createSensorDisplay("Rail Pressure", "-- bar", sensorLayout, 0, 2, sensor3Label);
    createSensorDisplay("MAP", "-- mbar", sensorLayout, 0, 3, sensor4Label);

    createSensorDisplay("Coolant Temp", "-- °C", sensorLayout, 1, 0, sensor5Label);
    createSensorDisplay("Intake Temp", "-- °C", sensorLayout, 1, 1, sensor6Label);
    createSensorDisplay("Throttle Pos", "-- %", sensorLayout, 1, 2, sensor7Label);
    createSensorDisplay("RPM", "-- rpm", sensorLayout, 1, 3, sensor8Label);

    createSensorDisplay("Injection Qty", "-- mg", sensorLayout, 2, 0, sensor9Label);
    createSensorDisplay("Battery", "-- V", sensorLayout, 2, 1, sensor10Label);
    createSensorDisplay("Vehicle Speed", "-- km/h", sensorLayout, 2, 2, sensor11Label);
    createSensorDisplay("Engine Load", "-- %", sensorLayout, 2, 3, sensor12Label);

    centerLayout->addWidget(sensorGroup);

    // Protocol and timing info
    QHBoxLayout* infoLayout = new QHBoxLayout();
    protocolLabel = new QLabel("Protocol: Unknown");
    protocolLabel->setStyleSheet("color: #94A3B8; font-size: 12px;");

    QLabel* refreshLabel = new QLabel("Auto-refresh: OFF");
    refreshLabel->setStyleSheet("color: #94A3B8; font-size: 12px;");

    infoLayout->addWidget(protocolLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(refreshLabel);

    centerLayout->addLayout(infoLayout);

    // Initialize sensor mappings for engine (default)
    updateSensorLayoutForModule(MODULE_ENGINE_EDC15);
}

void MainWindow::setupRightPanel() {
    rightPanel = new QWidget();
    rightPanel->setFixedWidth(RIGHT_PANEL_WIDTH);
    rightPanel->setStyleSheet("QWidget { background-color: #1E293B; border-radius: 8px; }");

    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(8);
    rightLayout->setContentsMargins(12, 12, 12, 12);

    // Fault codes section
    QGroupBox* faultGroup = new QGroupBox("Fault Codes");
    faultGroup->setFixedHeight(250);
    QVBoxLayout* faultLayout = new QVBoxLayout(faultGroup);

    // Simplified fault code list
    faultCodeList = new QListWidget();
    faultCodeList->setFixedHeight(200);
    faultCodeList->setStyleSheet(
        "QListWidget {"
        "    background-color: #0F172A;"
        "    border: 1px solid #374151;"
        "    border-radius: 4px;"
        "    font-size: 11px;"
        "}"
        "QListWidget::item {"
        "    padding: 4px;"
        "    border-bottom: 1px solid #374151;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #3B82F6;"
        "}"
        );
    faultLayout->addWidget(faultCodeList);
    rightLayout->addWidget(faultGroup);

    // Auto-refresh controls
    QGroupBox* autoGroup = new QGroupBox("Auto Refresh");
    autoGroup->setFixedHeight(120);
    QVBoxLayout* autoLayout = new QVBoxLayout(autoGroup);

    continuousReadingCheckBox = new QCheckBox("Enable Auto Refresh");
    continuousReadingCheckBox->setFixedHeight(30);
    autoLayout->addWidget(continuousReadingCheckBox);

    QHBoxLayout* intervalLayout = new QHBoxLayout();
    QLabel* intervalLabelText = new QLabel("Interval:");
    readingIntervalSlider = new QSlider(Qt::Horizontal);
    readingIntervalSlider->setRange(500, 5000);
    readingIntervalSlider->setValue(1000);
    intervalLabel = new QLabel("1000ms");
    intervalLabel->setFixedWidth(60);

    intervalLayout->addWidget(intervalLabelText);
    intervalLayout->addWidget(readingIntervalSlider);
    intervalLayout->addWidget(intervalLabel);
    autoLayout->addLayout(intervalLayout);

    rightLayout->addWidget(autoGroup);

    // Terminal/Log section
    QGroupBox* logGroup = new QGroupBox("System Log");
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);

    terminalDisplay = new QTextBrowser();
    terminalDisplay->setFixedHeight(150);
    terminalDisplay->setFont(QFont("Consolas", 9));
    terminalDisplay->setStyleSheet(
        "QTextBrowser {"
        "    background-color: #000000;"
        "    color: #00FF00;"
        "    border: 1px solid #374151;"
        "    border-radius: 4px;"
        "    font-family: 'Consolas', monospace;"
        "}"
        );
    logLayout->addWidget(terminalDisplay);

    // Manual command controls
    QHBoxLayout* commandLayout = new QHBoxLayout();
    commandLineEdit = new QLineEdit();
    commandLineEdit->setPlaceholderText("Manual command...");
    commandLineEdit->setFixedHeight(30);
    sendCommandButton = new QPushButton("Send");
    sendCommandButton->setFixedHeight(30);
    sendCommandButton->setFixedWidth(60);
    sendCommandButton->setEnabled(false);

    commandLayout->addWidget(commandLineEdit);
    commandLayout->addWidget(sendCommandButton);
    logLayout->addLayout(commandLayout);

    // Clear log and exit buttons
    QHBoxLayout* controlLayout = new QHBoxLayout();
    clearTerminalButton = new QPushButton("Clear Log");
    exitButton = new QPushButton("Exit");
    clearTerminalButton->setFixedHeight(30);
    exitButton->setFixedHeight(30);

    controlLayout->addWidget(clearTerminalButton);
    controlLayout->addWidget(exitButton);
    logLayout->addLayout(controlLayout);

    rightLayout->addWidget(logGroup);

    rightLayout->addStretch();
}

void MainWindow::createSensorDisplay(const QString& name, const QString& defaultValue,
                                     QGridLayout* layout, int row, int col, QLabel*& label) {
    QFrame* frame = new QFrame();
    frame->setFrameStyle(QFrame::Box | QFrame::Raised);
    frame->setFixedSize(SENSOR_DISPLAY_WIDTH, SENSOR_DISPLAY_HEIGHT);
    frame->setStyleSheet(
        "QFrame {"
        "    background-color: #374151;"
        "    border: 1px solid #4B5563;"
        "    border-radius: 8px;"
        "    margin: 2px;"
        "}"
        );

    QVBoxLayout* frameLayout = new QVBoxLayout(frame);
    frameLayout->setSpacing(4);
    frameLayout->setContentsMargins(8, 8, 8, 8);

    // Sensor name
    QLabel* nameLabel = new QLabel(name);
    nameLabel->setStyleSheet("color: #9CA3AF; font-size: 11px; font-weight: bold;");
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);
    frameLayout->addWidget(nameLabel);

    // Sensor value
    label = new QLabel(defaultValue);
    label->setStyleSheet("color: #FFFFFF; font-size: 14px; font-weight: bold;");
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    frameLayout->addWidget(label);

    layout->addWidget(frame, row, col);
}

void MainWindow::setupConnections() {
    // Connection type selection
    connect(connectionTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onConnectionTypeChanged);

    // Bluetooth device selection
    connect(bluetoothDevicesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onBluetoothDeviceSelected);
    connect(scanBluetoothButton, &QPushButton::clicked, this, &MainWindow::onScanBluetoothClicked);

    // Module selection
    connect(moduleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModuleSelectionChanged);

    // Connection management
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);

    // Simplified diagnostic commands
    connect(readAllSensorsButton, &QPushButton::clicked, this, &MainWindow::onReadAllSensorsClicked);
    connect(readFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onReadFaultCodesClicked);
    connect(clearFaultCodesButton, &QPushButton::clicked, this, &MainWindow::onClearFaultCodesClicked);

    // Manual command and continuous reading
    connect(sendCommandButton, &QPushButton::clicked, this, &MainWindow::onSendCommandClicked);
    connect(continuousReadingCheckBox, &QCheckBox::toggled, this, &MainWindow::onContinuousReadingToggled);
    connect(readingIntervalSlider, &QSlider::valueChanged, this, &MainWindow::onReadingIntervalChanged);
    connect(commandLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendCommandClicked);
    connect(clearTerminalButton, &QPushButton::clicked, this, &MainWindow::onClearTerminalClicked);
    connect(exitButton, &QPushButton::clicked, this, &MainWindow::onExitClicked);

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

// Apply car stereo specific styling
void MainWindow::applyCarStereoStyling() {
    // Large, touch-friendly styling optimized for car environment
    setStyleSheet(
        "QMainWindow {"
        "    background-color: #0F172A;"
        "    color: #F8FAFC;"
        "}"
        "QPushButton {"
        "    background-color: #3B82F6;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    min-height: 40px;"
        "    padding: 8px 16px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #2563EB;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1D4ED8;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #374151;"
        "    color: #9CA3AF;"
        "}"
        "QComboBox {"
        "    background-color: #374151;"
        "    color: white;"
        "    border: 1px solid #4B5563;"
        "    border-radius: 6px;"
        "    padding: 8px 12px;"
        "    font-size: 13px;"
        "    min-height: 24px;"
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
        "    background-color: #374151;"
        "    border: 1px solid #4B5563;"
        "    border-radius: 6px;"
        "    selection-background-color: #3B82F6;"
        "    color: white;"
        "    font-size: 13px;"
        "}"
        "QGroupBox {"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    color: #E5E7EB;"
        "    border: 2px solid #4B5563;"
        "    border-radius: 8px;"
        "    margin-top: 12px;"
        "    padding-top: 8px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 12px;"
        "    padding: 0 8px 0 8px;"
        "    background-color: #1E293B;"
        "}"
        "QLabel {"
        "    color: #E5E7EB;"
        "    font-size: 12px;"
        "}"
        "QCheckBox {"
        "    color: #E5E7EB;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "}"
        "QCheckBox::indicator {"
        "    width: 20px;"
        "    height: 20px;"
        "}"
        "QSlider::groove:horizontal {"
        "    border: 1px solid #4B5563;"
        "    height: 8px;"
        "    background: #374151;"
        "    border-radius: 4px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: #3B82F6;"
        "    border: 2px solid #1E40AF;"
        "    width: 20px;"
        "    height: 20px;"
        "    border-radius: 10px;"
        "    margin: -6px 0;"
        "}"
        "QLineEdit {"
        "    background-color: #374151;"
        "    color: white;"
        "    border: 1px solid #4B5563;"
        "    border-radius: 4px;"
        "    padding: 6px;"
        "    font-size: 12px;"
        "}"
        );
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
#else \
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

// Simplified module switching
void MainWindow::onModuleSelectionChanged(int index) {
    if (!connected) return;

    WJModule module = MODULE_UNKNOWN;
    QString moduleName;

    switch (index) {
    case 0:
        module = MODULE_ENGINE_EDC15;
        moduleName = "Engine Control Module";
        break;
    case 1:
        module = MODULE_TRANSMISSION;
        moduleName = "Transmission Control Module";
        break;
    case 2:
        module = MODULE_PCM;
        moduleName = "Powertrain Control Module";
        break;
    case 3:
        module = MODULE_ABS;
        moduleName = "Anti-lock Brake System";
        break;
    default: return;
    }

    if (switchToModule(module)) {
        currentModule = module;
        currentModuleLabel->setText("Current: " + WJUtils::getModuleName(module));
        moduleTitle->setText(moduleName);

        // Update sensor display layout based on module
        updateSensorLayoutForModule(module);

        logWJData("✓ Switched to " + moduleName);
    }
}

void MainWindow::updateSensorLayoutForModule(WJModule module) {
    // Map the universal sensor labels to module-specific data
    switch (module) {
    case MODULE_ENGINE_EDC15:
        // Map engine sensors to universal labels
        mafActualLabel = sensor1Label;
        mafSpecifiedLabel = sensor2Label;
        railPressureActualLabel = sensor3Label;
        mapActualLabel = sensor4Label;
        coolantTempLabel = sensor5Label;
        intakeAirTempLabel = sensor6Label;
        throttlePositionLabel = sensor7Label;
        rpmLabel = sensor8Label;
        injectionQuantityLabel = sensor9Label;
        batteryVoltageLabel = sensor10Label;
        vehicleSpeedLabel = sensor11Label;
        engineLoadLabel = sensor12Label;
        break;

    case MODULE_TRANSMISSION:
        // Map transmission sensors to universal labels
        transOilTempLabel = sensor1Label;
        transInputSpeedLabel = sensor2Label;
        transOutputSpeedLabel = sensor3Label;
        transCurrentGearLabel = sensor4Label;
        transLinePressureLabel = sensor5Label;
        transSolenoidALabel = sensor6Label;
        transSolenoidBLabel = sensor7Label;
        transTCCSolenoidLabel = sensor8Label;
        transTorqueConverterLabel = sensor9Label;
        break;

    case MODULE_PCM:
        // Map PCM sensors to universal labels
        vehicleSpeedLabel = sensor1Label;
        engineLoadLabel = sensor2Label;
        fuelTrimSTLabel = sensor3Label;
        fuelTrimLTLabel = sensor4Label;
        o2Sensor1Label = sensor5Label;
        o2Sensor2Label = sensor6Label;
        timingAdvanceLabel = sensor7Label;
        barometricPressureLabel = sensor8Label;
        break;

    case MODULE_ABS:
        // Map ABS sensors to universal labels
        wheelSpeedFLLabel = sensor1Label;
        wheelSpeedFRLabel = sensor2Label;
        wheelSpeedRLLabel = sensor3Label;
        wheelSpeedRRLabel = sensor4Label;
        yawRateLabel = sensor5Label;
        lateralAccelLabel = sensor6Label;
        break;

    default:
        break;
    }

    // Update sensor display texts with appropriate labels
    updateSensorLabelsForModule(module);
}

void MainWindow::updateSensorLabelsForModule(WJModule module) {
    // Clear all sensor displays first
    for (int i = 1; i <= 12; ++i) {
        QLabel* label = nullptr;
        switch (i) {
        case 1: label = sensor1Label; break;
        case 2: label = sensor2Label; break;
        case 3: label = sensor3Label; break;
        case 4: label = sensor4Label; break;
        case 5: label = sensor5Label; break;
        case 6: label = sensor6Label; break;
        case 7: label = sensor7Label; break;
        case 8: label = sensor8Label; break;
        case 9: label = sensor9Label; break;
        case 10: label = sensor10Label; break;
        case 11: label = sensor11Label; break;
        case 12: label = sensor12Label; break;
        }
        if (label) {
            label->setText("--");
            label->setVisible(false);
        }
    }

    // Show and label sensors based on current module
    switch (module) {
    case MODULE_ENGINE_EDC15:
        sensor1Label->setText("MAF Actual: -- g/s");
        sensor2Label->setText("MAF Spec: -- g/s");
        sensor3Label->setText("Rail Press: -- bar");
        sensor4Label->setText("MAP: -- mbar");
        sensor5Label->setText("Coolant: -- °C");
        sensor6Label->setText("Intake: -- °C");
        sensor7Label->setText("Throttle: -- %");
        sensor8Label->setText("RPM: -- rpm");
        sensor9Label->setText("Inj Qty: -- mg");
        sensor10Label->setText("Battery: -- V");
        sensor11Label->setText("Speed: -- km/h");
        sensor12Label->setText("Load: -- %");

        for (int i = 1; i <= 12; ++i) {
            QLabel* label = nullptr;
            switch (i) {
            case 1: label = sensor1Label; break;
            case 2: label = sensor2Label; break;
            case 3: label = sensor3Label; break;
            case 4: label = sensor4Label; break;
            case 5: label = sensor5Label; break;
            case 6: label = sensor6Label; break;
            case 7: label = sensor7Label; break;
            case 8: label = sensor8Label; break;
            case 9: label = sensor9Label; break;
            case 10: label = sensor10Label; break;
            case 11: label = sensor11Label; break;
            case 12: label = sensor12Label; break;
            }
            if (label) label->setVisible(true);
        }
        break;

    case MODULE_TRANSMISSION:
        sensor1Label->setText("Oil Temp: -- °C");
        sensor2Label->setText("Input: -- rpm");
        sensor3Label->setText("Output: -- rpm");
        sensor4Label->setText("Gear: --");
        sensor5Label->setText("Line Press: -- psi");
        sensor6Label->setText("Sol A: -- %");
        sensor7Label->setText("Sol B: -- %");
        sensor8Label->setText("TCC: -- %");
        sensor9Label->setText("TC: -- %");

        for (int i = 1; i <= 9; ++i) {
            QLabel* label = nullptr;
            switch (i) {
            case 1: label = sensor1Label; break;
            case 2: label = sensor2Label; break;
            case 3: label = sensor3Label; break;
            case 4: label = sensor4Label; break;
            case 5: label = sensor5Label; break;
            case 6: label = sensor6Label; break;
            case 7: label = sensor7Label; break;
            case 8: label = sensor8Label; break;
            case 9: label = sensor9Label; break;
            }
            if (label) label->setVisible(true);
        }
        break;

    case MODULE_PCM:
        sensor1Label->setText("Speed: -- km/h");
        sensor2Label->setText("Load: -- %");
        sensor3Label->setText("FT ST: -- %");
        sensor4Label->setText("FT LT: -- %");
        sensor5Label->setText("O2 1: -- V");
        sensor6Label->setText("O2 2: -- V");
        sensor7Label->setText("Timing: -- °");
        sensor8Label->setText("Baro: -- kPa");

        for (int i = 1; i <= 8; ++i) {
            QLabel* label = nullptr;
            switch (i) {
            case 1: label = sensor1Label; break;
            case 2: label = sensor2Label; break;
            case 3: label = sensor3Label; break;
            case 4: label = sensor4Label; break;
            case 5: label = sensor5Label; break;
            case 6: label = sensor6Label; break;
            case 7: label = sensor7Label; break;
            case 8: label = sensor8Label; break;
            }
            if (label) label->setVisible(true);
        }
        break;

    case MODULE_ABS:
        sensor1Label->setText("FL: -- km/h");
        sensor2Label->setText("FR: -- km/h");
        sensor3Label->setText("RL: -- km/h");
        sensor4Label->setText("RR: -- km/h");
        sensor5Label->setText("Yaw: -- deg/s");
        sensor6Label->setText("Lat G: -- g");

        for (int i = 1; i <= 6; ++i) {
            QLabel* label = nullptr;
            switch (i) {
            case 1: label = sensor1Label; break;
            case 2: label = sensor2Label; break;
            case 3: label = sensor3Label; break;
            case 4: label = sensor4Label; break;
            case 5: label = sensor5Label; break;
            case 6: label = sensor6Label; break;
            }
            if (label) label->setVisible(true);
        }
        break;

    default:
        break;
    }
}

// Update the connection methods to enable/disable the simplified controls
void MainWindow::updateControlsForConnection(bool connected) {
    connectButton->setEnabled(!connected);
    disconnectButton->setEnabled(connected);
    moduleCombo->setEnabled(connected);
    readAllSensorsButton->setEnabled(connected);
    readFaultCodesButton->setEnabled(connected);
    clearFaultCodesButton->setEnabled(connected);
    continuousReadingCheckBox->setEnabled(connected);
    sendCommandButton->setEnabled(connected);
}

// Protocol and Module Management
void MainWindow::onProtocolSwitchRequested(WJProtocol protocol) {
    if (!connected) {
        logWJData("❌ Not connected - cannot switch protocol");
        return;
    }

    if (switchToProtocol(protocol)) {
        currentProtocol = protocol;
        logWJData("✓ Switched to protocol: " + WJUtils::getProtocolName(protocol));
    } else {
        logWJData("❌ Failed to switch to protocol: " + WJUtils::getProtocolName(protocol));
    }
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
    currentProtocol = protocol;

    logWJData(QString("✓ Protocol switched to: %1")
                  .arg(WJUtils::getProtocolName(protocol)));
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

    currentModule = module;
    currentModuleLabel->setText("Current: " + WJUtils::getModuleName(module));

    try {
        // Get module initialization commands with null check
        QList<WJCommand> moduleCommands;

        // Check if the module is valid before getting commands
        if (module == MODULE_UNKNOWN) {
            logWJData("⚠️ Invalid module - skipping module commands");
        } else {
            moduleCommands = WJCommands::getModuleInitCommands(module);
        }

        if (!moduleCommands.isEmpty()) {
            // Execute commands with additional safety checks
            for (int i = 0; i < moduleCommands.size(); ++i) {
                const WJCommand& cmd = moduleCommands[i];

                // Check if command is valid before sending
                if (cmd.command.isEmpty()) {
                    logWJData(QString("⚠️ Skipping empty command at index %1").arg(i));
                    continue;
                }

                // Check if still connected before each command
                if (!connected || !connectionManager) {
                    logWJData("❌ Connection lost during module switch");
                    return false;
                }

                sendWJCommand(cmd.command, module);

                // Safer delay with bounds checking
                int delayMs = qMax(10, cmd.timeoutMs / 20); // Minimum 10ms delay
                QThread::msleep(delayMs);
            }
        } else {
            logWJData("→ No module-specific commands to execute");
        }
        return true;

    } catch (const std::exception& e) {
        logWJData(QString("❌ Exception during module switch: %1").arg(e.what()));
        return false;
    } catch (...) {
        logWJData("❌ Unknown exception during module switch");
        return false;
    }
    return true;
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

    updateControlsForConnection(false);
    connectionStatusLabel->setText("Status: Disconnected");
    protocolLabel->setText("Protocol: Unknown");
    currentModuleLabel->setText("Current: None");

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

    // Start with engine module (ISO_14230_4_KWP_FAST) initialization
    const WJCommand& firstCmd = initializationCommands[0];
    logWJData("→ " + firstCmd.description + ": " + firstCmd.command);

    lastSentCommand = firstCmd.command;
    connectionManager->send(firstCmd.command);

    return true;
}

void MainWindow::processWJInitResponse(const QString& response) {
    if (currentInitStep >= initializationCommands.size()) {
        // Initialization complete - even with some errors
        currentInitState = STATE_READY_ISO9141;
        initialized = true;
        connectionStatusLabel->setText("Status: Ready");

        logWJData("✓ WJ initialization completed!");
        logWJData(QString("→ Engine security access: %1").arg(engineSecurityAccessGranted ? "Granted" : "Limited"));
        logWJData("→ Basic diagnostics available");

        // Enable diagnostic buttons
        updateControlsForConnection(true);

        initializationTimer->stop();

        // Set initial protocol and module
        currentProtocol = PROTOCOL_ISO_14230_4_KWP_FAST;
        currentModule = MODULE_ENGINE_EDC15;
        currentModuleLabel->setText("Current: " + WJUtils::getModuleName(currentModule));
        protocolLabel->setText("Protocol: Ready");

        // Test basic communication
        QTimer::singleShot(2000, [this]() {
            onReadAllSensorsClicked();
        });

        return;
    }

    const WJCommand& currentCmd = initializationCommands[currentInitStep];
    QString cleanResponse = removeCommandEcho(response).trimmed().toUpper();
    bool responseOK = false;
    bool shouldAdvanceStep = true;

    // Handle errors more gracefully
    if (isWJError(cleanResponse)) {
        if (currentCmd.isCritical) {
            // Critical command failed
            logWJData("❌ Critical command failed: " + currentCmd.command + " - " + cleanResponse);
            responseOK = true; // Continue anyway
        } else {
            // Non-critical command failed - just continue
            logWJData("⚠️ Non-critical command failed: " + currentCmd.command + " - " + cleanResponse);
            responseOK = true; // Continue
        }
    }
    // Handle specific responses
    else if (currentCmd.command == "ATZ") {
        currentInitState = STATE_RESETTING;
        if (cleanResponse.contains("ELM327") || cleanResponse.isEmpty()) {
            logWJData("✓ ELM327 reset successful");
            responseOK = true;
        }
    }
    else if (currentCmd.command.startsWith("AT")) {
        if (cleanResponse.contains("OK") || cleanResponse.isEmpty()) {
            logWJData("✓ " + currentCmd.description);
            responseOK = true;
        }
    }
    else if (currentCmd.command == "81") {
        // Start communication - don't require success
        if (cleanResponse.contains("BUS INIT") || cleanResponse.contains("ERROR")) {
            logWJData("⚠️ Engine communication attempted: " + cleanResponse);
            responseOK = true; // Continue even with error
        }
    }
    else if (currentCmd.command.startsWith("27")) {
        // Security access - don't require success
        logWJData("⚠️ Security access attempted: " + cleanResponse);
        responseOK = true; // Continue regardless
        engineSecurityAccessGranted = cleanResponse.contains("67");
    }
    else {
        // Other commands
        logWJData("→ " + currentCmd.description + ": " + cleanResponse);
        responseOK = true; // Be permissive
    }

    // Always advance to next step unless explicitly told not to
    if (shouldAdvanceStep) {
        currentInitStep++;

        if (currentInitStep < initializationCommands.size()) {
            const WJCommand& nextCmd = initializationCommands[currentInitStep];

            // Use the command's timeout
            int delay = nextCmd.timeoutMs;

            QTimer::singleShot(delay, [this, nextCmd]() {
                if (connected && connectionManager) {
                    logWJData("→ " + nextCmd.description + ": " + nextCmd.command);
                    lastSentCommand = nextCmd.command;
                    connectionManager->send(nextCmd.command);
                }
            });
        }
    }
}

void MainWindow::onInitializationTimeout() {
    logWJData("⚠️ WJ initialization timeout - continuing with basic functionality");

    // Don't disconnect! Just mark as partially initialized
    initialized = true;
    currentInitState = STATE_READY_ISO9141;

    connectionStatusLabel->setText("Status: Ready (Limited)");
    protocolLabel->setText("Protocol: Basic Mode");

    // Enable basic functionality
    updateControlsForConnection(true);

    logWJData("→ Basic diagnostics available");

    // Set basic state
    currentProtocol = PROTOCOL_ISO_14230_4_KWP_FAST;
    currentModule = MODULE_ENGINE_EDC15;
    currentModuleLabel->setText("Current: Engine (Limited)");
}

void MainWindow::sendWJCommand(const QString& command, WJModule targetModule) {
    if (!connected || !connectionManager) {
        return;
    }

    QString cleanCommand = cleanWJData(command);

    // Only log a warning if there's a module mismatch, don't try to fix it
    if (targetModule != MODULE_UNKNOWN && targetModule != currentModule) {
        logWJData(QString("⚠️ Module mismatch: current=%1, target=%2 (continuing anyway)")
                      .arg(WJUtils::getModuleName(currentModule))
                      .arg(WJUtils::getModuleName(targetModule)));
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

    // Add to buffer
    dataBuffer += data;

    // Handle different line endings (\n, \r\n, \r)
    dataBuffer.replace("\r\n", "\n"); // Windows style
    dataBuffer.replace("\r", "\n");   // Mac style

    // Split by newlines
    QStringList lines = dataBuffer.split('\n');

    // Keep the last part if it doesn't end with newline
    if (!dataBuffer.endsWith('\n')) {
        dataBuffer = lines.takeLast();
    } else {
        dataBuffer.clear();
    }

    // Process each complete line
    for (const QString& line : lines) {
        if (!line.trimmed().isEmpty()) {
            processDataLine(line.trimmed());
        }
    }

    // Log buffer status for debugging
    if (!dataBuffer.isEmpty() && dataBuffer.length() > 100) {
        qDebug() << "Large buffer without newline:" << dataBuffer.length() << "chars";
    }
}

void MainWindow::processDataLine(const QString& line)
{
    if (line.isEmpty()) {
        return;
    }

    // Clean the line
    QString cleanData = line;
    cleanData.remove("\r");
    cleanData.remove(">");
    cleanData.remove("?");
    cleanData.remove(QChar(0xFFFD));
    cleanData.remove(QChar(0x00));
    cleanData = cleanData.trimmed(); // Remove leading/trailing whitespace

    if (cleanData.isEmpty()) {
        return;
    }

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

void MainWindow::logWJData(const QString& message) {
    // Show important messages and device info
    if (message.startsWith("→") ||       // Commands
        message.startsWith("←") ||       // Responses
        message.startsWith("❌") ||      // Errors
        message.startsWith("✓") ||       // Success
        message.startsWith("⚠️") ||      // Warnings
        message.startsWith("===") ||     // Section headers (=== DEVICE INFORMATION ===)
        message.contains("DEVICE") ||    // Device info
        message.contains("Screen") ||    // Screen info
        message.contains("Resolution") || // Screen resolution
        message.contains("Refresh") ||   // Refresh rate
        message.contains("Platform") ||  // Platform info
        message.contains("Windows") ||   // Windows info
        message.contains("Android") ||   // Android info
        message.contains("CPU") ||       // CPU info
        message.contains("Device:") ||   // Android device info
        message.contains("RAM:") ||      // Memory info
        message.contains("Storage") ||   // Storage info
        message.contains("Qt") ||        // Qt info
        message.contains("Tool") ||      // App info
        message.contains("WiFi") ||      // Connection info
        message.contains("Jeep") ||      // App title
        message.contains("Computer") ||  // Computer name
        message.contains("User") ||      // User name
        message.contains("Processor") || // Processor info
        message.contains("Architecture") || // Architecture info
        message.contains("Version") ||   // Version info
        message.contains("Kernel") ||    // Kernel info
        message.contains("Physical")) {  // Physical size

        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        terminalDisplay->append(QString("[%1] %2").arg(timestamp, message));
        terminalDisplay->moveCursor(QTextCursor::End);
    }

    // Keep terminal manageable
    if (terminalDisplay->document()->blockCount() > 50) {
        QTextCursor cursor = terminalDisplay->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 10);
        cursor.removeSelectedText();
    }
}

void MainWindow::parseWJResponse(const QString& response) {
    QString cleanResponse = cleanWJData(response);

    if (cleanResponse.isEmpty()) {
        return; // Don't process empty responses
    }

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

    // Update displays with safety check
    try {
        updateSensorDisplays();
    } catch (...) {
        // Log error but don't crash
        logWJData("❌ Error updating sensor displays");
    }
}

// Simplified diagnostic command implementations
void MainWindow::onReadAllSensorsClicked() {
    if (!connected || !initialized) {
        logWJData("❌ Not connected or initialized");
        return;
    }

    logWJData("→ Reading all sensors for " + WJUtils::getModuleName(currentModule) + "...");

    switch (currentModule) {
    case MODULE_ENGINE_EDC15:
        executeEngineCommands();
        break;
    case MODULE_TRANSMISSION:
        executeTransmissionCommands();
        break;
    case MODULE_PCM:
        executePCMCommands();
        break;
    case MODULE_ABS:
        executeABSCommands();
        break;
    default:
        logWJData("❌ Unknown module selected");
        return;
    }
}

void MainWindow::onReadFaultCodesClicked() {
    if (!connected || !initialized) {
        logWJData("❌ Not connected or initialized");
        return;
    }

    logWJData("→ Reading fault codes for " + WJUtils::getModuleName(currentModule) + "...");

    switch (currentModule) {
    case MODULE_ENGINE_EDC15:
        sendWJCommand(WJ::Engine::READ_DTC, MODULE_ENGINE_EDC15);
        break;
    case MODULE_TRANSMISSION:
        sendWJCommand(WJ::Transmission::READ_DTC, MODULE_TRANSMISSION);
        break;
    case MODULE_PCM:
        sendWJCommand(WJ::PCM::READ_DTC, MODULE_PCM);
        break;
    case MODULE_ABS:
        sendWJCommand(WJ::ABS::READ_DTC, MODULE_ABS);
        break;
    default:
        logWJData("❌ Unknown module selected");
        return;
    }
}

void MainWindow::onClearFaultCodesClicked() {
    if (!connected || !initialized) {
        logWJData("❌ Not connected or initialized");
        return;
    }

    QString moduleName = WJUtils::getModuleName(currentModule);
    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                                              "Clear Fault Codes",
                                                              QString("Are you sure you want to clear all fault codes from %1?\n\nThis action cannot be undone.")
                                                                  .arg(moduleName),
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    logWJData("→ Clearing fault codes for " + moduleName + "...");

    switch (currentModule) {
    case MODULE_ENGINE_EDC15:
        sendWJCommand(WJ::Engine::CLEAR_DTC, MODULE_ENGINE_EDC15);
        break;
    case MODULE_TRANSMISSION:
        sendWJCommand(WJ::Transmission::CLEAR_DTC, MODULE_TRANSMISSION);
        break;
    case MODULE_PCM:
        sendWJCommand(WJ::PCM::CLEAR_DTC, MODULE_PCM);
        break;
    case MODULE_ABS:
        sendWJCommand(WJ::ABS::CLEAR_DTC, MODULE_ABS);
        break;
    default:
        logWJData("❌ Unknown module selected");
        return;
    }

    // Clear the fault code display
    faultCodeList->clear();
}

// Command execution methods for each module
void MainWindow::executeEngineCommands() {
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
            if (connected && currentModule == MODULE_ENGINE_EDC15) {
                sendWJCommand(commands[i], MODULE_ENGINE_EDC15);
            }
        });
    }
}

void MainWindow::executeTransmissionCommands() {
    QStringList commands = {
        WJ::Transmission::READ_TRANS_DATA,
        WJ::Transmission::READ_SPEED_DATA,
        WJ::Transmission::READ_SOLENOID_STATUS
    };

    for (int i = 0; i < commands.size(); ++i) {
        QTimer::singleShot(i * 400, [this, commands, i]() {
            if (connected && currentModule == MODULE_TRANSMISSION) {
                sendWJCommand(commands[i], MODULE_TRANSMISSION);
            }
        });
    }
}

void MainWindow::executePCMCommands() {
    QStringList commands = {
        WJ::PCM::READ_LIVE_DATA,
        WJ::PCM::READ_FUEL_TRIM,
        WJ::PCM::READ_O2_SENSORS
    };

    for (int i = 0; i < commands.size(); ++i) {
        QTimer::singleShot(i * 400, [this, commands, i]() {
            if (connected && currentModule == MODULE_PCM) {
                sendWJCommand(commands[i], MODULE_PCM);
            }
        });
    }
}

void MainWindow::executeABSCommands() {
    QStringList commands = {
        WJ::ABS::READ_WHEEL_SPEEDS,
        WJ::ABS::READ_STABILITY_DATA
    };

    for (int i = 0; i < commands.size(); ++i) {
        QTimer::singleShot(i * 400, [this, commands, i]() {
            if (connected && currentModule == MODULE_ABS) {
                sendWJCommand(commands[i], MODULE_ABS);
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
    terminalDisplay->clear();
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
    intervalLabel->setText(QString::number(intervalMs) + "ms");

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

    // Read data based on current module
    switch (currentModule) {
    case MODULE_ENGINE_EDC15:
        sendWJCommand(WJ::Engine::READ_MAF_DATA, MODULE_ENGINE_EDC15);
        break;
    case MODULE_TRANSMISSION:
        sendWJCommand(WJ::Transmission::READ_TRANS_DATA, MODULE_TRANSMISSION);
        break;
    case MODULE_PCM:
        sendWJCommand(WJ::PCM::READ_LIVE_DATA, MODULE_PCM);
        break;
    case MODULE_ABS:
        sendWJCommand(WJ::ABS::READ_WHEEL_SPEEDS, MODULE_ABS);
        break;
    default:
        break;
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

bool MainWindow::isImportantMessage(const QString& message)
{
    // Only show these types of messages in minimal mode:

    // Connection events
    if (message.contains("Physical connection established") ||
        message.contains("Disconnected") ||
        message.contains("Connection failed")) {
        return true;
    }

    // Initialization complete
    if (message.contains("WJ multi-protocol initialization completed") ||
        message.contains("Ready for multi-module diagnostics")) {
        return true;
    }

    // Module switches (only the result)
    if (message.contains("Successfully switched to module") ||
        message.contains("✓ Switched to module")) {
        return true;
    }

    // Protocol switches (only the result)
    if (message.contains("✓ Protocol switched to")) {
        return true;
    }

    // Errors
    if (message.startsWith("❌") || message.contains("Error") || message.contains("Failed")) {
        return true;
    }

    // Critical warnings
    if (message.startsWith("⚠️") && (message.contains("Security") || message.contains("Critical"))) {
        return true;
    }

    // Data reading results (sensor values)
    if (message.contains("Battery:") ||
        message.contains("RPM:") ||
        message.contains("Temperature:") ||
        message.contains("Pressure:")) {
        return true;
    }

    // Fault codes
    if (message.contains("fault code") || message.contains("DTC")) {
        return true;
    }

    // App startup/shutdown
    if (message.contains("Jeep WJ Diagnostic Tool") ||
        message.contains("Exiting application")) {
        return true;
    }

    return false; // Hide everything else in minimal mode
}

// Display Update Methods
void MainWindow::updateSensorDisplays() {
    updateEngineDisplay();
    updateTransmissionDisplay();
    updatePCMDisplay();
    updateABSDisplay();
}

void MainWindow::updateEngineDisplay() {
    if (!sensorData.engine.dataValid) {
        return; // Don't update if no valid data
    }

    // Add null pointer checks for ALL labels before calling setText()
    if (mafActualLabel) {
        mafActualLabel->setText(formatSensorValue(sensorData.engine.mafActual, "g/s", 1));
    }
    if (mafSpecifiedLabel) {
        mafSpecifiedLabel->setText(formatSensorValue(sensorData.engine.mafSpecified, "g/s", 1));
    }
    if (railPressureActualLabel) {
        railPressureActualLabel->setText(formatSensorValue(sensorData.engine.railPressureActual, "bar", 1));
    }
    if (railPressureSpecifiedLabel) {
        railPressureSpecifiedLabel->setText(formatSensorValue(sensorData.engine.railPressureSpecified, "bar", 1));
    }
    if (mapActualLabel) {
        mapActualLabel->setText(formatSensorValue(sensorData.engine.mapActual, "mbar", 0));
    }
    if (mapSpecifiedLabel) {
        mapSpecifiedLabel->setText(formatSensorValue(sensorData.engine.mapSpecified, "mbar", 0));
    }
    if (coolantTempLabel) {
        coolantTempLabel->setText(formatSensorValue(sensorData.engine.coolantTemp, "°C", 1));
    }
    if (intakeAirTempLabel) {
        intakeAirTempLabel->setText(formatSensorValue(sensorData.engine.intakeAirTemp, "°C", 1));
    }
    if (throttlePositionLabel) {
        throttlePositionLabel->setText(formatSensorValue(sensorData.engine.throttlePosition, "%", 1));
    }
    if (rpmLabel) {
        rpmLabel->setText(formatSensorValue(sensorData.engine.engineRPM, "rpm", 0));
    }
    if (injectionQuantityLabel) {
        injectionQuantityLabel->setText(formatSensorValue(sensorData.engine.injectionQuantity, "mg", 1));
    }
    if (batteryVoltageLabel) {
        batteryVoltageLabel->setText(formatSensorValue(sensorData.engine.batteryVoltage, "V", 2));
    }

    if (injector1Label) {
        injector1Label->setText(formatSensorValue(sensorData.engine.injector1Correction, "mg", 2));
    }
    if (injector2Label) {
        injector2Label->setText(formatSensorValue(sensorData.engine.injector2Correction, "mg", 2));
    }
    if (injector3Label) {
        injector3Label->setText(formatSensorValue(sensorData.engine.injector3Correction, "mg", 2));
    }
    if (injector4Label) {
        injector4Label->setText(formatSensorValue(sensorData.engine.injector4Correction, "mg", 2));
    }
    if (injector5Label) {
        injector5Label->setText(formatSensorValue(sensorData.engine.injector5Correction, "mg", 2));
    }
}

void MainWindow::updateTransmissionDisplay() {
    if (!sensorData.transmission.dataValid) {
        return;
    }

    if (transOilTempLabel) {
        transOilTempLabel->setText(formatSensorValue(sensorData.transmission.oilTemp, "°C", 1));
    }
    if (transInputSpeedLabel) {
        transInputSpeedLabel->setText(formatSensorValue(sensorData.transmission.inputSpeed, "rpm", 0));
    }
    if (transOutputSpeedLabel) {
        transOutputSpeedLabel->setText(formatSensorValue(sensorData.transmission.outputSpeed, "rpm", 0));
    }
    if (transCurrentGearLabel) {
        transCurrentGearLabel->setText(formatSensorValue(sensorData.transmission.currentGear, "", 0));
    }
    if (transLinePressureLabel) {
        transLinePressureLabel->setText(formatSensorValue(sensorData.transmission.linePresssure, "psi", 1));
    }
    if (transSolenoidALabel) {
        transSolenoidALabel->setText(formatSensorValue(sensorData.transmission.shiftSolenoidA, "%", 1));
    }
    if (transSolenoidBLabel) {
        transSolenoidBLabel->setText(formatSensorValue(sensorData.transmission.shiftSolenoidB, "%", 1));
    }
    if (transTCCSolenoidLabel) {
        transTCCSolenoidLabel->setText(formatSensorValue(sensorData.transmission.tccSolenoid, "%", 1));
    }
    if (transTorqueConverterLabel) {
        transTorqueConverterLabel->setText(formatSensorValue(sensorData.transmission.torqueConverter, "%", 1));
    }
}

void MainWindow::updatePCMDisplay() {
    if (!sensorData.pcm.dataValid) {
        return;
    }

    if (vehicleSpeedLabel) {
        vehicleSpeedLabel->setText(formatSensorValue(sensorData.pcm.vehicleSpeed, "km/h", 0));
    }
    if (engineLoadLabel) {
        engineLoadLabel->setText(formatSensorValue(sensorData.pcm.engineLoad, "%", 1));
    }
    if (fuelTrimSTLabel) {
        fuelTrimSTLabel->setText(formatSensorValue(sensorData.pcm.fuelTrimST, "%", 2));
    }
    if (fuelTrimLTLabel) {
        fuelTrimLTLabel->setText(formatSensorValue(sensorData.pcm.fuelTrimLT, "%", 2));
    }
    if (o2Sensor1Label) {
        o2Sensor1Label->setText(formatSensorValue(sensorData.pcm.o2Sensor1, "V", 3));
    }
    if (o2Sensor2Label) {
        o2Sensor2Label->setText(formatSensorValue(sensorData.pcm.o2Sensor2, "V", 3));
    }
    if (timingAdvanceLabel) {
        timingAdvanceLabel->setText(formatSensorValue(sensorData.pcm.timingAdvance, "°", 1));
    }
    if (barometricPressureLabel) {
        barometricPressureLabel->setText(formatSensorValue(sensorData.pcm.barometricPressure, "kPa", 1));
    }
}

void MainWindow::updateABSDisplay() {
    if (!sensorData.abs.dataValid) {
        return;
    }

    if (wheelSpeedFLLabel) {
        wheelSpeedFLLabel->setText(formatSensorValue(sensorData.abs.wheelSpeedFL, "km/h", 1));
    }
    if (wheelSpeedFRLabel) {
        wheelSpeedFRLabel->setText(formatSensorValue(sensorData.abs.wheelSpeedFR, "km/h", 1));
    }
    if (wheelSpeedRLLabel) {
        wheelSpeedRLLabel->setText(formatSensorValue(sensorData.abs.wheelSpeedRL, "km/h", 1));
    }
    if (wheelSpeedRRLabel) {
        wheelSpeedRRLabel->setText(formatSensorValue(sensorData.abs.wheelSpeedRR, "km/h", 1));
    }
    if (yawRateLabel) {
        yawRateLabel->setText(formatSensorValue(sensorData.abs.yawRate, "deg/s", 2));
    }
    if (lateralAccelLabel) {
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

// Updated fault code display for simplified list widget
void MainWindow::displayFaultCodes(const QList<WJ_DTC>& dtcs, const QString& moduleLabel) {
    if (dtcs.isEmpty()) {
        logWJData("✓ No fault codes found in " + moduleLabel);
        return;
    }

    // Clear existing fault codes
    faultCodeList->clear();

    // Add fault codes to the list
    for (const WJ_DTC& dtc : dtcs) {
        QString status = dtc.confirmed ? "Confirmed" : "Pending";
        QString itemText = QString("%1: %2 [%3]").arg(dtc.code, dtc.description, status);

        QListWidgetItem* item = new QListWidgetItem(itemText);

        // Color coding for different severity levels
        if (WJ_DTCs::isCriticalDTC(dtc.code, dtc.sourceModule)) {
            item->setForeground(QColor(220, 38, 38)); // Red for critical
            item->setData(Qt::UserRole, "CRITICAL");
        } else if (dtc.confirmed) {
            item->setForeground(QColor(245, 158, 11)); // Orange for confirmed
            item->setData(Qt::UserRole, "CONFIRMED");
        } else {
            item->setForeground(QColor(156, 163, 175)); // Gray for pending
            item->setData(Qt::UserRole, "PENDING");
        }

        faultCodeList->addItem(item);
    }

    // Update terminal log
    logWJData(QString("✓ Found %1 fault code(s) in %2").arg(dtcs.size()).arg(moduleLabel));
    for (const WJ_DTC& dtc : dtcs) {
        QString criticality = WJ_DTCs::isCriticalDTC(dtc.code, dtc.sourceModule) ? " [CRITICAL]" : "";
        logWJData(QString("  %1: %2%3").arg(dtc.code, dtc.description, criticality));
    }
}

// Fault Code Management
void MainWindow::clearFaultCodesForModule(WJModule module) {
    QString moduleLabel = WJUtils::getModuleName(module);

    // Clear the fault code list
    faultCodeList->clear();

    logWJData("✓ Cleared fault codes display for " + moduleLabel);
}

// Additional utility function for DTC code formatting
QString MainWindow::formatDTCCode(int byte1, int byte2) {
    return WJUtils::formatDTCCode(byte1, byte2, currentProtocol);
}

void MainWindow::logDeviceInformation() {
    // Screen Resolution & Refresh Rate
    QScreen *primaryScreen = QApplication::primaryScreen();
    if (primaryScreen) {
        QRect screenGeometry = primaryScreen->geometry();
        qreal devicePixelRatio = primaryScreen->devicePixelRatio();
        int refreshRate = primaryScreen->refreshRate();

        logWJData(QString("Resolution: %1x%2 (DPR: %3)")
                      .arg(screenGeometry.width())
                      .arg(screenGeometry.height())
                      .arg(devicePixelRatio));

        logWJData(QString("Refresh Rate: %1 Hz").arg(refreshRate));
    }

    // Platform & Architecture
    logWJData(QString("Platform: %1").arg(QSysInfo::prettyProductName()));
    logWJData(QString("Architecture: %1").arg(QSysInfo::currentCpuArchitecture()));

    // CPU Information
    logWJData(QString("CPU Cores: %1").arg(QThread::idealThreadCount()));

#ifdef Q_OS_WIN
    // Windows CPU details
    QString processor = qgetenv("PROCESSOR_IDENTIFIER");
    if (!processor.isEmpty()) {
        logWJData(QString("CPU: %1").arg(processor));
    }

    // Windows Memory Information
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        DWORDLONG totalPhysMB = memInfo.ullTotalPhys / (1024 * 1024);
        DWORDLONG availPhysMB = memInfo.ullAvailPhys / (1024 * 1024);
        DWORD memoryLoad = memInfo.dwMemoryLoad;

        logWJData(QString("RAM: %1 MB total, %2 MB available (%3% used)")
                      .arg(totalPhysMB)
                      .arg(availPhysMB)
                      .arg(memoryLoad));
    }

    // Storage Information for C: drive
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExW(L"C:\\", &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        ULONGLONG totalGB = totalNumberOfBytes.QuadPart / (1024 * 1024 * 1024);
        ULONGLONG freeGB = totalNumberOfFreeBytes.QuadPart / (1024 * 1024 * 1024);
        ULONGLONG usedGB = totalGB - freeGB;

        logWJData(QString("Storage C:\\: %1 GB total, %2 GB used, %3 GB free")
                      .arg(totalGB)
                      .arg(usedGB)
                      .arg(freeGB));
    }

#elif defined(Q_OS_ANDROID)
    // Android-specific information
    // Get device manufacturer and model using QJniObject
    QJniObject manufacturer = QJniObject::getStaticObjectField("android/os/Build",
                                                               "MANUFACTURER",
                                                               "Ljava/lang/String;");
    QJniObject model = QJniObject::getStaticObjectField("android/os/Build",
                                                        "MODEL",
                                                        "Ljava/lang/String;");
    QJniObject hardware = QJniObject::getStaticObjectField("android/os/Build",
                                                           "HARDWARE",
                                                           "Ljava/lang/String;");

    if (manufacturer.isValid() && model.isValid()) {
        logWJData(QString("Device: %1 %2").arg(manufacturer.toString(), model.toString()));
    }
    if (hardware.isValid()) {
        logWJData(QString("CPU: %1").arg(hardware.toString()));
    }

    // Try to get memory information via Java reflection
    QJniObject context = QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative",
                                                            "activity",
                                                            "()Landroid/app/Activity;");
    if (context.isValid()) {
        QJniObject activityServiceString = QJniObject::fromString("activity");
        QJniObject activityManager = context.callObjectMethod("getSystemService",
                                                              "(Ljava/lang/String;)Ljava/lang/Object;",
                                                              activityServiceString.object());
        if (activityManager.isValid()) {
            QJniObject memInfo("android/app/ActivityManager$MemoryInfo");
            activityManager.callMethod<void>("getMemoryInfo",
                                             "(Landroid/app/ActivityManager$MemoryInfo;)V",
                                             memInfo.object());

            if (memInfo.isValid()) {
                jlong totalMem = memInfo.getField<jlong>("totalMem");
                jlong availMem = memInfo.getField<jlong>("availMem");

                if (totalMem > 0) {
                    jlong usedMem = totalMem - availMem;
                    int usedPercent = (int)((usedMem * 100) / totalMem);

                    logWJData(QString("RAM: %1 MB total, %2 MB available (%3% used)")
                                  .arg(totalMem / (1024 * 1024))
                                  .arg(availMem / (1024 * 1024))
                                  .arg(usedPercent));
                }
            }
        }

        // Get storage information for internal storage
        QJniObject dataPath = QJniObject::fromString("/data");
        QJniObject statFs("android/os/StatFs", "(Ljava/lang/String;)V", dataPath.object());
        if (statFs.isValid()) {
            jlong blockSize = statFs.callMethod<jlong>("getBlockSizeLong");
            jlong totalBlocks = statFs.callMethod<jlong>("getBlockCountLong");
            jlong availableBlocks = statFs.callMethod<jlong>("getAvailableBlocksLong");

            if (blockSize > 0 && totalBlocks > 0) {
                jlong totalBytes = totalBlocks * blockSize;
                jlong availableBytes = availableBlocks * blockSize;
                jlong usedBytes = totalBytes - availableBytes;

                logWJData(QString("Storage: %1 GB total, %2 GB used, %3 GB free")
                              .arg(totalBytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1)
                              .arg(usedBytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1)
                              .arg(availableBytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1));
            }
        }
    } else {
        // Fallback: Basic info only
        logWJData("RAM & Storage: Android context not available");
    }

#else
    // Linux/Other platforms - basic info
    logWJData("RAM & Storage: Platform not supported for detailed info");

#endif
}
