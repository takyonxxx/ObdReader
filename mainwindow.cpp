#include "mainwindow.h"
#include "obdscan.h"
#include "global.h"
#include <QApplication>
#include <QCoreApplication>
#include <QScreen>
#include <QThread>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Initialize screen and window properties
    setWindowTitle("OBD-II Diagnostic Interface");
    QScreen *screen = window()->screen();
    desktopRect = screen->availableGeometry();

    // Setup UI programmatically
    setupUi();

    // Apply styling
    applyStyles();

    // Initialize OBD components
    elm = ELM::getInstance();
    elm->resetPids();

    m_settingsManager = SettingsManager::getInstance();
    if(m_settingsManager) {
        saveSettings();
        textTerminal->append("Wifi Ip: " + m_settingsManager->getWifiIp() + " : " +
                           QString::number(m_settingsManager->getWifiPort()));
    }

    m_connectionManager = ConnectionManager::getInstance();

    // Setup connections and interval slider
    setupConnections();
    setupIntervalSlider();

    // Set protocol default
    protocolCombo->setCurrentIndex(3);

    // Display initial status
    textTerminal->append("Resolution : " + QString::number(desktopRect.width()) +
                       "x" + QString::number(desktopRect.height()));
}

MainWindow::~MainWindow()
{
    if(m_connectionManager) {
        m_connectionManager->disConnectElm();
    }

    if(m_settingsManager) {
        m_settingsManager->saveSettings();
    }
}

bool MainWindow::initializeELM327()
{
    if (!m_connected || initializeCommands.isEmpty()) {
        return false;
    }

    commandOrder = 0;
    m_initialized = false;

    // Start with the first command
    const ELM327Command& firstCmd = initializeCommands[0];
    textTerminal->append("→ Starting ELM327 initialization...");
    textTerminal->append("→ Init: " + firstCmd.command);

    m_lastSentCommand = firstCmd.command;
    m_connectionManager->send(firstCmd.command);

    return true;
}

void MainWindow::processInitializationResponse(const QString& data)
{
    if (commandOrder >= initializeCommands.size()) {
        commandOrder = 0;
        m_initialized = true;
        textTerminal->append("✓ ELM327 initialization completed successfully!");

        if(m_searchPidsEnable) {
            getPids();
        }
        return;
    }

    // Get current command
    const ELM327Command& currentCmd = initializeCommands[commandOrder];

    // Clean and process response
    QString cleanResponse = removeEcho(data).trimmed().toUpper();
    bool responseOK = false;

    // Handle specific command responses
    if (commandOrder == 0 && currentCmd.command == "ATZ") {
        // ATZ returns version info
        if (cleanResponse.contains("ELM327") || cleanResponse.contains("V1.5")) {
            textTerminal->append("✓ ELM327 reset successful");
            responseOK = true;
        }
    }
    else if (currentCmd.command == "ATDP") {
        // Protocol description
        if (cleanResponse.contains("9141") || cleanResponse.contains("ISO")) {
            textTerminal->append("✓ ISO 9141-2 protocol confirmed");
            responseOK = true;
        } else if (!cleanResponse.isEmpty()) {
            textTerminal->append("⚠️ Protocol: " + cleanResponse);
            responseOK = true;
        }
    }
    else if (currentCmd.command == "ATWS") {
        // Warm start
        if (cleanResponse.contains("OK") || cleanResponse.contains("BUS INIT") ||
            cleanResponse.contains("SEARCHING") || cleanResponse.isEmpty()) {
            textTerminal->append("✓ Warm start completed");
            responseOK = true;
        }
    }
    else {
        // Standard commands expecting OK
        if (cleanResponse.contains("OK") || cleanResponse.isEmpty()) {
            textTerminal->append("✓ " + currentCmd.command + " - OK");
            responseOK = true;
        }
    }

    // Continue to next command
    commandOrder++;

    if (commandOrder < initializeCommands.size()) {
        const ELM327Command& nextCmd = initializeCommands[commandOrder];
        textTerminal->append("→ Init: " + nextCmd.command);

        // Calculate delay based on command
        int delay = 1000; // Default delay
        if (nextCmd.command == "ATZ") delay = 2000;
        else if (nextCmd.command == "ATSP3") delay = 1500;
        else if (nextCmd.command == "ATWS") delay = 5000;
        else if (nextCmd.command == "ATDP") delay = 1000;

        QTimer::singleShot(delay, [this, nextCmd]() {
            if (m_connectionManager && m_connected) {
                m_lastSentCommand = nextCmd.command;
                m_connectionManager->send(nextCmd.command);
            }
        });
    }
}

QString MainWindow::removeEcho(const QString& data)
{
    QString cleaned = data;

    // Remove the last sent command if it appears in the response
    if (!m_lastSentCommand.isEmpty()) {
        // Remove exact match at the beginning
        if (cleaned.startsWith(m_lastSentCommand, Qt::CaseInsensitive)) {
            cleaned = cleaned.mid(m_lastSentCommand.length()).trimmed();
        }

        // Remove command anywhere in the string (case insensitive)
        cleaned.remove(m_lastSentCommand, Qt::CaseInsensitive);
    }

    // Clean up common artifacts
    cleaned.remove(QRegularExpression("^\\s*"));  // Leading whitespace
    cleaned.remove(QRegularExpression("\\s*$"));  // Trailing whitespace

    return cleaned.trimmed();
}

void MainWindow::dataReceived(QString data)
{
    if(m_reading)
        return;

    // Clean the data
    data.remove("\r");
    data.remove(">");
    data.remove("?");
    data.remove(",");
    data.remove(QChar(0xFFFD)); // Remove � characters
    data.remove(QChar(0x00));   // Remove null characters

    // Check for errors first
    if(isError(data.toUpper().toStdString())) {
        textTerminal->append("❌ Error: " + data);
        return;
    }

    // Remove echo and display clean response
    if (!data.isEmpty()) {
        QString cleanedData = removeEcho(data);

        if (!cleanedData.isEmpty()) {
            textTerminal->append("← " + cleanedData);
        }
    }

    // Handle initialization sequence
    if(!m_initialized) {
        processInitializationResponse(data);
    }
    // Process the data if initialized
    else if(m_initialized && !data.isEmpty()) {
        try {
            QString cleanedData = cleanData(data);
            analysData(cleanedData);
        }
        catch (const std::exception& e) {
            qDebug() << "Exception in data analysis: " << e.what();
        }
        catch (...) {
            qDebug() << "Unknown exception in data analysis";
        }
    }
}

