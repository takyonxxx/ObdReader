#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "obdscan.h"

#include "global.h"
QStringList runtimeCommands = {};  // initialize
int interval = 100;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("OBD-II Diagnostic Interface");
    QScreen *screen = window()->screen();
    desktopRect = screen->availableGeometry();

    // Set main window background
    this->setStyleSheet(
        "QMainWindow {"
        "    background-color: #1E1E2E;"  // Dark background
        "}"
        );

    applyStyles();

    ui->protocolCombo->setCurrentIndex(3);

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

    ui->sendEdit->setText("0101");

    elm = new ELM();
    elm->resetPids();

    m_settingsManager = SettingsManager::getInstance();
    if(m_settingsManager)
    {       
        saveSettings();
        ui->textTerminal->append("Wifi Ip: " + m_settingsManager->getWifiIp() + " : " + QString::number(m_settingsManager->getWifiPort()));
    }

    m_connectionManager = ConnectionManager::getInstance();
    if(m_connectionManager)
    {
        connect(m_connectionManager,&ConnectionManager::connected,this, &MainWindow::connected);
        connect(m_connectionManager,&ConnectionManager::disconnected,this,&MainWindow::disconnected);
        connect(m_connectionManager,&ConnectionManager::dataReceived,this,&MainWindow::dataReceived);
        connect(m_connectionManager, &ConnectionManager::stateChanged, this, &MainWindow::stateChanged);
    }

    setupIntervalSlider();

    ui->textTerminal->append("Resolution : " + QString::number(desktopRect.width()) + "x" + QString::number(desktopRect.height()));
    ui->textTerminal->append("Press Connect Button");
    ui->pushConnect->setFocus();
}

MainWindow::~MainWindow()
{
    if(m_connectionManager)
    {
        m_connectionManager->disConnectElm();
        delete m_connectionManager;
    }

    if(m_settingsManager)
    {
        m_settingsManager->saveSettings();
        delete m_settingsManager;
    }

    delete ui;
}

void MainWindow::applyStyles()
{
    // Common style variables
    const QString PRIMARY_COLOR = "#89B4FA";      // Light blue
    const QString SECONDARY_COLOR = "#313244";    // Dark gray    
    const QString SUCCESS_COLOR = "#A6E3A1";      // Green
    const QString TEXT_COLOR = "#CDD6F4";         // Light gray

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
                                        "    color: #1E1E2E;"
                                        "}"
                                        "QPushButton:pressed {"
                                        "    background-color: %4;"               // Darker version
                                        "    padding: 14px 18px;"
                                        "}"
                                        ).arg(TEXT_COLOR, SECONDARY_COLOR, PRIMARY_COLOR, "#7497D3");

    // Terminal style
    ui->textTerminal->setStyleSheet(QString(
        "QTextEdit {"
        "    font: 20pt 'Consolas';"
        "    color: #94E2D5;"                     // Cyan text
        "    background-color: #11111B;"          // Darker background
        "    border: 1px solid #313244;"
        "    border-radius: 8px;"
        "    padding: 10px;"
        "}"
        ));

    // Standard buttons
    QList<QPushButton*> standardButtons = {
        ui->pushConnect, ui->pushSend, ui->pushRead,
        ui->pushSetProtocol, ui->pushGetProtocol, ui->pushClear,
        ui->pushScan, ui->pushReadFault, ui->pushClearFault, ui->pushExit
    };
    for (auto* button : standardButtons) {
        button->setStyleSheet(buttonBaseStyle);
    }

    // Checkbox style
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
                                           "    image: url(:/icons/check.png);"      // Optional: Add checkmark icon
                                           "}"
                                           "QCheckBox::indicator:hover {"
                                           "    border-color: %4;"
                                           "}"
                                           ).arg(TEXT_COLOR, PRIMARY_COLOR, SUCCESS_COLOR, PRIMARY_COLOR));

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
                                   ).arg(TEXT_COLOR, SECONDARY_COLOR, PRIMARY_COLOR, "#B4BEFE");

    ui->sendEdit->setStyleSheet(inputStyle);
    ui->protocolCombo->setStyleSheet(inputStyle + QString(
                                         "QComboBox::drop-down {"
                                         "    border: none;"
                                         "    width: 30px;"
                                         "}"
                                         "QComboBox::down-arrow {"
                                         "    image: url(:/icons/dropdown.png);"   // Optional: Add dropdown icon
                                         "    width: 16px;"
                                         "    height: 16px;"
                                         "}"
                                         ));
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
    ui->pushConnect->setText(QString("Connect"));
    commandOrder = 0;
    m_initialized = false;
    m_connected = false;
    ui->textTerminal->append("Elm DisConnected");
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

    if(!supportedPIDs.isEmpty())
    {
        if(supportedPIDs.contains(","))
        {
            runtimeCommands.append(supportedPIDs.split(","));
            QString str = runtimeCommands.join("");
            str = runtimeCommands.join(", ");
        }
    }
}

