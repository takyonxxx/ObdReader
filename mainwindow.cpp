#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "obdscan.h"
#include "global.h"

QStringList runtimeCommands;
int interval = 10;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Initialize screen and window properties
    setWindowTitle("OBD-II Diagnostic Interface");
    QScreen *screen = window()->screen();
    desktopRect = screen->availableGeometry();

    // Apply styling
    applyStyles();

#ifdef Q_OS_ANDROID
    requestAndroidPermissions();
#endif

    // Set protocol default
    ui->protocolCombo->setCurrentIndex(3);

    // Initialize OBD components
    elm = ELM::getInstance();
    elm->resetPids();

    m_settingsManager = SettingsManager::getInstance();
    if(m_settingsManager) {
        saveSettings();
        ui->textTerminal->append("Wifi Ip: " + m_settingsManager->getWifiIp() + " : " +
                                 QString::number(m_settingsManager->getWifiPort()));
    }

    m_connectionManager = ConnectionManager::getInstance();

    // Initialize UI components
    setupUi();
    setupConnections();
    setupIntervalSlider();

    // Setup initialization commands
    initializeCommands = {"ATZ", "ATSP0", "ATE0", "ATL0", "ATAT1", "ATH1"};

    // Display initial status
    ui->textTerminal->append("Resolution : " + QString::number(desktopRect.width()) +
                             "x" + QString::number(desktopRect.height()));
    ui->textTerminal->append("Press Connect Button");
    ui->pushConnect->setFocus();

    // Configure refresh timer
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
}

#ifdef Q_OS_ANDROID
void MainWindow::requestAndroidPermissions()
{
    // Permission that requires runtime request (location)
    const QStringList permissions {
        "android.permission.INTERNET",
        "android.permission.ACCESS_NETWORK_STATE",
        "android.permission.ACCESS_WIFI_STATE",
        "android.permission.CHANGE_WIFI_STATE",
        "android.permission.ACCESS_FINE_LOCATION",
        "android.permission.ACCESS_COARSE_LOCATION",
        "android.permission.WAKE_LOCK"
    };

    bool allPermissionsGranted = true;

    // Check each permission
    for (const QString &permission : permissions) {
        QJniObject jPermission = QJniObject::fromString(permission);
        QJniObject activity = QJniObject::callStaticObjectMethod(
            "org/qtproject/qt/android/QtNative",
            "activity",
            "()Landroid/app/Activity;");

        jint result = activity.callMethod<jint>(
            "checkSelfPermission",
            "(Ljava/lang/String;)I",
            jPermission.object<jstring>());

        if (result != 0) { // PERMISSION_GRANTED is 0
            allPermissionsGranted = false;

            // Request permission using JNI
            jobjectArray permissionArray;
            QJniEnvironment env;
            jclass stringClass = env->FindClass("java/lang/String");
            permissionArray = env->NewObjectArray(1, stringClass, nullptr);
            env->SetObjectArrayElement(permissionArray, 0, jPermission.object<jstring>());

            activity.callMethod<void>(
                "requestPermissions",
                "([Ljava/lang/String;I)V",
                permissionArray,
                0);
        }
    }

    if (!allPermissionsGranted) {
        QMessageBox::information(this, "Permissions Required",
                                 "Location permissions are required for WiFi scanning.\n"
                                 "Please grant the requested permissions.");
    }
}
#endif

MainWindow::~MainWindow()
{
    // Cleanup
    if(m_refreshTimer.isActive()) {
        m_refreshTimer.stop();
    }

    if(m_connectionManager) {
        m_connectionManager->disConnectElm();
    }

    if(m_settingsManager) {
        m_settingsManager->saveSettings();
    }

    delete ui;
}

void MainWindow::setupUi()
{
    // Set default command
    ui->sendEdit->setText("0101");
}