QString MainWindow::send(const QString &command)
{
    if(m_connectionManager && m_connected) {
        QString cleanCommand = cleanData(command);
        m_lastSentCommand = cleanCommand;

        if (command.startsWith("ATSH", Qt::CaseInsensitive)) {
            elm->setLastHeader(command);
        }

        textTerminal->append("→ " + cleanCommand);
        m_connectionManager->send(cleanCommand);
        QThread::msleep(5);
    }

    return QString();
}

void MainWindow::clearHeader()
{
    if(!m_connected)
        return;

    send("ATSH"); // Clear current header
    QThread::msleep(200);
}

void MainWindow::onReadFaultClicked()
{
    if(!m_connected || !m_initialized)
        return;

    textTerminal->append("→ Reading PCM trouble codes...");

    // Clear any existing header and set PCM header
    QTimer::singleShot(100, [this]() {
        clearHeader(); // Clear any existing header
    });

    QTimer::singleShot(400, [this]() {
        send(PCM_ECU_HEADER); // Set header for PCM - "ATSH8115F1"
    });

    QTimer::singleShot(800, [this]() {
        send("03"); // Standard Mode 03 - Request stored DTCs
    });
}

void MainWindow::onReadTransFaultClicked()
{
    if(!m_connected || !m_initialized)
        return;

    textTerminal->append("→ Reading transmission trouble codes...");

    // Clear any existing header and set transmission header
    QTimer::singleShot(100, [this]() {
        clearHeader(); // Clear any existing header
    });

    QTimer::singleShot(400, [this]() {
        send(TRANS_ECU_HEADER); // Set header for transmission ECU
    });

    QTimer::singleShot(800, [this]() {
        send("03"); // Request DTCs
    });
}

void MainWindow::onSendClicked()
{
    if(!m_connected || !m_initialized)
        return;

    QString command = sendEdit->text();

    // Clear header and set PCM header before sending command
    QTimer::singleShot(100, [this]() {
        clearHeader(); // Clear any existing header
    });

    QTimer::singleShot(400, [this, command]() {
        send(PCM_ECU_HEADER); // Set header for PCM - "ATSH8115F1"
    });

    QTimer::singleShot(800, [this, command]() {
        send(command);
    });
}

void MainWindow::onReadClicked()
{
    if(!m_connected || !m_initialized)
        return;

    m_reading = true;
    QString command = sendEdit->text();

    // Clear header and set PCM header before sending command
    clearHeader(); // Clear any existing header
    QThread::msleep(300);

    send(PCM_ECU_HEADER); // Set header for PCM - "ATSH8115F1"
    QThread::msleep(300);

    auto data = getData(command);

    if(isError(data.toUpper().toStdString())) {
        textTerminal->append("❌ Error: " + data);
    }
    else if (!data.isEmpty()) {
        QString cleanedData = removeEcho(data);
        if (!cleanedData.isEmpty()) {
            textTerminal->append("← " + cleanedData);
        }
        analysData(cleanData(data));
    }

    m_reading = false;
}

// Rest of the implementation remains the same as your original code...
// (setupUi, setupBluetoothUI, setupConnections, applyStyles, etc.)

