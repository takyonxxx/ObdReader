#include "mainwindow.h"
#include "obdscan.h"
#include "global.h"
#include <QApplication>
#include <QCoreApplication>
#include <QScreen>
#include <QThread>

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
    textTerminal->append("Press Connect Button");
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
    m_connectionTypeCombo->setMinimumHeight(48);

    btDeviceLabel = new QLabel("BT Device:", centralWidget);
    btDeviceLabel->setMinimumSize(80, 48);

    m_bluetoothDevicesCombo = new QComboBox(centralWidget);
    m_bluetoothDevicesCombo->setMinimumHeight(48);

    // Connection controls
    pushConnect = new QPushButton("Connect", centralWidget);
    pushConnect->setMinimumHeight(48);

    checkSearchPids = new QCheckBox("Get Pids", centralWidget);
    checkSearchPids->setMinimumHeight(48);
    checkSearchPids->setIconSize(QSize(32, 32));
    checkSearchPids->setChecked(false);

    // Spacer after connection controls
    verticalSpacer = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

    // Fault reading buttons
    pushReadFault = new QPushButton("Read Fault", centralWidget);
    pushReadFault->setMinimumHeight(48);

    pushClearFault = new QPushButton("Clear Fault", centralWidget);
    pushClearFault->setMinimumHeight(48);

    btnReadTransFault = new QPushButton("Read Transmission", centralWidget);
    btnReadTransFault->setMinimumHeight(48);

    btnClearTransFault = new QPushButton("Clear Transmission", centralWidget);
    btnClearTransFault->setMinimumHeight(48);

    btnReadAirbagFault = new QPushButton("Read Airbag", centralWidget);
    btnReadAirbagFault->setMinimumHeight(48);

    btnClearAirbagFault = new QPushButton("Clear Airbag", centralWidget);
    btnClearAirbagFault->setMinimumHeight(48);

    // Spacer after fault buttons
    verticalSpacer2 = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

    // Scan and Read buttons
    pushScan = new QPushButton("Scan", centralWidget);
    pushScan->setMinimumHeight(48);

    pushRead = new QPushButton("Read", centralWidget);
    pushRead->setMinimumHeight(48);

    // Spacer after scan/read buttons
    verticalSpacer3 = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

    // Command input and send
    sendEdit = new QLineEdit(centralWidget);
    sendEdit->setMinimumHeight(48);
    sendEdit->setText("0101"); // Default command

    pushSend = new QPushButton("    Send    ", centralWidget);
    pushSend->setMinimumHeight(48);

    // Spacer after command input
    verticalSpacer4 = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

    // Protocol selection
    protocolCombo = new QComboBox(centralWidget);
    protocolCombo->setMinimumHeight(48);
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
    pushSetProtocol->setMinimumHeight(48);

    pushGetProtocol = new QPushButton("Get", centralWidget);
    pushGetProtocol->setMinimumHeight(48);

    // Interval controls
    intervalSlider = new QSlider(Qt::Horizontal, centralWidget);
    intervalSlider->setMinimumHeight(40);

    labelInterval = new QLabel("0 ms", centralWidget);
    labelInterval->setMinimumHeight(40);
    labelInterval->setAlignment(Qt::AlignCenter);

    // Terminal display
    textTerminal = new QTextBrowser(centralWidget);
    textTerminal->setMinimumHeight(150);
    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(1);
    textTerminal->setSizePolicy(sizePolicy);

    // Bottom buttons
    pushClear = new QPushButton("Reset ", centralWidget);
    pushClear->setMinimumHeight(48);

    pushExit = new QPushButton("Exit", centralWidget);
    pushExit->setMinimumHeight(48);

    // Add all widgets to the layout

    // Row 0: Connection type
    gridLayoutElm->addWidget(connectionTypeLabel, 0, 0);
    gridLayoutElm->addWidget(m_connectionTypeCombo, 0, 1, 1, 3);

    // Row 1: BT Device
    gridLayoutElm->addWidget(btDeviceLabel, 1, 0);
    gridLayoutElm->addWidget(m_bluetoothDevicesCombo, 1, 1, 1, 3);

    // Row 2: Connect button and Get Pids checkbox
    gridLayoutElm->addWidget(pushConnect, 2, 0, 1, 2);
    gridLayoutElm->addWidget(checkSearchPids, 2, 2, 1, 2);

    // Row 3: Spacer
    gridLayoutElm->addItem(verticalSpacer, 3, 0, 1, 4);

    // Row 10: Fault buttons
    gridLayoutElm->addWidget(pushReadFault, 10, 0, 1, 2);
    gridLayoutElm->addWidget(pushClearFault, 10, 2, 1, 2);

    // Row 11: Transmission buttons
    gridLayoutElm->addWidget(btnReadTransFault, 11, 0, 1, 2);
    gridLayoutElm->addWidget(btnClearTransFault, 11, 2, 1, 2);

    // Row 12: Airbag buttons
    gridLayoutElm->addWidget(btnReadAirbagFault, 12, 0, 1, 2);
    gridLayoutElm->addWidget(btnClearAirbagFault, 12, 2, 1, 2);

    // Row 13: Spacer
    gridLayoutElm->addItem(verticalSpacer2, 13, 0, 1, 4);

    // Row 14: Scan and Read buttons
    gridLayoutElm->addWidget(pushScan, 14, 0, 1, 2);
    gridLayoutElm->addWidget(pushRead, 14, 2, 1, 2);

    // Row 16: Spacer
    gridLayoutElm->addItem(verticalSpacer3, 16, 0, 1, 4);

    // Row 17: Command input and Send button
    gridLayoutElm->addWidget(sendEdit, 17, 0, 1, 2);
    gridLayoutElm->addWidget(pushSend, 17, 2, 1, 2);

    // Row 18: Spacer
    gridLayoutElm->addItem(verticalSpacer4, 18, 0, 1, 4);

    // Row 19: Protocol selection
    gridLayoutElm->addWidget(protocolCombo, 19, 0, 1, 2);
    gridLayoutElm->addWidget(pushSetProtocol, 19, 2);
    gridLayoutElm->addWidget(pushGetProtocol, 19, 3);

    // Row 20: Interval controls
    gridLayoutElm->addWidget(intervalSlider, 20, 0, 1, 2);
    gridLayoutElm->addWidget(labelInterval, 20, 2, 1, 2);

    // Row 24: Terminal
    gridLayoutElm->addWidget(textTerminal, 24, 0, 1, 4);

    // Row 25: Bottom buttons
    gridLayoutElm->addWidget(pushClear, 25, 0);
    gridLayoutElm->addWidget(pushExit, 25, 1, 1, 3);

    // Add gridLayoutElm to verticalLayout
    verticalLayout->addLayout(gridLayoutElm);

    // Add verticalLayout to the main gridLayout
    gridLayout->addLayout(verticalLayout, 0, 0);

    // Set up Bluetooth UI specific functionality
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

    // Connect signals with strong connection type
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
                        // Request permissions before scanning on Android
                        requestBluetoothPermissions();
