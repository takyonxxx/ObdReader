#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "obdscan.h"

#include "global.h"
QStringList runtimeCommands = {};  // initialize
int interval = 200;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QScreen *screen = window()->screen();
    desktopRect = screen->availableGeometry();

    ui->textTerminal->setStyleSheet("font: 20pt; color: #00cccc; background-color: #001a1a;");
    ui->pushConnect->setStyleSheet("font-size: 24pt; font-weight: bold; color: white;background-color:#154360; padding: 3px; spacing: 3px;");
    ui->pushSend->setStyleSheet("font-size: 24pt; font-weight: bold; color: white;background-color: #154360; padding: 3px; spacing: 3px;");
    ui->pushRead->setStyleSheet("font-size: 24pt; font-weight: bold; color: white;background-color: #154360; padding: 3px; spacing: 3px;");
    ui->pushSetProtocol->setStyleSheet("font-size: 24pt; font-weight: bold; color: white;background-color: #154360; padding: 3px; spacing: 3px;");
    ui->pushGetProtocol->setStyleSheet("font-size: 24pt; font-weight: bold; color: white;background-color: #154360; padding: 3px; spacing: 3px;");
    ui->pushClear->setStyleSheet("font-size: 24pt; font-weight: bold; color: white;background-color: #154360; padding: 3px; spacing: 3px;");
    ui->pushReadFault->setStyleSheet("font-size: 24pt; font-weight: bold; color: white; background-color: #0B5345; padding: 3px; spacing: 3px;");
    ui->pushClearFault->setStyleSheet("font-size: 24pt; font-weight: bold; color: white; background-color: #0B5345; padding: 3px; spacing: 3px;");
    ui->pushScan->setStyleSheet("font-size: 24pt; font-weight: bold; color: white;background-color: #154360 ; padding: 6px; spacing: 6px;");
    ui->pushExit->setStyleSheet("font-size: 24pt; font-weight: bold; color: white;background-color: #512E5F; padding: 3px; spacing: 3px;");
    ui->checkSearchPids->setStyleSheet(
        "QCheckBox {"
        "   font-size: 24pt;"
        "   font-weight: bold;"
        "   color: white;"
        "   background-color: #0c677a;"
        "   padding: 6px;"
        "   spacing: 12px;"
        "}"
        "QCheckBox::indicator {"
        "   width: 25px;"
        "   height: 25px;"
        "   border: 2px solid white;"
        "   border-radius: 4px;"
        "}"
        "QCheckBox::indicator:unchecked {"
        "   background-color: transparent;"
        "}"
        "QCheckBox::indicator:checked {"
        "   background-color: #2ECC71;"  // Green when checked
        "}"
        "QCheckBox::indicator:hover {"
        "   border-color: #85C1E9;"
        "}"
        );
    ui->sendEdit->setStyleSheet("font-size: 20pt; font-weight: bold; color:white; padding: 3px; spacing: 3px;");
    ui->protocolCombo->setStyleSheet("font-size: 20pt; font-weight: bold; color:white; padding: 3px; spacing: 3px;");
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


void MainWindow::connected()
{
    ui->pushConnect->setStyleSheet("font-size: 22pt; font-weight: bold; color: white;background-color:#154360; padding: 24px; spacing: 24px;");
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
    //QString ip = "192.168.0.10";
    QString ip = "192.168.1.16";
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
    if(m_settingsManager)
    {
        saveSettings();
    }

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
    ui->textTerminal->clear();
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
    obdScan->setGeometry(desktopRect);
    obdScan->move(this->x(), this->y());
    obdScan->show();
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