void MainWindow::setupUi()
{
    // Create central widget and main layouts
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    gridLayout = new QGridLayout(centralWidget);
    gridLayout->setContentsMargins(6, 6, 6, 6);
    gridLayout->setSpacing(6);

    verticalLayout = new QVBoxLayout();
    verticalLayout->setSpacing(6);

    gridLayoutElm = new QGridLayout();
    gridLayoutElm->setSpacing(6);

    // Create and set up all UI elements

    // Bluetooth UI components
    connectionTypeLabel = new QLabel("Connection:", centralWidget);
    connectionTypeLabel->setMinimumSize(80, 48);

    m_connectionTypeCombo = new QComboBox(centralWidget);
    m_connectionTypeCombo->setMinimumHeight(44);

    btDeviceLabel = new QLabel("BT Device:", centralWidget);
    btDeviceLabel->setMinimumSize(80, 48);

    m_bluetoothDevicesCombo = new QComboBox(centralWidget);
    m_bluetoothDevicesCombo->setMinimumHeight(44);

    // Connection controls
    pushConnect = new QPushButton("Connect", centralWidget);
    pushConnect->setMinimumHeight(44);

    checkSearchPids = new QCheckBox("Get Pids", centralWidget);
    checkSearchPids->setMinimumHeight(44);
    checkSearchPids->setIconSize(QSize(32, 32));
    checkSearchPids->setChecked(false);

    // Spacer after connection controls
    verticalSpacer = new QSpacerItem(20, 5, QSizePolicy::Minimum, QSizePolicy::Fixed);

    // Fault reading buttons
    pushReadFault = new QPushButton("Read Fault", centralWidget);
    pushReadFault->setMinimumHeight(44);

    pushClearFault = new QPushButton("Clear Fault", centralWidget);
    pushClearFault->setMinimumHeight(44);

    btnReadTransFault = new QPushButton("Read GearBox", centralWidget);
    btnReadTransFault->setMinimumHeight(44);

    btnClearTransFault = new QPushButton("Clear GearBox", centralWidget);
    btnClearTransFault->setMinimumHeight(44);

    // Spacer after fault buttons
    verticalSpacer2 = new QSpacerItem(20, 5, QSizePolicy::Minimum, QSizePolicy::Fixed);

    // Scan and Read buttons
    pushScan = new QPushButton("Scan", centralWidget);
    pushScan->setMinimumHeight(44);

    pushRead = new QPushButton("Read", centralWidget);
    pushRead->setMinimumHeight(44);

    // Spacer after scan/read buttons
    verticalSpacer3 = new QSpacerItem(20, 5, QSizePolicy::Minimum, QSizePolicy::Fixed);

    // Command input and send
    sendEdit = new QLineEdit(centralWidget);
    sendEdit->setMinimumHeight(44);
    sendEdit->setText("0100"); // Default to basic OBD command

    pushSend = new QPushButton("    Send    ", centralWidget);
    pushSend->setMinimumHeight(44);

    // Spacer after command input
    verticalSpacer4 = new QSpacerItem(20, 5, QSizePolicy::Minimum, QSizePolicy::Fixed);

    // Protocol selection
    protocolCombo = new QComboBox(centralWidget);
    protocolCombo->setMinimumHeight(44);
    protocolCombo->addItem("Automatic protocol detection");
    protocolCombo->addItem("SAE J1850 PWM (41.6 kbaud)");
    protocolCombo->addItem("SAE J1850 VPW (10.4 kbaud)");
    protocolCombo->addItem("ISO 9141-2 (5 baud init, 10.4 kbaud)");
    protocolCombo->addItem("ISO 14230-4 KWP (5 baud init, 10.4 kbaud)");
    protocolCombo->addItem("ISO 14230-4 KWP (fast init, 10.4 kbaud)");
    protocolCombo->addItem("ISO 15765-4 CAN (11 bit ID, 500 kbaud)");
    protocolCombo->addItem("ISO 15765-4 CAN (29 bit ID, 500 kbaud)");
    protocolCombo->addItem("ISO 15765-4 CAN (11 bit ID, 250 kbaud)");
    protocolCombo->addItem("ISO 15765-4 CAN (29 bit ID, 250 kbaud)");
    protocolCombo->addItem("SAE J1939 CAN (29 bit ID, 250* kbaud)");
    protocolCombo->addItem("User1 CAN (11* bit ID, 125* kbaud)");
    protocolCombo->addItem("User2 CAN (11* bit ID, 50* kbaud)");

    pushSetProtocol = new QPushButton("Set", centralWidget);
    pushSetProtocol->setMinimumHeight(44);

    pushGetProtocol = new QPushButton("Get", centralWidget);
    pushGetProtocol->setMinimumHeight(44);

    // Interval controls
    intervalSlider = new QSlider(Qt::Horizontal, centralWidget);
    intervalSlider->setMinimumHeight(40);

    labelInterval = new QLabel("0 ms", centralWidget);
    labelInterval->setMinimumHeight(40);
    labelInterval->setAlignment(Qt::AlignCenter);

    // Terminal display
    textTerminal = new QTextBrowser(centralWidget);
    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(1);
    textTerminal->setSizePolicy(sizePolicy);

    // Bottom buttons
    pushClear = new QPushButton("Reset ", centralWidget);
    pushClear->setMinimumHeight(44);

    pushExit = new QPushButton("Exit", centralWidget);
    pushExit->setMinimumHeight(44);

    gridLayoutElm->addWidget(connectionTypeLabel, 0, 0);
    gridLayoutElm->addWidget(m_connectionTypeCombo, 0, 1, 1, 3);

    gridLayoutElm->addWidget(btDeviceLabel, 1, 0);
    gridLayoutElm->addWidget(m_bluetoothDevicesCombo, 1, 1, 1, 3);

    gridLayoutElm->addWidget(pushConnect, 2, 0, 1, 2);
    gridLayoutElm->addWidget(checkSearchPids, 2, 2, 1, 2);

    gridLayoutElm->addItem(verticalSpacer, 3, 0, 1, 4);

    gridLayoutElm->addWidget(pushReadFault, 4, 0, 1, 2);
    gridLayoutElm->addWidget(pushClearFault, 4, 2, 1, 2);

    gridLayoutElm->addWidget(btnReadTransFault, 5, 0, 1, 2);
    gridLayoutElm->addWidget(btnClearTransFault, 5, 2, 1, 2);

    gridLayoutElm->addItem(verticalSpacer2, 6, 0, 1, 4);

    gridLayoutElm->addWidget(sendEdit, 7, 0, 1, 2);
    gridLayoutElm->addWidget(pushRead, 7, 2, 1, 2);

    gridLayoutElm->addItem(verticalSpacer3, 8, 0, 1, 4);

    gridLayoutElm->addWidget(pushScan, 9, 0, 1, 2);
    gridLayoutElm->addWidget(pushSend, 9, 2, 1, 2);

    gridLayoutElm->addItem(verticalSpacer4, 10, 0, 1, 4);

    gridLayoutElm->addWidget(protocolCombo, 11, 0, 1, 2);
    gridLayoutElm->addWidget(pushSetProtocol, 11, 2);
    gridLayoutElm->addWidget(pushGetProtocol, 11, 3);

    gridLayoutElm->addWidget(intervalSlider, 12, 0, 1, 2);
    gridLayoutElm->addWidget(labelInterval, 12, 2, 1, 2);

    gridLayoutElm->addWidget(textTerminal, 13, 0, 1, 4);

    gridLayoutElm->addWidget(pushClear, 14, 0);
    gridLayoutElm->addWidget(pushExit, 14, 1, 1, 3);

    verticalLayout->addLayout(gridLayoutElm);

    gridLayout->addLayout(verticalLayout, 0, 0);

    setupBluetoothUI();
}