void MainWindow::dataReceived(QString data)
{
    if(m_reading)
        return;

    data.remove("\r");
    data.remove(">");
    data.remove("?");
    data.remove(",");

    if(isError(data.toUpper().toStdString()))
    {
        ui->textTerminal->append("Error : " + data);
    }
    else if (!data.isEmpty())
    {
        ui->textTerminal->append("<- " + data);
    }

    if(!m_initialized && initializeCommands.size() == commandOrder)
    {
        commandOrder = 0;
        m_initialized = true;

        if(m_searchPidsEnable)
        {
            getPids();
        }
    }
    else if(!m_initialized && commandOrder < initializeCommands.size())
    {
        send(initializeCommands[commandOrder]);
        commandOrder++;
    }

    if(m_initialized && !data.isEmpty())
    {

        try
        {
            data = cleanData(data);
            analysData(data);
        }
        catch (const std::exception& e)
        {
        }
        catch (...)
        {
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
    if(m_connectionManager && m_connected)
    {
        auto cmd = cleanData(command);
        ui->textTerminal->append("-> " + cmd);

        m_connectionManager->send(cmd);
        QThread::msleep(5);
    }

    return QString();
}

bool MainWindow::isError(std::string msg) {
    std::vector<std::string> errors(ERROR, ERROR + 18);
    for(unsigned int i=0; i < errors.size(); i++) {
        if(msg.find(errors[i]) != std::string::npos)
            return true;
    }
    return false;
}

void MainWindow::setupIntervalSlider()
{
    ui->intervalSlider->setMinimum(1);
    ui->intervalSlider->setMaximum(100);
    ui->intervalSlider->setSingleStep(5);
    ui->intervalSlider->setTickInterval(5);
    ui->intervalSlider->setTickPosition(QSlider::TicksBelow);

    ui->labelInterval->setAlignment(Qt::AlignCenter);
    ui->labelInterval->setStyleSheet(
        "QLabel {"
        "    font-size: 28px;"
        "    font-weight: bold;"
        "    color: #2196F3;"
        "    padding: 3px;"
        "    background-color:#313244;"
        "    border-radius: 3px;"
        "    margin: 3px;"
        "}"
        );

    ui->intervalSlider->setMinimumHeight(50);  // Taller for touch
    ui->intervalSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "    border: none;"
        "    height: 20px;"
        "    background: #E0E0E0;"
        "    border-radius: 5px;"
        "    margin: 0px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "        stop:0 #2196F3, stop:1 #1976D2);"
        "    border: none;"
        "    width: 30px;"  // Wider handle for touch
        "    margin: -5px 0;"
        "    border-radius: 10px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "    background: #2196F3;"
        "    border-radius: 5px;"
        "}"
        "QSlider::add-page:horizontal {"
        "    background: #E0E0E0;"
        "    border-radius: 5px;"
        "}"
        "QSlider::tick-mark {"
        "    background: #757575;"
        "    width: 2px;"
        "    height: 5px;"
        "    margin-top: 5px;"
        "}"
        );

    // Set initial value (convert your interval to 0-100 range)
    int initialValue = 10;  // or whatever default you want
    ui->intervalSlider->setValue(initialValue);

    // Connect signal
    connect(ui->intervalSlider, &QSlider::valueChanged,
            this, &MainWindow::onIntervalSliderChanged);

    // Initial label update
    ui->labelInterval->setText(QString("Rate: %1 ms").arg(interval));
}

QString MainWindow::getData(const QString &command)
{
    auto dataReceived = ConnectionManager::getInstance()->readData(command);

    dataReceived.remove("\r");
    dataReceived.remove(">");
    dataReceived.remove("?");
    dataReceived.remove(",");

    if(isError(dataReceived.toUpper().toStdString()))
    {
        QThread::msleep(500);
        return "error";
    }

    dataReceived = cleanData(dataReceived);
    return dataReceived;
}

void MainWindow::saveSettings()
{
    QString ip = "192.168.0.10";
    //QString ip = "192.168.1.16";
    // elm -n 35000 -s car
    quint16 wifiPort = 35000;
    m_settingsManager->setWifiIp(ip);
    m_settingsManager->setWifiPort(wifiPort);
    m_settingsManager->setSerialPort("/dev/ttys001");
    m_settingsManager->setEngineDisplacement(2700);
    m_settingsManager->saveSettings();
}

void MainWindow::analysData(const QString &dataReceived)
{
    unsigned A = 0;
    unsigned B = 0;
    unsigned PID = 0;

    std::vector<QString> vec;
    auto resp= elm->prepareResponseToDecode(dataReceived);

    if(resp.size()>2 && !resp[0].compare("41",Qt::CaseInsensitive))
    {
        QRegularExpression hexMatcher("^[0-9A-F]{2}$", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = hexMatcher.match(resp[1]);
        if (!match.hasMatch())
            return;

        PID =std::stoi(resp[1].toStdString(),nullptr,16);
        std::vector<QString> vec;

        vec.insert(vec.begin(),resp.begin()+2, resp.end());
        if(vec.size()>=2)
        {
            A = std::stoi(vec[0].toStdString(),nullptr,16);
            B = std::stoi(vec[1].toStdString(),nullptr,16);
        }
        else if(vec.size()>=1)
        {
            A = std::stoi(vec[0].toStdString(),nullptr,16);
            B = 0;
        }

        //ui->textTerminal->append("Pid: " + QString::number(PID) + "  A: " + QString::number(A)+ "  B: " + QString::number(B));
    }

    //number of dtc & mil
    if(resp.size()>2 && !resp[0].compare("41",Qt::CaseInsensitive) && !resp[1].compare("01",Qt::CaseInsensitive))
    {
        vec.insert(vec.begin(),resp.begin()+2, resp.end());
        std::pair<int,bool> dtcNumber = elm->decodeNumberOfDtc(vec);
        QString milText = dtcNumber.second ? "true" : "false";
        ui->textTerminal->append("Number of Dtcs: " +  QString::number(dtcNumber.first) + ",  Mil on: " + milText);
    }
    //dtc codes
    if(resp.size()>1 && !resp[0].compare("43",Qt::CaseInsensitive))
    {
        //auto resp= elm->prepareResponseToDecode("486B104303000302030314486B10430304000000000D");
        vec.insert(vec.begin(),resp.begin()+1, resp.end());
        std::vector<QString> dtcCodes( elm->decodeDTC(vec));
        if(dtcCodes.size()>0)
        {
            QString dtc_list{"Dtcs: "};
            for(auto &code : dtcCodes)
            {
                dtc_list.append(code + " ");
            }
            ui->textTerminal->append(dtc_list);
        }
        else
            ui->textTerminal->append("Number of Dtcs: 0");
    }
}

void MainWindow::onConnectClicked()
{
    if(ui->pushConnect->text() == "Connect")
    {
        QCoreApplication::processEvents();
        ui->textTerminal->clear();
        if(m_connectionManager)
            m_connectionManager->connectElm();
    }
    else
    {
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
    auto dataReceived = getData(command);
    ui->textTerminal->append("<- " + dataReceived);
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
    if(m_settingsManager)
    {
        ui->textTerminal->append("Wifi Ip: " + m_settingsManager->getWifiIp() + " : " + QString::number(m_settingsManager->getWifiPort()));
    }
    ui->textTerminal->append("Resolution : " + QString::number(desktopRect.width()) + "x" + QString::number(desktopRect.height()));
}

void MainWindow::onReadFaultClicked()
{
    if(!m_connected)
        return;

    ui->textTerminal->append("-> Reading the trouble codes.");
    QThread::msleep(60);
    send(READ_TROUBLE);
}

void MainWindow::onClearFaultClicked()
{
    if(!m_connected)
        return;

    ui->textTerminal->append("-> Clearing the trouble codes.");
    QThread::msleep(60);
    send(CLEAR_TROUBLE);
}

void MainWindow::onScanClicked()
{
    ObdScan *obdScan = new ObdScan(this);
    //obdScan->setGeometry(desktopRect);
    obdScan->show();
}

void MainWindow::onIntervalSliderChanged(int value)
{
    interval = value * 10;  // This will give you 0-1000 ms range
    ui->labelInterval->setText(QString("Interval: %1 ms").arg(interval));
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