#else
                        // Direct scanning on other platforms
                        scanBluetoothDevices();
#endif
                    }
                }
            });

    connect(m_bluetoothDevicesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onBluetoothDeviceSelected, Qt::DirectConnection);

    // Make sure the connection manager signals are connected properly
    if (m_connectionManager) {
        // Disconnect any existing connections to avoid duplicates
        disconnect(m_connectionManager, &ConnectionManager::bluetoothDeviceFound,
                   this, &MainWindow::onBluetoothDeviceFound);
        disconnect(m_connectionManager, &ConnectionManager::bluetoothDiscoveryCompleted,
                   this, &MainWindow::onBluetoothDiscoveryCompleted);

        // Reconnect with direct connection
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
    connect(btnReadAirbagFault, &QPushButton::clicked, this, &MainWindow::onReadAirbagFaultClicked);
    connect(btnClearAirbagFault, &QPushButton::clicked, this, &MainWindow::onClearAirbagFaultClicked);

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
    // Font size constant
    const int FONT_SIZE = 20;

    // Marine theme color palette
    const QString PRIMARY_COLOR = "#005999";      // Lighter marine blue
    const QString SECONDARY_COLOR = "#002D4D";    // Darker marine blue
    const QString ACCENT_COLOR = "#0073BF";       // Bright marine blue
    const QString TEXT_COLOR = "#E6F3FF";         // Light blue-white
    const QString BORDER_COLOR = "#004C80";       // Mid marine blue
    const QString BACKGROUND_COLOR = "#003366";   // Main background

    // Main window background
    this->setStyleSheet(
        "QMainWindow {"
        "    background-color: " + BACKGROUND_COLOR + ";"
        "}"
    );

    // Base button style
    const QString buttonBaseStyle = QString(
        "QPushButton {"
        "    font-size: %5pt;"
        "    font-weight: bold;"
        "    color: %1;"                          // TEXT_COLOR
        "    background-color: %2;"               // SECONDARY_COLOR
        "    border-radius: 6px;"
        "    padding: 4px 6px;"
        "    margin: 2px;"
        "}"
        "QPushButton:hover {"
        "    background-color: %3;"               // PRIMARY_COLOR
        "    color: #E6F3FF;"
        "}"
        "QPushButton:pressed {"
        "    background-color: %4;"               // Darker version
        "    padding: 4px 6px;"
        "}"
    ).arg(TEXT_COLOR, SECONDARY_COLOR, PRIMARY_COLOR, "#001F33").arg(FONT_SIZE);

    // Terminal style with marine theme
    textTerminal->setStyleSheet(QString(
        "QTextEdit {"
        "    font: %4pt 'Consolas';"
        "    color: %1;"                     // Light text
        "    background-color: %2;"          // Darker background
        "    border: 1px solid %3;"
        "    border-radius: 6px;"
        "    padding: 5px;"
        "}"
    ).arg(TEXT_COLOR, "#001F33", BORDER_COLOR).arg(FONT_SIZE - 4)); // Slightly smaller for text

    // Apply styles to all standard buttons
    QList<QPushButton*> standardButtons = {
        pushConnect, pushSend, pushRead,
        pushSetProtocol, pushGetProtocol, pushClear,
        pushScan, pushReadFault, btnReadTransFault, btnClearTransFault,
        pushClearFault, btnReadAirbagFault, btnClearAirbagFault, pushExit
    };

    for (auto* button : standardButtons) {
        button->setStyleSheet(buttonBaseStyle);
    }

    // Checkbox style with marine theme
    checkSearchPids->setStyleSheet(QString(
        "QCheckBox {"
        "    font-size: %5pt;"
        "    font-weight: bold;"
        "    color: %1;"
        "    padding: 3px;"
        "    spacing: 6px;"
        "}"
        "QCheckBox::indicator {"
        "    width: 28px;"
        "    height: 22px;"
        "    border: 2px solid %2;"
        "    border-radius: 6px;"
        "}"
        "QCheckBox::indicator:unchecked {"
        "    background-color: transparent;"
        "}"
        "QCheckBox::indicator:checked {"
        "    background-color: %3;"
        "}"
        "QCheckBox::indicator:hover {"
        "    border-color: %4;"
        "}"
    ).arg(TEXT_COLOR, PRIMARY_COLOR, ACCENT_COLOR, PRIMARY_COLOR).arg(FONT_SIZE - 2));

    // Input fields style
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

    sendEdit->setStyleSheet(inputStyle);
    protocolCombo->setStyleSheet(inputStyle + QString(
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 30px;"
        "}"
        "QComboBox::down-arrow {"
        "    width: 16px;"
        "    height: 16px;"
        "}"
    ));

    // Interval slider style
    labelInterval->setStyleSheet(QString(
        "QLabel {"
        "    font-size: %2px;"
        "    font-weight: bold;"
        "    color: %1;"
        "    padding: 3px;"
        "    background-color: %3;"
        "    border-radius: 3px;"
        "    margin: 3px;"
        "}"
    ).arg(TEXT_COLOR, QString::number(FONT_SIZE), SECONDARY_COLOR));

    intervalSlider->setStyleSheet(QString(
        "QSlider::groove:horizontal {"
        "    border: none;"
        "    height: 16px;"
        "    background: %1;"
        "    border-radius: 5px;"
        "    margin: 0px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "        stop:0 %2, stop:1 %3);"
        "    border: none;"
        "    width: 30px;"
        "    margin: -5px 0;"
        "    border-radius: 10px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "    background: %4;"
        "    border-radius: 5px;"
        "}"
        "QSlider::add-page:horizontal {"
        "    background: %5;"
        "    border-radius: 5px;"
        "}"
        "QSlider::tick-mark {"
        "    background: %6;"
        "    width: 2px;"
        "    height: 5px;"
        "    margin-top: 5px;"
        "}"
    ).arg(
        "#001F33",              // Groove background
        ACCENT_COLOR,           // Handle gradient start
        PRIMARY_COLOR,          // Handle gradient end
        PRIMARY_COLOR,          // Sub-page (filled)
        "#001F33",              // Add-page (empty)
        BORDER_COLOR            // Tick marks
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
    textTerminal->append("Elm 327 connected");
    send(RESET);
    QThread::msleep(250);
}

void MainWindow::disconnected()
{
    pushConnect->setText(QString("Connect"));
    commandOrder = 0;
    m_initialized = false;
    m_connected = false;
    textTerminal->append("Elm DisConnected");
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

    // Check for errors
    if(isError(data.toUpper().toStdString())) {
        textTerminal->append("Error : " + data);
    }
    else if (!data.isEmpty()) {
        textTerminal->append("<- " + data);
    }

    // Handle initialization sequence
    if(!m_initialized) {
        if(commandOrder >= initializeCommands.size()) {
            commandOrder = 0;
            m_initialized = true;

            if(m_searchPidsEnable) {
                getPids();
            }
        }
        else {
            // Send next initialization command
            QString nextCommand = initializeCommands[commandOrder];
            textTerminal->append("-> Init: " + nextCommand);
            commandOrder++;

            // Delay to ensure the command is sent properly
            QThread::msleep(100);

            // Send the command
            m_connectionManager->send(nextCommand);
        }
    }
    // Process the data if initialized
    else if(m_initialized && !data.isEmpty()) {
        try {
            data = cleanData(data);
            analysData(data);
        }
        catch (const std::exception& e) {
            qDebug() << "Exception in data analysis: " << e.what();
        }
        catch (...) {
            qDebug() << "Unknown exception in data analysis";
        }
    }
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

QString MainWindow::send(const QString &command)
{
    if(m_connectionManager && m_connected) {
        auto cmd = cleanData(command);
        textTerminal->append("-> " + cmd);

        m_connectionManager->send(cmd);
        QThread::msleep(5);  // Small delay for processing
    }

    return QString();
}

void MainWindow::getPids()
{
    if(!m_connected)
        return;

    runtimeCommands.clear();
    elm->resetPids();
    textTerminal->append("-> Searching available pids.");

    QString supportedPIDs = elm->get_available_pids();
    textTerminal->append("<- Pids:  " + supportedPIDs);

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
    QString ip = "192.168.0.10";
#else
    // elm -n 35000 -s car
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
            // textTerminal->append("Pid: " + QString::number(PID) +
            //                          "  A: " + QString::number(A) +
            //                          "  B: " + QString::number(B));
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

    // Handle Mode 03 (DTC codes) - CAN format with ECU ID (e.g. 7E8 02 43 ...)
    if(resp.size() > 3 &&
        resp[0].length() == 3 && resp[0].startsWith("7", Qt::CaseInsensitive) &&
        resp[2].compare("43", Qt::CaseInsensitive) == 0) {

        // This is a CAN format response with ECU ID
        textTerminal->append("CAN format DTC response detected from ECU: " + resp[0]);

        vec.clear();
        // Skip ECU ID (7Ex), byte count, and response code (43)
        vec.insert(vec.begin(), resp.begin() + 3, resp.end());

        // Debug output
        QString debugStr = "Parsing bytes: ";
        for(const auto& val : vec) {
            debugStr += val + " ";
        }
        textTerminal->append(debugStr);

        // Process DTCs
        std::vector<QString> dtcCodes = elm->decodeDTC(vec);
        if(!dtcCodes.empty()) {
            QString dtc_list = "DTCs from ECU " + resp[0] + ": ";
            for(const auto &code : dtcCodes) {
                dtc_list.append(code + " ");
            }
            textTerminal->append(dtc_list);
        }
        else {
            textTerminal->append("No DTCs reported from ECU " + resp[0]);
        }
    }
    // Handle standard Mode 03 (DTC codes) response without CAN headers
    else if(resp.size() > 1 && !resp[0].compare("43", Qt::CaseInsensitive)) {
        vec.clear();  // Clear vector before inserting new data
        vec.insert(vec.begin(), resp.begin() + 1, resp.end());

        // Debug output
        QString debugStr = "Parsing bytes: ";
        for(const auto& val : vec) {
            debugStr += val + " ";
        }
        textTerminal->append(debugStr);

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

void MainWindow::onSendClicked()
{
    if(!m_connected)
        return;

    QString command = sendEdit->text();
    send(command);
}

void MainWindow::onReadClicked()
{
    if(!m_connected)
        return;

    m_reading = true;
    QString command = sendEdit->text();
    auto data = getData(command);

    if(isError(data.toUpper().toStdString())) {
        textTerminal->append("Error : " + data);
    }
    else if (!data.isEmpty()) {
        textTerminal->append("<- " + data);
    }

    analysData(data);
    m_reading = false;
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

void MainWindow::onReadFaultClicked()
{
    if(!m_connected)
        return;
    textTerminal->append("-> Reading PCM trouble codes...");

    // Request PCM DTCs (using standard Mode 03 request)
    send(READ_TROUBLE);
    QThread::msleep(250);  // Longer delay for complete response
}

void MainWindow::onClearFaultClicked()
{
    if(!m_connected)
        return;
    textTerminal->append("-> Clearing PCM trouble codes...");

    // Clear DTCs (using standard Mode 04 request)
    send(CLEAR_TROUBLE);
    QThread::msleep(250);

    textTerminal->append("PCM codes cleared. Please cycle ignition.");
}

void MainWindow::onReadTransFaultClicked()
{
    if(!m_connected)
        return;

    textTerminal->append("-> Reading transmission trouble codes...");

    // First try to select the transmission control module
    send(TRANS_ECU_HEADER);  // Set header for transmission ECU
    QThread::msleep(100);

    // Read DTCs from transmission
    send(READ_TROUBLE);
    QThread::msleep(250);  // Longer delay for complete response
}

void MainWindow::onClearTransFaultClicked()
{
    if(!m_connected)
        return;

    textTerminal->append("-> Clearing transmission trouble codes...");

    // First try to select the transmission control module
    send(TRANS_ECU_HEADER);  // Set header for transmission ECU
    QThread::msleep(100);

    // Clear DTCs
    send(CLEAR_TROUBLE);
    QThread::msleep(250);  // Wait for ECU to process

    textTerminal->append("Transmission codes cleared. Please cycle ignition.");
}

void MainWindow::onReadAirbagFaultClicked()
{
    if(!m_connected)
        return;
    textTerminal->append("-> Reading Airbag/SRS trouble codes...");

    // First select the Airbag control module
    send(AIRBAG_ECU_HEADER);  // Set header for Airbag ECU (try 7D0 if this doesn't work)
    QThread::msleep(100);

    // Request Airbag DTCs (using standard Mode 03 request)
    send(READ_TROUBLE);
    QThread::msleep(250);  // Longer delay for complete response
}

void MainWindow::onClearAirbagFaultClicked()
{
    if(!m_connected)
        return;
    textTerminal->append("-> Clearing Airbag/SRS trouble codes...");

    // First select the Airbag control module
    send(AIRBAG_ECU_HEADER);  // Set header for Airbag ECU
    QThread::msleep(100);

    // Clear DTCs (using standard Mode 04 request)
    send(CLEAR_TROUBLE);
    QThread::msleep(250);

    textTerminal->append("Airbag codes cleared. Please cycle ignition.");
}

void MainWindow::onScanClicked()
{
    if(!m_connected)
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

    // Display scanning message
    textTerminal->append("Scanning for Bluetooth devices...");

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

    // Display in terminal with QDebug for debugging
    textTerminal->append("Found device: " + name + " (" + address + ")");
    qDebug() << "Bluetooth device found:" << name << address << "at index" << index;

    // Process events to update UI immediately
    QCoreApplication::processEvents();
}

void MainWindow::onBluetoothDiscoveryCompleted()
{
    if (m_bluetoothDevicesCombo->count() <= 1) {
        textTerminal->append("No Bluetooth devices found.");
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
                                        textTerminal->append("Bluetooth permission granted. Starting scan...");
                                        scanBluetoothDevices();
                                    } else {
                                        textTerminal->append("Bluetooth permission denied. Cannot proceed.");
                                    }
                                });
        break;
    case Qt::PermissionStatus::Granted:
        textTerminal->append("Bluetooth permission already granted. Starting scan...");
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
                                    if (qApp->checkPermission(permission) == Qt::PermissionStatus::Granted) {
                                        textTerminal->append("Location permission granted.");
                                        // Permission granted - but we already started scan in Bluetooth permission handler
                                    } else {
                                        textTerminal->append("Location permission denied. Bluetooth scanning may not work properly.");
                                    }
                                });
        break;
    case Qt::PermissionStatus::Granted:
        textTerminal->append("Location permission already granted.");
        break;
    case Qt::PermissionStatus::Denied:
        textTerminal->append("Location permission denied. Bluetooth scanning may not work properly.");
        break;
    }
}
#endif