void MainWindow::setupBluetoothUI()
{
    // Initialize combo boxes
    m_connectionTypeCombo->addItem("WiFi");
    m_connectionTypeCombo->addItem("Bluetooth");
    m_connectionTypeCombo->setCurrentIndex(0); // Default to WiFi

    m_bluetoothDevicesCombo->clear();
    m_bluetoothDevicesCombo->addItem("Select device...");

    // Style the components
    const int FONT_SIZE = 20;
    const QString TEXT_COLOR = "#E6F3FF";
    const QString SECONDARY_COLOR = "#002D4D";
    const QString BORDER_COLOR = "#004C80";
    const QString ACCENT_COLOR = "#0073BF";

    const QString inputStyle = QString(
                                   "QWidget {"
                                   "    font-size: %5pt;"
                                   "    font-weight: bold;"
                                   "    color: %1;"
                                   "    background-color: %2;"
                                   "    border: 1px solid %3;"
                                   "    border-radius: 6px;"
                                   "    padding: 8px;"
                                   "}"
                                   "QWidget:focus {"
                                   "    border: 2px solid %4;"
                                   "}"
                                   ).arg(TEXT_COLOR, SECONDARY_COLOR, BORDER_COLOR, ACCENT_COLOR).arg(FONT_SIZE - 2);

    m_connectionTypeCombo->setStyleSheet(inputStyle + QString(
                                             "QComboBox::drop-down {"
                                             "    border: none;"
                                             "    width: 30px;"
                                             "}"
                                             "QComboBox::down-arrow {"
                                             "    width: 16px;"
                                             "    height: 16px;"
                                             "}"
                                             ));

    m_bluetoothDevicesCombo->setStyleSheet(inputStyle + QString(
                                               "QComboBox::drop-down {"
                                               "    border: none;"
                                               "    width: 30px;"
                                               "}"
                                               "QComboBox::down-arrow {"
                                               "    width: 16px;"
                                               "    height: 16px;"
                                               "}"
                                               ));

    connectionTypeLabel->setStyleSheet(QString(
                                           "QLabel {"
                                           "    font-size: %2pt;"
                                           "    font-weight: bold;"
                                           "    color: %1;"
                                           "}"
                                           ).arg(TEXT_COLOR).arg(FONT_SIZE));

    btDeviceLabel->setStyleSheet(QString(
                                     "QLabel {"
                                     "    font-size: %2pt;"
                                     "    font-weight: bold;"
                                     "    color: %1;"
                                     "}"
                                     ).arg(TEXT_COLOR).arg(FONT_SIZE));

    // Connect signals
    connect(m_connectionTypeCombo, &QComboBox::currentIndexChanged,
            [this](int index) {
                if (index == 0) {
                    if (m_connectionManager) {
                        m_connectionManager->setConnectionType(Wifi);
                        btDeviceLabel->setVisible(false);
                        m_bluetoothDevicesCombo->setVisible(false);
                    }
                }
                else if (index == 1) { // Bluetooth
                    if (m_connectionManager) {
                        m_connectionManager->setConnectionType(BlueTooth);
                        btDeviceLabel->setVisible(true);
                        m_bluetoothDevicesCombo->setVisible(true);

#ifdef Q_OS_ANDROID
                        requestBluetoothPermissions();
#else
                        scanBluetoothDevices();
#endif
                    }
                }
            });

    connect(m_bluetoothDevicesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onBluetoothDeviceSelected, Qt::DirectConnection);

    if (m_connectionManager) {
        disconnect(m_connectionManager, &ConnectionManager::bluetoothDeviceFound,
                   this, &MainWindow::onBluetoothDeviceFound);
        disconnect(m_connectionManager, &ConnectionManager::bluetoothDiscoveryCompleted,
                   this, &MainWindow::onBluetoothDiscoveryCompleted);

        connect(m_connectionManager, &ConnectionManager::bluetoothDeviceFound,
                this, &MainWindow::onBluetoothDeviceFound, Qt::DirectConnection);
        connect(m_connectionManager, &ConnectionManager::bluetoothDiscoveryCompleted,
                this, &MainWindow::onBluetoothDiscoveryCompleted, Qt::DirectConnection);
    }
}