void MainWindow::setupConnections()
{
    // Connect UI buttons
    connect(ui->pushConnect, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->pushSend, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(ui->pushRead, &QPushButton::clicked, this, &MainWindow::onReadClicked);
    connect(ui->pushSetProtocol, &QPushButton::clicked, this, &MainWindow::onSetProtocolClicked);
    connect(ui->pushGetProtocol, &QPushButton::clicked, this, &MainWindow::onGetProtocolClicked);
    connect(ui->pushClear, &QPushButton::clicked, this, &MainWindow::onClearClicked);
    connect(ui->pushReadFault, &QPushButton::clicked, this, &MainWindow::onReadFaultClicked);
    connect(ui->pushClearFault, &QPushButton::clicked, this, &MainWindow::onClearFaultClicked);
    connect(ui->pushScan, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    connect(ui->pushExit, &QPushButton::clicked, this, &MainWindow::onExitClicked);
    connect(ui->checkSearchPids, &QCheckBox::stateChanged, this, &MainWindow::onSearchPidsStateChanged);
    connect(ui->btnReadTransFault, &QPushButton::clicked, this, &MainWindow::onReadTransFaultClicked);
    connect(ui->btnClearTransFault, &QPushButton::clicked, this, &MainWindow::onClearTransFaultClicked);

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
                                        "    font-size: 22pt;"
                                        "    font-weight: bold;"
                                        "    color: %1;"                          // TEXT_COLOR
                                        "    background-color: %2;"               // SECONDARY_COLOR
                                        "    border-radius: 8px;"
                                        "    padding: 6px 10px;"
                                        "    margin: 4px;"
                                        "}"
                                        "QPushButton:hover {"
                                        "    background-color: %3;"               // PRIMARY_COLOR
                                        "    color: #E6F3FF;"
                                        "}"
                                        "QPushButton:pressed {"
                                        "    background-color: %4;"               // Darker version
                                        "    padding: 14px 18px;"
                                        "}"
                                        ).arg(TEXT_COLOR, SECONDARY_COLOR, PRIMARY_COLOR, "#001F33");

    // Terminal style with marine theme
    ui->textTerminal->setStyleSheet(QString(
                                        "QTextEdit {"
                                        "    font: 20pt 'Consolas';"
                                        "    color: %1;"                     // Light text
                                        "    background-color: %2;"          // Darker background
                                        "    border: 1px solid %3;"
                                        "    border-radius: 8px;"
                                        "    padding: 10px;"
                                        "}"
                                        ).arg(TEXT_COLOR, "#001F33", BORDER_COLOR));

    // Apply styles to all standard buttons
    QList<QPushButton*> standardButtons = {
        ui->pushConnect, ui->pushSend, ui->pushRead,
        ui->pushSetProtocol, ui->pushGetProtocol, ui->pushClear,
        ui->pushScan, ui->pushReadFault, ui->btnReadTransFault, ui->btnClearTransFault,
        ui->pushClearFault, ui->pushExit
    };

    for (auto* button : standardButtons) {
        button->setStyleSheet(buttonBaseStyle);
    }

    // Checkbox style with marine theme
    ui->checkSearchPids->setStyleSheet(QString(
                                           "QCheckBox {"
                                           "    font-size: 22pt;"
                                           "    font-weight: bold;"
                                           "    color: %1;"
                                           "    padding: 3px;"
                                           "    spacing: 6px;"
                                           "}"
                                           "QCheckBox::indicator {"
                                           "    width: 28px;"
                                           "    height: 28px;"
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
                                           ).arg(TEXT_COLOR, PRIMARY_COLOR, ACCENT_COLOR, PRIMARY_COLOR));

    // Input fields style
    const QString inputStyle = QString(
                                   "QWidget {"
                                   "    font-size: 20pt;"
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
                                   ).arg(TEXT_COLOR, SECONDARY_COLOR, BORDER_COLOR, ACCENT_COLOR);

    ui->sendEdit->setStyleSheet(inputStyle);
    ui->protocolCombo->setStyleSheet(inputStyle + QString(
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
    ui->labelInterval->setStyleSheet(QString(
                                         "QLabel {"
                                         "    font-size: 28px;"
                                         "    font-weight: bold;"
                                         "    color: %1;"
                                         "    padding: 3px;"
                                         "    background-color: %2;"
                                         "    border-radius: 3px;"
                                         "    margin: 3px;"
                                         "}"
                                         ).arg(TEXT_COLOR, SECONDARY_COLOR));

    ui->intervalSlider->setStyleSheet(QString(
                                          "QSlider::groove:horizontal {"
                                          "    border: none;"
                                          "    height: 20px;"
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
    ui->intervalSlider->setMinimum(1);
    ui->intervalSlider->setMaximum(100);
    ui->intervalSlider->setSingleStep(5);
    ui->intervalSlider->setTickInterval(5);
    ui->intervalSlider->setTickPosition(QSlider::TicksBelow);
    ui->intervalSlider->setMinimumHeight(50);  // Taller for touch

    // Set label alignment
    ui->labelInterval->setAlignment(Qt::AlignCenter);

    // Set initial value
    interval = 100;  // Default 100ms
    ui->intervalSlider->setValue(interval / 10);  // Convert to slider range
    ui->labelInterval->setText(QString("%1 ms").arg(interval));

    // Connect signal
    connect(ui->intervalSlider, &QSlider::valueChanged, this, &MainWindow::onIntervalSliderChanged);
}

void MainWindow::connected()
{
    ui->pushConnect->setText(QString("Disconnect"));
    commandOrder = 0;
    m_initialized = false;
    m_connected = true;
    ui->textTerminal->append("Elm 327 connected");
    send(RESET);
}

void MainWindow::disconnected()
{
    // Stop auto-refresh if active
    if (m_refreshTimer.isActive()) {
        m_refreshTimer.stop();
    }

    ui->pushConnect->setText(QString("Connect"));
    commandOrder = 0;
    m_initialized = false;
    m_connected = false;
    ui->textTerminal->append("Elm DisConnected");
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
        ui->textTerminal->append("Error : " + data);
    }
    else if (!data.isEmpty()) {
        ui->textTerminal->append("<- " + data);
    }

    // Handle initialization sequence
    if(!m_initialized && initializeCommands.size() == commandOrder) {
        commandOrder = 0;
        m_initialized = true;

        if(m_searchPidsEnable) {
            getPids();
        }
    }
    else if(!m_initialized && commandOrder < initializeCommands.size()) {
        send(initializeCommands[commandOrder]);
        commandOrder++;
    }

    // Process the data if initialized
    if(m_initialized && !data.isEmpty()) {
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
    ui->textTerminal->append(state);
}

QString MainWindow::send(const QString &command)
{
    if(m_connectionManager && m_connected) {
        auto cmd = cleanData(command);
        ui->textTerminal->append("-> " + cmd);

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
    ui->textTerminal->append("-> Searching available pids.");

    QString supportedPIDs = elm->get_available_pids();
    ui->textTerminal->append("<- Pids:  " + supportedPIDs);

    if(!supportedPIDs.isEmpty()) {
        if(supportedPIDs.contains(",")) {
            runtimeCommands = supportedPIDs.split(",");
        }
    }
}

bool MainWindow::isError(std::string msg) {
    static const std::vector<std::string> errors(ERROR, ERROR + 18);
    for(const auto& error : errors) {
        if(msg.find(error) != std::string::npos)
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
    QString ip = "192.168.1.27";  // Default IP
    quint16 wifiPort = 35000;     // Default port

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
            ui->textTerminal->append("Pid: " + QString::number(PID) +
                                     "  A: " + QString::number(A) +
                                     "  B: " + QString::number(B));
        }
        catch (const std::exception& e) {
            ui->textTerminal->append("Error parsing data: " + QString(e.what()));
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
        ui->textTerminal->append("Number of DTCs: " + QString::number(dtcNumber.first) +
                                 ", MIL on: " + milText);
    }

    // Handle Mode 03 (DTC codes) - CAN format with ECU ID (e.g. 7E8 02 43 ...)
    if(resp.size() > 3 &&
        resp[0].length() == 3 && resp[0].startsWith("7", Qt::CaseInsensitive) &&
        resp[2].compare("43", Qt::CaseInsensitive) == 0) {

        // This is a CAN format response with ECU ID
        ui->textTerminal->append("CAN format DTC response detected from ECU: " + resp[0]);

        vec.clear();
        // Skip ECU ID (7Ex), byte count, and response code (43)
        vec.insert(vec.begin(), resp.begin() + 3, resp.end());

        // Debug output
        QString debugStr = "Parsing bytes: ";
        for(const auto& val : vec) {
            debugStr += val + " ";
        }
        ui->textTerminal->append(debugStr);

        // Process DTCs
        std::vector<QString> dtcCodes = elm->decodeDTC(vec);
        if(!dtcCodes.empty()) {
            QString dtc_list = "DTCs from ECU " + resp[0] + ": ";
            for(const auto &code : dtcCodes) {
                dtc_list.append(code + " ");
            }
            ui->textTerminal->append(dtc_list);
        }
        else {
            ui->textTerminal->append("No DTCs reported from ECU " + resp[0]);
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
        ui->textTerminal->append(debugStr);

        std::vector<QString> dtcCodes = elm->decodeDTC(vec);
        if(!dtcCodes.empty()) {
            QString dtc_list = "DTCs: ";
            for(const auto &code : dtcCodes) {
                dtc_list.append(code + " ");
            }
            ui->textTerminal->append(dtc_list);
        }
        else {
            ui->textTerminal->append("Number of DTCs: 0");
        }
    }
}

void MainWindow::refreshData()
{
    // Skip if not connected or busy reading
    if (!m_connected || m_reading) {
        return;
    }

    // Get next command in the runtimeCommands list
    if (!runtimeCommands.isEmpty() && commandOrder < runtimeCommands.size()) {
        m_reading = true;
        QString cmd = runtimeCommands[commandOrder];
        auto data = getData(cmd);

        // Process response
        if (!data.isEmpty() && data != "error") {
            ui->textTerminal->append("<- " + data);
            try {
                analysData(data);
            } catch (...) {
                qDebug() << "Error processing refresh data";
            }
        }

        m_reading = false;
        commandOrder = (commandOrder + 1) % runtimeCommands.size();  // Cycle through commands
    }
}

void MainWindow::startAutoRefresh()
{
    if (!m_refreshTimer.isActive() && m_connected && !runtimeCommands.isEmpty()) {
        m_refreshTimer.start(interval);
        m_autoRefresh = true;
        ui->textTerminal->append("Auto-refresh started");
    }
}

void MainWindow::stopAutoRefresh()
{
    if (m_refreshTimer.isActive()) {
        m_refreshTimer.stop();
        m_autoRefresh = false;
        ui->textTerminal->append("Auto-refresh stopped");
    }
}

// UI Event Handlers

void MainWindow::onConnectClicked()
{
    if(ui->pushConnect->text() == "Connect") {
        QCoreApplication::processEvents();
        ui->textTerminal->clear();
        if(m_connectionManager)
            m_connectionManager->connectElm();
    }
    else {
        if(m_connectionManager)
            m_connectionManager->disConnectElm();
    }
}

void MainWindow::onSendClicked()
{
    if(!m_connected)
        return;

    QString command = ui->sendEdit->text();
    send(command);
}

void MainWindow::onReadClicked()
{
    if(!m_connected)
        return;

    m_reading = true;
    QString command = ui->sendEdit->text();
    auto data = getData(command);

    if(isError(data.toUpper().toStdString())) {
        ui->textTerminal->append("Error : " + data);
    }
    else if (!data.isEmpty()) {
        ui->textTerminal->append("<- " + data);
    }

    analysData(data);
    m_reading = false;
}

void MainWindow::onSetProtocolClicked()
{
    if(!m_connected)
        return;

    QString index = QString::number(ui->protocolCombo->currentIndex());
    if(ui->protocolCombo->currentIndex() == 10)
        index = "A";
    else if(ui->protocolCombo->currentIndex() == 11)
        index = "B";
    else if(ui->protocolCombo->currentIndex() == 12)
        index = "C";
    QString command = "ATTP" + index;
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

    ui->textTerminal->clear();
    if(m_settingsManager) {
        ui->textTerminal->append("Wifi Ip: " + m_settingsManager->getWifiIp() + " : " +
                                 QString::number(m_settingsManager->getWifiPort()));
    }
    ui->textTerminal->append("Resolution : " + QString::number(desktopRect.width()) +
                             "x" + QString::number(desktopRect.height()));
}

void MainWindow::onReadFaultClicked()
{
    if(!m_connected)
        return;

    // First read engine DTCs
    ui->textTerminal->append("-> Reading engine trouble codes...");
    send("ATSH 7E0");  // Set header for engine ECU
    QThread::msleep(60);
    send(READ_TROUBLE);
}

void MainWindow::onClearFaultClicked()
{
    if(!m_connected)
        return;

    ui->textTerminal->append("-> Clearing the trouble codes.");
    send("ATSH 7E0");  // Make sure we're addressing the engine ECU
    QThread::msleep(60);
    send(CLEAR_TROUBLE);
}

void MainWindow::onReadTransFaultClicked()
{
    if(!m_connected)
        return;

    ui->textTerminal->append("-> Reading transmission trouble codes...");

    // First try to select the transmission control module
    send("ATSH 7E1");  // Set header for transmission ECU
    QThread::msleep(100);

    // Read DTCs from transmission
    send(READ_TRANSMISSION);
    QThread::msleep(250);  // Longer delay for complete response
}

void MainWindow::onClearTransFaultClicked()
{
    if(!m_connected)
        return;

    ui->textTerminal->append("-> Clearing transmission trouble codes...");

    // First try to select the transmission control module
    send("ATSH 7E1");  // Set header for transmission ECU
    QThread::msleep(100);

    // Clear DTCs
    send(CLEAR_TRANS_TROUBLE);
    QThread::msleep(250);  // Wait for ECU to process

    ui->textTerminal->append("Transmission codes cleared. Please cycle ignition.");
}

void MainWindow::onScanClicked()
{
    ObdScan *obdScan = new ObdScan(this);
    obdScan->show();
}

void MainWindow::onIntervalSliderChanged(int value)
{
    interval = value * 10;  // Scale to 10-1000ms range
    ui->labelInterval->setText(QString("%1 ms").arg(interval));

    // If auto-refresh is running, restart with new interval
    if (m_refreshTimer.isActive()) {
        m_refreshTimer.setInterval(interval);
    }
}

void MainWindow::onSearchPidsStateChanged(int state)
{
    if(!m_connected)
        return;

    if (state == Qt::Checked) {
        m_searchPidsEnable = true;
        getPids();

        // Optionally start auto-refresh if PIDs were found
        if (!runtimeCommands.isEmpty() && !m_refreshTimer.isActive()) {
            commandOrder = 0;
            //startAutoRefresh();
        }
    } else {
        m_searchPidsEnable = false;
        runtimeCommands.clear();
        //stopAutoRefresh();
    }
}

void MainWindow::onExitClicked()
{
    // Stop auto-refresh if active
    //stopAutoRefresh();

    // Disconnect if connected
    if (m_connected && m_connectionManager) {
        m_connectionManager->disConnectElm();
    }

    // Save settings before exit
    if (m_settingsManager) {
        m_settingsManager->saveSettings();
    }

    QApplication::quit();
}