void MainWindow::setupConnections()
{
    // Connect UI buttons
    connect(pushConnect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(pushSend, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(pushRead, &QPushButton::clicked, this, &MainWindow::onReadClicked);
    connect(pushSetProtocol, &QPushButton::clicked, this, &MainWindow::onSetProtocolClicked);
    connect(pushGetProtocol, &QPushButton::clicked, this, &MainWindow::onGetProtocolClicked);
    connect(pushClear, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    connect(pushReadFault, &QPushButton::clicked, this, &MainWindow::onReadFaultClicked);
    connect(pushClearFault, &QPushButton::clicked, this, &MainWindow::onClearFaultClicked);
    connect(pushScan, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    connect(pushExit, &QPushButton::clicked, this, &MainWindow::onExitClicked);
    connect(checkSearchPids, &QCheckBox::stateChanged, this, &MainWindow::onSearchPidsStateChanged);
    connect(btnReadTransFault, &QPushButton::clicked, this, &MainWindow::onReadTransFaultClicked);
    connect(btnClearTransFault, &QPushButton::clicked, this, &MainWindow::onClearTransFaultClicked);

    // Connect ConnectionManager signals
    if(m_connectionManager) {
        connect(m_connectionManager, &ConnectionManager::connected, this, &MainWindow::connected);
        connect(m_connectionManager, &ConnectionManager::disconnected, this, &MainWindow::disconnected);
        connect(m_connectionManager, &ConnectionManager::dataReceived, this, &MainWindow::dataReceived);
        connect(m_connectionManager, &ConnectionManager::stateChanged, this, &MainWindow::stateChanged);
    }
}

void MainWindow::applyStyles()
{
    // Font size constant - slightly increased for better readability on automotive displays
    const int FONT_SIZE = 20;

    // Enhanced marine theme color palette with better contrast for OBD scanner
    const QString PRIMARY_COLOR = "#0077CC";      // Brighter marine blue
    const QString SECONDARY_COLOR = "#003A66";    // Darker marine blue
    const QString ACCENT_COLOR = "#00AAFF";       // Vibrant highlight blue
    const QString TEXT_COLOR = "#F0F8FF";         // Crisp white-blue text
    const QString BORDER_COLOR = "#005799";       // Mid marine blue
    const QString BACKGROUND_COLOR = "#00274D";   // Deeper background

    // Main window background
    this->setStyleSheet(
        "QMainWindow {"
        "    background-color: " + BACKGROUND_COLOR + ";"
                             "}"
        );

    // Enhanced button style for OBD scanner controls
    const QString buttonBaseStyle = QString(
                                        "QPushButton {"
                                        "    font-size: %5pt;"
                                        "    font-weight: bold;"
                                        "    color: %1;"                          // TEXT_COLOR
                                        "    background-color: %2;"               // SECONDARY_COLOR
                                        "    border-radius: 8px;"                 // Increased radius
                                        "    padding: 6px 10px;"                  // More padding for larger touch targets
                                        "    margin: 3px;"
                                        "    min-height: 36px;"                   // Minimum height for consistent buttons
                                        "}"
                                        "QPushButton:hover {"
                                        "    background-color: %3;"               // PRIMARY_COLOR
                                        "    color: #FFFFFF;"
                                        "    border: 1px solid #80CFFF;"          // Subtle border on hover
                                        "}"
                                        "QPushButton:pressed {"
                                        "    background-color: %4;"               // Darker version
                                        "    padding: 7px 11px;"                  // Pressed effect
                                        "}"
                                        "QPushButton:disabled {"
                                        "    background-color: #334455;"          // Disabled state
                                        "    color: #8899AA;"
                                        "}"
                                        ).arg(TEXT_COLOR, SECONDARY_COLOR, PRIMARY_COLOR, "#002244").arg(FONT_SIZE);

    // Terminal style optimized for OBD data display
    textTerminal->setStyleSheet(QString(
                                    "QTextEdit {"
                                    "    font: %4pt 'Consolas';"
                                    "    color: %1;"                     // Light text
                                    "    background-color: %2;"          // Darker background
                                    "    border: 1px solid %3;"
                                    "    border-radius: 8px;"
                                    "    padding: 8px;"
                                    "    selection-background-color: #0077CC;" // Highlight color for selected text
                                    "    selection-color: #FFFFFF;"
                                    "}"
                                    ).arg(TEXT_COLOR, "#001A2E", BORDER_COLOR).arg(FONT_SIZE - 4)); // Slightly smaller for text

    // Apply styles to all standard buttons
    QList<QPushButton*> standardButtons = {
        pushConnect, pushSend, pushRead,
        pushSetProtocol, pushGetProtocol, pushClear,
        pushScan, pushReadFault, btnReadTransFault, btnClearTransFault,
        pushClearFault, pushExit
    };

    for (auto* button : standardButtons) {
        button->setStyleSheet(buttonBaseStyle);
    }

    // Special styling for critical buttons
    pushConnect->setStyleSheet(buttonBaseStyle + QString(
                                   "QPushButton {"
                                   "    background-color: #005599;"
                                   "    border: 2px solid #0077CC;"
                                   "}"
                                   "QPushButton:checked {"               // For toggle behavior if implemented
                                   "    background-color: #00AA66;"      // Green when connected
                                   "    border: 2px solid #00CC77;"
                                   "}"
                                   ));

    pushReadFault->setStyleSheet(buttonBaseStyle + QString(
                                     "QPushButton {"
                                     "    background-color: #884400;"      // Warning color for fault reading
                                     "}"
                                     ));

    pushClearFault->setStyleSheet(buttonBaseStyle + QString(
                                      "QPushButton {"
                                      "    background-color: #AA3300;"      // Caution color for fault clearing
                                      "}"
                                      ));

    btnReadTransFault->setStyleSheet(buttonBaseStyle + QString(
                                     "QPushButton {"
                                     "    background-color: #884400;"      // Warning color for fault reading
                                     "}"
                                     ));

    btnClearTransFault->setStyleSheet(buttonBaseStyle + QString(
                                      "QPushButton {"
                                      "    background-color: #AA3300;"      // Caution color for fault clearing
                                      "}"
                                      ));

    pushScan->setStyleSheet(buttonBaseStyle + QString(
                                         "QPushButton {"
                                         "    background-color: #884400;"
                                         "}"
                                         ));

    pushRead->setStyleSheet(buttonBaseStyle + QString(
                                          "QPushButton {"
                                          "    background-color: #AA3300;"
                                          "}"
                                          ));

    pushSend->setStyleSheet(buttonBaseStyle + QString(
                                "QPushButton {"
                                "    background-color: #AA3300;"
                                "}"
                                ));

    // Enhanced checkbox style for OBD scanner
    checkSearchPids->setStyleSheet(QString(
                                       "QCheckBox {"
                                       "    font-size: %5pt;"
                                       "    font-weight: bold;"
                                       "    color: %1;"
                                       "    padding: 4px;"
                                       "    spacing: 8px;"
                                       "}"
                                       "QCheckBox::indicator {"
                                       "    width: 30px;"                    // Larger indicator for touch screens
                                       "    height: 24px;"
                                       "    border: 2px solid %2;"
                                       "    border-radius: 6px;"
                                       "}"
                                       "QCheckBox::indicator:unchecked {"
                                       "    background-color: #001A2E;"
                                       "}"
                                       "QCheckBox::indicator:checked {"
                                       "    background-color: %3;"
                                       "    image: url(:/icons/check.png);"  // Optional: add a checkmark image
                                       "}"
                                       "QCheckBox::indicator:hover {"
                                       "    border-color: %4;"
                                       "}"
                                       ).arg(TEXT_COLOR, PRIMARY_COLOR, ACCENT_COLOR, PRIMARY_COLOR).arg(FONT_SIZE));

    // Input fields style optimized for OBD commands
    const QString inputStyle = QString(
                                   "QWidget {"
                                   "    font-size: %5pt;"
                                   "    font-weight: bold;"
                                   "    color: %1;"
                                   "    background-color: %2;"
                                   "    border: 1px solid %3;"
                                   "    border-radius: 8px;"
                                   "    padding: 10px;"
                                   "    selection-background-color: #0077CC;"
                                   "    selection-color: #FFFFFF;"
                                   "}"
                                   "QWidget:focus {"
                                   "    border: 2px solid %4;"
                                   "    background-color: #002D4D;"      // Slightly lighter when focused
                                   "}"
                                   ).arg(TEXT_COLOR, SECONDARY_COLOR, BORDER_COLOR, ACCENT_COLOR).arg(FONT_SIZE);

    sendEdit->setStyleSheet(inputStyle);

    // Protocol combo box style with dropdown arrow indicator
    protocolCombo->setStyleSheet(inputStyle + QString(
                                                  "QComboBox::drop-down {"
                                                  "    border: none;"
                                                  "    width: 36px;"                   // Wider clickable area
                                                  "}"
                                                  "QComboBox::down-arrow {"
                                                  "    width: 18px;"
                                                  "    height: 18px;"
                                                  "    image: url(:/icons/dropdown.png);" // Optional: add custom dropdown arrow
                                                  "}"
                                                  "QComboBox QAbstractItemView {"       // Dropdown menu styling
                                                  "    background-color: %1;"
                                                  "    border: 1px solid %2;"
                                                  "    border-radius: 6px;"
                                                  "    selection-background-color: %3;"
                                                  "    color: %4;"
                                                  "    padding: 4px;"
                                                  "}"
                                                  ).arg(SECONDARY_COLOR, BORDER_COLOR, PRIMARY_COLOR, TEXT_COLOR));

    // Interval slider label with value display
    labelInterval->setStyleSheet(QString(
                                     "QLabel {"
                                     "    font-size: %2px;"
                                     "    font-weight: bold;"
                                     "    color: %1;"
                                     "    padding: 5px 8px;"
                                     "    background-color: %3;"
                                     "    border-radius: 5px;"
                                     "    margin: 4px;"
                                     "}"
                                     ).arg(TEXT_COLOR, QString::number(FONT_SIZE), SECONDARY_COLOR));

    // Enhanced slider with better visual feedback for interval control
    intervalSlider->setStyleSheet(QString(
                                      "QSlider {"
                                      "    height: 36px;"                   // Taller for better touch control
                                      "}"
                                      "QSlider::groove:horizontal {"
                                      "    border: none;"
                                      "    height: 18px;"                   // Thicker groove
                                      "    background: %1;"
                                      "    border-radius: 9px;"
                                      "    margin: 0px;"
                                      "}"
                                      "QSlider::handle:horizontal {"
                                      "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                                      "        stop:0 %2, stop:1 %3);"
                                      "    border: 1px solid #80CFFF;"      // Subtle border
                                      "    width: 36px;"                    // Wider handle
                                      "    height: 26px;"                   // Taller handle
                                      "    margin: -5px 0;"
                                      "    border-radius: 12px;"
                                      "}"
                                      "QSlider::sub-page:horizontal {"      // Filled area
                                      "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
                                      "        stop:0 %4, stop:1 %2);"
                                      "    border-radius: 9px;"
                                      "}"
                                      "QSlider::add-page:horizontal {"      // Empty area
                                      "    background: %5;"
                                      "    border-radius: 9px;"
                                      "}"
                                      "QSlider::tick-mark {"
                                      "    background: %6;"
                                      "    width: 2px;"
                                      "    height: 8px;"                    // Taller tick marks
                                      "    margin-top: 5px;"
                                      "}"
                                      ).arg(
                                          "#001A2E",              // Groove background
                                          ACCENT_COLOR,           // Handle gradient start
                                          PRIMARY_COLOR,          // Handle gradient end
                                          "#0066AA",              // Sub-page gradient start (filled)
                                          "#001A2E",              // Add-page (empty)
                                          "#80CFFF"               // Tick marks - brighter for visibility
                                          ));
}

void MainWindow::setupIntervalSlider()
{
    // Configure slider properties
    intervalSlider->setMinimum(1);
    intervalSlider->setMaximum(100);
    intervalSlider->setSingleStep(5);
    intervalSlider->setTickInterval(5);
    intervalSlider->setTickPosition(QSlider::TicksBelow);
    intervalSlider->setMinimumHeight(50);  // Taller for touch

    // Set label alignment
    labelInterval->setAlignment(Qt::AlignCenter);

    // Set initial value
    interval = 250;
    intervalSlider->setValue(interval / 10);  // Convert to slider range
    labelInterval->setText(QString("%1 ms").arg(interval));

    // Connect signal
    connect(intervalSlider, &QSlider::valueChanged, this, &MainWindow::onIntervalSliderChanged);
}

void MainWindow::connected()
{
    pushConnect->setText(QString("Disconnect"));
    commandOrder = 0;
    m_initialized = false;
    m_connected = true;
    textTerminal->append("✓ ELM327 connected");

    // Use the new initialization method instead of direct RESET
    initializeELM327();
}

void MainWindow::disconnected()
{
    pushConnect->setText(QString("Connect"));
    commandOrder = 0;
    m_initialized = false;
    m_connected = false;
    textTerminal->append("✗ ELM327 disconnected");
}

QString MainWindow::cleanData(const QString& input)
{
    QString cleaned = input.trimmed().simplified();
    cleaned.remove(QRegularExpression(QString("[\\n\\t\\r]")));
    cleaned.remove(QRegularExpression(QString("[^a-zA-Z0-9]+")));
    return cleaned;
}

void MainWindow::stateChanged(QString state)
{
    textTerminal->append(state);
}

void MainWindow::getPids()
{
    if(!m_connected)
        return;

    runtimeCommands.clear();
    elm->resetPids();
    textTerminal->append("→ Searching available pids.");

    QString supportedPIDs = elm->get_available_pids();
    textTerminal->append("← Pids:  " + supportedPIDs);

    if(!supportedPIDs.isEmpty()) {
        if(supportedPIDs.contains(",")) {
            runtimeCommands = supportedPIDs.split(",");
        }
    }
}

bool MainWindow::isError(std::string msg) {
    std::vector<std::string> errors(ERROR, ERROR + 16);
    for(unsigned int i=0; i < errors.size(); i++) {
        if(msg.find(errors[i]) != std::string::npos)
            return true;
    }
    return false;
}

QString MainWindow::getData(const QString &command)
{
    auto dataReceived = ConnectionManager::getInstance()->readData(command);

    // Clean up the response
    dataReceived.remove("\r");
    dataReceived.remove(">");
    dataReceived.remove("?");
    dataReceived.remove(",");

    if(isError(dataReceived.toUpper().toStdString())) {
        QThread::msleep(500);
        return "error";
    }

    return dataReceived;
}

void MainWindow::saveSettings()
{
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

    if (m_settingsManager) {
        m_settingsManager->setWifiIp(ip);
        m_settingsManager->setWifiPort(wifiPort);
        m_settingsManager->setSerialPort("/dev/ttys001");
        m_settingsManager->setEngineDisplacement(2700);
        m_settingsManager->saveSettings();
    }
}

void MainWindow::analysData(const QString &dataReceived)
{
    // Use uint8_t for OBD byte values
    uint8_t A = 0;
    uint8_t B = 0;
    uint8_t PID = 0;

    const QString TRANS_MODULE_ID = "7E9";   // Transmission module CAN ID
    const QString AIRBAG_MODULE_ID = "7D3";  // Airbag module CAN ID
    const QString ABS_MODULE_ID = "7E8";     // ABS module CAN ID

    std::vector<QString> vec;
    auto resp = elm->prepareResponseToDecode(dataReceived);

    // Handle standard PID responses (41 XX ...)
    if(resp.size() > 2 && !resp[0].compare("41", Qt::CaseInsensitive)) {
        // Validate PID format
        QRegularExpression hexMatcher("^[0-9A-F]{2}$", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = hexMatcher.match(resp[1]);
        if (!match.hasMatch())
            return;

        try {
            // Safe conversion of PID with range check
            unsigned long pidTemp = std::stoul(resp[1].toStdString(), nullptr, 16);
            if (pidTemp > 0xFF) {
                return;  // Invalid PID value
            }
            PID = static_cast<uint8_t>(pidTemp);

            vec.clear();  // Clear vector before inserting new data
            vec.insert(vec.begin(), resp.begin() + 2, resp.end());

            if(vec.size() >= 2) {
                // Safe conversion of A and B with range checks
                unsigned long aTemp = std::stoul(vec[0].toStdString(), nullptr, 16);
                unsigned long bTemp = std::stoul(vec[1].toStdString(), nullptr, 16);

                if (aTemp <= 0xFF && bTemp <= 0xFF) {
                    A = static_cast<uint8_t>(aTemp);
                    B = static_cast<uint8_t>(bTemp);
                } else {
                    return;  // Invalid A or B values
                }
            }
            else if(vec.size() >= 1) {
                // Safe conversion of A with range check
                unsigned long aTemp = std::stoul(vec[0].toStdString(), nullptr, 16);
                if (aTemp <= 0xFF) {
                    A = static_cast<uint8_t>(aTemp);
                    B = 0;
                } else {
                    return;  // Invalid A value
                }
            }
        }
        catch (const std::exception& e) {
            textTerminal->append("Error parsing data: " + QString(e.what()));
            return;
        }
    }

    // Handle Mode 01 PID 01 (number of DTCs and MIL status)
    if(resp.size() > 2 && !resp[0].compare("41", Qt::CaseInsensitive) &&
        !resp[1].compare("01", Qt::CaseInsensitive)) {
        vec.clear();  // Clear vector before inserting new data
        vec.insert(vec.begin(), resp.begin() + 2, resp.end());
        std::pair<int, bool> dtcNumber = elm->decodeNumberOfDtc(vec);
        QString milText = dtcNumber.second ? "true" : "false";
        textTerminal->append("Number of DTCs: " + QString::number(dtcNumber.first) +
                             ", MIL on: " + milText);
    }

    // Handle Mode 03 (DTC codes) - Standard response format
    if(resp.size() > 1 && !resp[0].compare("43", Qt::CaseInsensitive)) {
        textTerminal->append("Standard DTC response detected");

        vec.clear();  // Clear vector before inserting new data
        vec.insert(vec.begin(), resp.begin() + 1, resp.end());

        std::vector<QString> dtcCodes = elm->decodeDTC(vec);
        if(!dtcCodes.empty()) {
            QString dtc_list = "DTCs: ";
            for(const auto &code : dtcCodes) {
                dtc_list.append(code + " ");
            }
            textTerminal->append(dtc_list);
        }
        else {
            textTerminal->append("Number of DTCs: 0");
        }
    }
}

void MainWindow::onConnectClicked()
{
    if(pushConnect->text() == "Connect") {
        textTerminal->clear();

        if(m_connectionManager) {
            // If Bluetooth is selected and a device is chosen, pass the address
            if (m_connectionTypeCombo->currentIndex() == 1) { // Bluetooth
                int deviceIndex = m_bluetoothDevicesCombo->currentIndex();
                if (deviceIndex > 0 && m_deviceAddressMap.contains(deviceIndex)) {
                    QString deviceAddress = m_deviceAddressMap[deviceIndex];
                    m_connectionManager->connectElm(deviceAddress);
                } else {
                    m_connectionManager->connectElm(); // Will start discovery
                }
            } else {
                m_connectionManager->connectElm(); // WiFi connection
            }
        }
    }
    else {
        if(m_connectionManager) {
            m_connectionManager->disConnectElm();
        }
    }
}

void MainWindow::onSetProtocolClicked()
{
    if(!m_connected)
        return;

    QString index = QString::number(protocolCombo->currentIndex());
    if(protocolCombo->currentIndex() == 10)
        index = "A";
    else if(protocolCombo->currentIndex() == 11)
        index = "B";
    else if(protocolCombo->currentIndex() == 12)
        index = "C";

    QString command = "ATSP" + index;
    send(command);
}

void MainWindow::onGetProtocolClicked()
{
    if(!m_connected)
        return;

    send(GET_PROTOCOL);
}

void MainWindow::onClearClicked()
{
    saveSettings();
    runtimeCommands.clear();

    textTerminal->clear();
    if(m_settingsManager) {
        textTerminal->append("Wifi Ip: " + m_settingsManager->getWifiIp() + " : " +
                             QString::number(m_settingsManager->getWifiPort()));
    }
    textTerminal->append("Resolution : " + QString::number(desktopRect.width()) +
                         "x" + QString::number(desktopRect.height()));

    send(RESET);
}

void MainWindow::onClearFaultClicked()
{
    if(!m_connected || !m_initialized)
        return;

    textTerminal->append("→ Clearing PCM trouble codes...");

    // Clear any existing header and set PCM header
    QTimer::singleShot(100, [this]() {
        clearHeader(); // Clear any existing header
    });

    QTimer::singleShot(400, [this]() {
        send(PCM_ECU_HEADER); // Set header for PCM
    });

    QTimer::singleShot(800, [this]() {
        send("04"); // Standard Mode 04 - Clear DTCs
    });

    QTimer::singleShot(1200, [this]() {
        textTerminal->append("PCM codes cleared. Please cycle ignition.");
    });
}

void MainWindow::onClearTransFaultClicked()
{
    if(!m_connected || !m_initialized)
        return;

    textTerminal->append("→ Clearing transmission trouble codes...");

    // Clear any existing header and set transmission header
    QTimer::singleShot(100, [this]() {
        clearHeader(); // Clear any existing header
    });

    QTimer::singleShot(400, [this]() {
        send(TRANS_ECU_HEADER); // Set transmission header
    });

    QTimer::singleShot(800, [this]() {
        send("04"); // Clear DTCs
    });

    QTimer::singleShot(1200, [this]() {
        textTerminal->append("Transmission codes cleared. Please cycle ignition.");
    });
}

void MainWindow::onScanClicked()
{
    if(!m_connected || !m_initialized)
        return;

    ObdScan *obdScan = new ObdScan(this, runtimeCommands, interval);
    obdScan->show();
}

void MainWindow::onIntervalSliderChanged(int value)
{
    interval = value * 10;  // Scale to 10-1000ms range
    labelInterval->setText(QString("%1 ms").arg(interval));
}

void MainWindow::onSearchPidsStateChanged(int state)
{
    if(!m_connected)
        return;

    if (state == Qt::Checked) {
        m_searchPidsEnable = true;
        getPids();
    } else {
        m_searchPidsEnable = false;
        runtimeCommands.clear();
    }
}

void MainWindow::onExitClicked()
{
    QApplication::quit();
}

void MainWindow::scanBluetoothDevices()
{
    // Clear previous devices
    m_bluetoothDevicesCombo->clear();
    m_bluetoothDevicesCombo->addItem("Select device...");
    m_deviceAddressMap.clear();

    // Process events to make sure UI updates before starting scan
    QCoreApplication::processEvents();

    // Start scanning
    if (m_connectionManager) {
        // Explicitly reconnect signals for this scan operation
        connect(m_connectionManager, &ConnectionManager::bluetoothDeviceFound,
                this, &MainWindow::onBluetoothDeviceFound, Qt::DirectConnection);
        connect(m_connectionManager, &ConnectionManager::bluetoothDiscoveryCompleted,
                this, &MainWindow::onBluetoothDiscoveryCompleted, Qt::DirectConnection);

        m_connectionManager->startBluetoothDiscovery();
    }
}

void MainWindow::onBluetoothDeviceFound(const QString &name, const QString &address)
{
    // Add device to combo box and map
    int index = m_bluetoothDevicesCombo->count();
    m_bluetoothDevicesCombo->addItem(name + " (" + address + ")");
    m_deviceAddressMap[index] = address;

    // Automatically select the first found device
    if (index == 1) { // First real device (index 0 is "Select device...")
        m_bluetoothDevicesCombo->setCurrentIndex(index);
        textTerminal->append("Auto-selected: " + name + " (" + address + ")");
    }

    // Display in terminal with QDebug for debugging
    textTerminal->append("Found device: " + name + " (" + address + ")");
    qDebug() << "Bluetooth device found:" << name << address << "at index" << index;

    // Process events to update UI immediately
    QCoreApplication::processEvents();
}

void MainWindow::onBluetoothDiscoveryCompleted()
{
    if (m_bluetoothDevicesCombo->count() <= 1) {
        textTerminal->append("No OBD Bluetooth devices found.");
    } else {
        textTerminal->append("Found " + QString::number(m_bluetoothDevicesCombo->count() - 1) + " Bluetooth devices.");
    }

    // Process events to update UI
    QCoreApplication::processEvents();
}

void MainWindow::onBluetoothDeviceSelected(int index)
{
    if (index <= 0) {
        return; // "Select device..." option
    }
    if (m_deviceAddressMap.contains(index)) {
        QString selectedAddress = m_deviceAddressMap[index];
        textTerminal->append("Selected device with address: " + selectedAddress);
    }
}

QString MainWindow::getSelectedBluetoothDeviceAddress() const
{
    int index = m_bluetoothDevicesCombo->currentIndex();
    if (index > 0 && m_deviceAddressMap.contains(index)) {
        return m_deviceAddressMap[index];
    }
    return QString();
}

#if defined(Q_OS_ANDROID)
void MainWindow::requestBluetoothPermissions()
{
    QBluetoothPermission bluetoothPermission;
    bluetoothPermission.setCommunicationModes(QBluetoothPermission::Access);

    switch (qApp->checkPermission(bluetoothPermission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(bluetoothPermission, this,
                                [this](const QPermission &permission) {
                                    if (qApp->checkPermission(permission) == Qt::PermissionStatus::Granted) {
                                        scanBluetoothDevices();
                                    } else {
                                        textTerminal->append("Bluetooth permission denied. Cannot proceed.");
                                    }
                                });
        break;
    case Qt::PermissionStatus::Granted:
        scanBluetoothDevices();
        break;
    case Qt::PermissionStatus::Denied:
        textTerminal->append("Bluetooth permission denied. Please enable in Settings.");
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
                                        textTerminal->append("Location permission denied. Bluetooth scanning may not work properly.");
                                    }
                                });
        break;
    case Qt::PermissionStatus::Granted:
        break;
    case Qt::PermissionStatus::Denied:
        textTerminal->append("Location permission denied. Bluetooth scanning may not work properly.");
        break;
    }
}
#endif
