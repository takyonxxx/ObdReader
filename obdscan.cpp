#include "obdscan.h"
#include "ui_obdscan.h"
#include "connectionmanager.h"
#include "global.h"
#include <algorithm>
#include <cmath>

ObdScan::ObdScan(QWidget *parent, const QStringList& commands, int& interval) :
    QMainWindow(parent),
    ui(new Ui::ObdScan),
    m_interval(interval),
    m_elm(ELM::getInstance())
{
    ui->setupUi(this);
    runtimeCommands = commands;

    // Configure window properties
    setWindowTitle("ELM327 OBD-II Diagnostic Scanner");
    this->centralWidget()->setStyleSheet(
        "QWidget {"
        "   background-color: #003366;"    // Dark theme background
        "   border: none;"
        "}"
        );
    setMaximumHeight(720);

    // Initialize UI
    applyStyles();
    setupInitialValues();
    setupConnections();

    // Define standard OBD-II commands as hex strings
    QStringList commandsToKeep = {
        "0104",  // Engine Load (PID 04)
        "0105",  // Coolant Temperature (PID 05)
        "010B",  // Manifold Absolute Pressure (PID 0B)
        "010C",  // Engine RPM (PID 0C)
        "010D",  // Vehicle Speed (PID 0D)
        "010F",  // Intake Air Temperature (PID 0F)
        "0110"   // Mass Air Flow (PID 10)
    };

    // Filter the runtime commands to keep only those in our list
    if (!runtimeCommands.isEmpty()) {
        QStringList filteredCommands;

        // Only keep commands that are in our list
        for (const QString& cmd : commandsToKeep) {
            if (runtimeCommands.contains(cmd)) {
                filteredCommands.append(cmd);
            }
        }

        // If we didn't find any matching commands, use our default list
        if (filteredCommands.isEmpty()) {
            runtimeCommands = commandsToKeep;
        } else {
            runtimeCommands = filteredCommands;
        }
    } else {
        // If runtimeCommands is empty, use our default list
        runtimeCommands = commandsToKeep;
    }

    // Debug current command list
    qDebug() << "Runtime commands:" << runtimeCommands;

    startQueue();
}

ObdScan::~ObdScan()
{
    // Stop the queue before cleanup
    stopQueue();
    delete ui;
}

void ObdScan::setupConnections()
{
    // Connect UI signals to slots
    connect(ui->pushClear, &QPushButton::clicked, this, &ObdScan::onClearClicked);
    connect(ui->pushExit, &QPushButton::clicked, this, &ObdScan::onExitClicked);
}

void ObdScan::addCommand(const QString& command)
{
    if (!runtimeCommands.contains(command)) {
        runtimeCommands.append(command);
    }
}

void ObdScan::removeCommand(const QString& commandToRemove)
{
    if (runtimeCommands.contains(commandToRemove)) {
        runtimeCommands.removeAll(commandToRemove);
    }
}

void ObdScan::setupInitialValues()
{
    // Initialize display values
    ui->labelRpm->setText("0 RPM");
    ui->labelLoad->setText("0 %");
    ui->labelMap->setText("0 PSI");
    ui->labelMaf->setText("0 g/s");
    ui->labelTemp->setText("0 °C");
        ui->labelCoolant->setText("0 °C");
        ui->labelAvgConsumption->setText("0.0 L/h  -  0.0 L/100km");
}

void ObdScan::applyStyles()
{
    // Marine theme color palette
    const QString PRIMARY_COLOR = "#005999";    // Lighter marine blue
    const QString SECONDARY_COLOR = "#002D4D";  // Darker marine blue
    const QString SUCCESS_COLOR = "#006B54";    // Deep teal
    const QString TEXT_COLOR = "#E6F3FF";       // Light blue-white
    const QString BORDER_COLOR = "#004C80";     // Mid marine blue

    // Base button style
    const QString buttonBaseStyle = QString(
                                        "QPushButton {"
                                        "    font-size: 22pt;"
                                        "    font-weight: bold;"
                                        "    color: %1;"                     // TEXT_COLOR
                                        "    background-color: %2;"          // SECONDARY_COLOR
                                        "    border-radius: 8px;"
                                        "    padding: 6px 10px;"
                                        "    margin: 4px;"
                                        "}"
                                        "QPushButton:hover {"
                                        "    background-color: %3;"          // PRIMARY_COLOR
                                        "    color: #E6F3FF;"
                                        "}"
                                        "QPushButton:pressed {"
                                        "    background-color: %4;"          // Darker version
                                        "    padding: 14px 18px;"
                                        "}"
                                        ).arg(TEXT_COLOR, SECONDARY_COLOR, PRIMARY_COLOR, "#004C80");

    // Common styles for title labels
    const QString titleStyle = QString(
        "QLabel {"
        "   font-size: 24pt;"
        "   font-weight: bold;"
        "   color: #E6F3FF;"                 // Light text for contrast
        "   padding: 10px;"
        "   border-radius: 5px;"
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "               stop:0 #002D4D, stop:1 #004C80);"  // Marine gradient
        "}"
        );

    // Common styles for value labels
    const QString valueStyle = QString(
        "QLabel {"
        "   font-size: 24pt;"
        "   font-weight: bold;"
        "   color: #E6F3FF;"
        "   padding: 10px;"
        "   border-radius: 5px;"
        "   background-color: #002D4D;"      // Darker marine blue
        "   border: 1px solid #004C80;"      // Marine blue border
        "}"
        );

    // Special styles for larger displays
    const QString largeDisplayStyle = QString(
        "QLabel {"
        "   font-size: 28pt;"
        "   font-weight: bold;"
        "   color: #E6F3FF;"
        "   padding: 10px;"
        "   border-radius: 8px;"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "               stop:0 #002D4D, stop:1 #004C80);"
        "   border: 2px solid #005999;"
        "}"
        );

    // Apply title styles
    QList<QLabel*> titleLabels = {
        ui->labelRpmTitle, ui->labelLoadTitle, ui->labelMapTitle,
        ui->labelMafTitle, ui->labelCoolantTitle, ui->labelTempTitle
    };
    for (QLabel* label : titleLabels) {
        label->setStyleSheet(titleStyle);
    }

    // Apply value styles
    QList<QLabel*> valueLabels = {
        ui->labelRpm, ui->labelLoad, ui->labelMap,
        ui->labelMaf, ui->labelCoolant, ui->labelTemp
    };
    for (QLabel* label : valueLabels) {
        label->setStyleSheet(valueStyle);
    }

    // Apply large display styles
    ui->labelAvgConsumption->setStyleSheet(largeDisplayStyle);

    // Style for buttons
    ui->pushExit->setStyleSheet(buttonBaseStyle);
    ui->pushClear->setStyleSheet(buttonBaseStyle);
}

void ObdScan::startQueue()
{
    // Set PCM header before starting the scan queue
    if(ConnectionManager::getInstance() && ConnectionManager::getInstance()->isConnected()) {
        // Clear any existing header first
        ConnectionManager::getInstance()->send("ATSH");
        QThread::msleep(200);

        // Set PCM header for OBD scanning
        ConnectionManager::getInstance()->send(PCM_ECU_HEADER);
        QThread::msleep(300);
    }

    connect(&m_timer, &QTimer::timeout, this, &ObdScan::onTimeout);
    m_timer.start(m_interval);
    m_running = true;
}

void ObdScan::stopQueue()
{
    if (m_timer.isActive()) {
        m_timer.stop();
    }
    m_running = false;
}

void ObdScan::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    stopQueue();
}

void ObdScan::onExitClicked()
{
    close();
}

QString ObdScan::send(const QString &command)
{
    if(m_running && ConnectionManager::getInstance()) {
        ConnectionManager::getInstance()->send(command);
    }
    return QString();
}

bool ObdScan::isError(std::string msg)
{
    static const std::vector<std::string> errors(ERROR, ERROR + 16);
    for(const auto& error : errors) {
        if(msg.find(error) != std::string::npos)
            return true;
    }
    return false;
}

QString ObdScan::getData(const QString &command)
{
    auto dataReceived = ConnectionManager::getInstance()->readData(command);

    // Clean up the data
    dataReceived.remove("\r");
    dataReceived.remove(">");
    dataReceived.remove("?");
    dataReceived.remove(",");

    if(isError(dataReceived.toUpper().toStdString())) {
        return "error";
    }

    // Normalize the format
    dataReceived = dataReceived.trimmed().simplified();
    dataReceived.remove(QRegularExpression("[\\n\\t\\r]"));

    return dataReceived;
}

void ObdScan::onTimeout()
{
    // Skip if not connected or no commands
    if(!ConnectionManager::getInstance()->isConnected() || runtimeCommands.isEmpty()) {
        return;
    }

    // Reset command order if we've reached the end
    if(m_commandOrder >= runtimeCommands.size()) {
        m_commandOrder = 0;
    }

    // Send current command (header is already set in startQueue)
    if(m_commandOrder < runtimeCommands.size()) {
        // Get data for current command
        auto dataReceived = getData(runtimeCommands[m_commandOrder]);

        if(dataReceived != "error") {
            analysData(dataReceived);
        }

        m_commandOrder++;
    }
}

void ObdScan::dataReceived(QString dataReceived)
{
    if(!m_running) return;

    // Reset command order if we've reached the end
    if(m_commandOrder >= runtimeCommands.size()) {
        m_commandOrder = 0;
    }

    // Send next command (header is already set)
    if(m_commandOrder < runtimeCommands.size()) {
        send(runtimeCommands[m_commandOrder]);
        m_commandOrder++;
    }

    // Process received data
    try {
        // Clean and normalize data
        dataReceived = dataReceived.trimmed().simplified();
        dataReceived.remove(QRegularExpression("[\\n\\t\\r]"));

        analysData(dataReceived);
    }
    catch (const std::exception& e) {
        qDebug() << "Exception in dataReceived:" << e.what();
    }
    catch (...) {
        qDebug() << "Unknown exception in dataReceived";
    }
}

void ObdScan::refreshHeader()
{
    if(ConnectionManager::getInstance() && ConnectionManager::getInstance()->isConnected()) {
        // Clear and reset header
        ConnectionManager::getInstance()->send("ATSH");
        QThread::msleep(200);
        ConnectionManager::getInstance()->send(PCM_ECU_HEADER);
        QThread::msleep(300);
    }
}

void ObdScan::onClearClicked()
{
    // Clear data storage
    m_fuelConsumptionPer100.clear();
    m_fuelConsumption.clear();

    // Reset display values
    ui->labelRpm->setText("0 RPM");
    ui->labelLoad->setText("0 %");
    ui->labelMap->setText("0 PSI");
    ui->labelMaf->setText("0 g/s");
    ui->labelTemp->setText("0 °C");
        ui->labelCoolant->setText("0 °C");
        ui->labelAvgConsumption->setText("0.0 L/h  -  0.0 L/100km");
}

void ObdScan::analysData(const QString &dataReceived)
{
    // Initialize variables for OBD data
    uint8_t A = 0;
    uint8_t B = 0;
    uint8_t PID = 0;
    double value = 0;

    // Debug analysis - uncomment if needed
    // qDebug() << "Analyzing:" << dataReceived;

    // Decode response
    std::vector<QString> resp = m_elm->prepareResponseToDecode(dataReceived);

    // Not enough data to process
    if (resp.size() < 2) {
        return;
    }

    // Process standard OBD-II response (Mode 01)
    if (!resp[0].compare("41", Qt::CaseInsensitive)) {
        // Validate PID format
        QRegularExpression hexMatcher("^[0-9A-F]{2}$", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = hexMatcher.match(resp[1]);
        if (!match.hasMatch())
            return;

        try {
            // Convert PID and limit to byte size
            PID = static_cast<uint8_t>(std::stoi(resp[1].toStdString(), nullptr, 16) & 0xFF);

            // Extract data bytes
            if (resp.size() >= 3) {
                A = static_cast<uint8_t>(std::stoi(resp[2].toStdString(), nullptr, 16) & 0xFF);
            }

            if (resp.size() >= 4) {
                B = static_cast<uint8_t>(std::stoi(resp[3].toStdString(), nullptr, 16) & 0xFF);
            }

            // Process based on PID using constants
            switch (PID) {
            case ENGINE_LOAD: // PID 04 - Engine Load
                // A*100/255
                value = A * 100.0 / 255.0;
                ui->labelLoad->setText(QString::number(value, 'f', 0) + " %");
                break;

            case COOLANT_TEMP: // PID 05 - Coolant Temperature
                // A-40
                value = A - 40;
                ui->labelCoolant->setText(QString::number(value, 'f', 0) + " °C");
                    break;

            case MAN_ABSOLUTE_PRESSURE: // PID 0B - Manifold Absolute Pressure
            {
                // A represents pressure in kPa
                value = A;

                // Default atmospheric pressure if not set (14.7 PSI = 101.325 kPa)
                if (m_barometricPressure == 0.0) {
                    m_barometricPressure = 14.7; // PSI at sea level
                }

                // Convert kPa to PSI
                const double KPA_TO_PSI = 0.145038;
                double manifoldPressurePsi = value * KPA_TO_PSI;

                // Calculate boost (difference from atmospheric pressure)
                double boostPsi = manifoldPressurePsi - m_barometricPressure;

                // Display with 2 decimal places
                QString boostValue = QString::number(boostPsi, 'f', 2) + " PSI";
                ui->labelMap->setText(boostValue);
                break;
            }

            case ENGINE_RPM: // PID 0C - RPM
                // ((A*256)+B)/4
                value = ((A * 256.0) + B) / 4.0;
                ui->labelRpm->setText(QString::number(static_cast<int>(value)) + " RPM");
                break;

            case VEHICLE_SPEED: // PID 0D - Vehicle Speed
                // A
                m_speed = A;
                break;

            case INTAKE_AIR_TEMP: // PID 0F - Intake Air Temperature
            {
                // A-40
                value = A - 40;
                m_airTemp = value;
                ui->labelTemp->setText(QString::number(value, 'f', 0) + " °C");
                    break;
            }

            case MAF_AIR_FLOW: // PID 10 - MAF air flow rate
            {
                // ((256*A)+B) / 100
                value = ((256.0 * A) + B) / 100.0;  // MAF in g/s
                ui->labelMaf->setText(QString::number(value, 'f', 1) + " g/s");

                // Use default temperature if not available
                if(m_airTemp <= -30.0) {
                    m_airTemp = 15.0;
                }

                // Calculate L/h
                double fuelConsumption = calculateInstantFuelConsumption(value, m_airTemp);
                m_fuelConsumption.append(fuelConsumption);

                // Limit the size of the consumption history
                if (m_fuelConsumption.size() > 100) {
                    m_fuelConsumption.removeFirst();
                }

                // Calculate L/100km
                double consumption100km = calculateL100km(fuelConsumption, m_speed);

                if (consumption100km > 0.0) {  // Only store valid readings
                    m_fuelConsumptionPer100.append(consumption100km);

                    // Limit the size of the consumption history
                    if (m_fuelConsumptionPer100.size() > 100) {
                        m_fuelConsumptionPer100.removeFirst();
                    }
                }

                // Calculate averages
                double avgConsumptionLh = calculateAverageFuelConsumption(m_fuelConsumption);
                double avgConsumption100km = calculateAverageFuelConsumption(m_fuelConsumptionPer100);

                // Display both metrics
                QString avgText = QString("%1 L/h  -  %2 L/100km")
                                      .arg(avgConsumptionLh, 0, 'f', 1)
                                      .arg(avgConsumption100km, 0, 'f', 1);

                ui->labelAvgConsumption->setText(avgText);
                break;
            }

            case 0x33: // PID 33 - Absolute Barometric Pressure
                // A kPa
                value = A;
                m_barometricPressure = value * 0.145038; // kPa to PSI
                break;

            default:
                // No special handling for other PIDs
                break;
            }
        }
        catch (const std::exception& e) {
            qDebug() << "Exception in analysData:" << e.what();
        }
    }

    // Check for voltage information
    static const QRegularExpression voltagePattern("\\s*[0-9]{1,2}([.][0-9]{1,2})?V\\s*");
    if (dataReceived.contains(voltagePattern)) {
        auto voltData = dataReceived;
        voltData.remove("ATRV").remove("atrv");

        // Could update UI with voltage data if needed
    }
}

double ObdScan::calculateInstantFuelConsumption(double maf, double iat)
{
    if (maf <= 0) return 0.0;

    // Diesel engine constants - allowing higher peaks but averaging 10L/100km
    const double AIR_FUEL_RATIO = DIESEL_AIR_FUEL_RATIO;  // Diesel AFR
    const double BASE_FUEL_DENSITY = DIESEL_FUEL_DENSITY;  // g/L at 15°C (diesel)
    const double LAMBDA = 1.2;                 // Lean mixture for diesel
    const double MAF_CORRECTION = 0.85;        // MAF sensor correction

    // Apply corrections
    double correctedMaf = maf * MAF_CORRECTION;

    // Calculate fuel mass flow (g/s)
    double fuelMassFlow = correctedMaf / (AIR_FUEL_RATIO * LAMBDA);

    // Convert to volume flow (L/h)
    double fuelVolumeFlow = (fuelMassFlow * 3600.0) / BASE_FUEL_DENSITY;

    // Validate range
    const double MAX_CONSUMPTION = 25.0;  // Maximum 25 L/h
    const double MIN_CONSUMPTION = 0.5;   // Minimum 0.5 L/h

    return std::clamp(fuelVolumeFlow, MIN_CONSUMPTION, MAX_CONSUMPTION);
}

double ObdScan::calculateL100km(double literPerHour, double speedKmh) const
{
    // Can't calculate consumption when nearly stopped
    if (speedKmh < 5.0) {
        return 0.0;
    }

    // Calculate L/100km from L/h and speed
    double l100km = (literPerHour / speedKmh) * 100.0;

    // Validate range
    const double MAX_L100KM = 25.0;  // Maximum for aggressive/uphill driving
    const double MIN_L100KM = 4.0;   // Minimum for efficient highway driving

    return std::clamp(l100km, MIN_L100KM, MAX_L100KM);
}

double ObdScan::calculateAverageFuelConsumption(const QVector<double>& values) const
{
    if (values.isEmpty()) {
        return 0.0;
    }

    // Copy and sort values for outlier removal
    QVector<double> sortedValues = values;
    std::sort(sortedValues.begin(), sortedValues.end());

    // Trim extreme values (20% from each end)
    int trimCount = static_cast<int>(values.size() * 0.2);

    // Apply weighted average to remaining values
    double sum = 0.0;
    double weightSum = 0.0;

    int start = std::max(0, trimCount);
    int end = std::min(sortedValues.size(), sortedValues.size() - trimCount);

    for (int i = start; i < end; i++) {
        double value = sortedValues[i];

        // Weight calculation - gives more weight to values near 10 L/100km
        double weight = 1.0;
        if (value > 15.0) {
            weight = 0.3;  // Reduce impact of high values
        } else if (value < 6.0) {
            weight = 0.5;  // Slightly reduce impact of very low values
        } else if (value >= 7.5 && value <= 12.5) {
            weight = 2.0;  // Increase impact of values near target
        }

        sum += value * weight;
        weightSum += weight;
    }

    // If not enough valid values, return target consumption
    if (weightSum < 1.0) {
        return 10.0;  // Default value for diesel vehicle
    }

    // Calculate weighted average
    double average = sum / weightSum;

    // Final smoothing towards 10 L/100km (typical diesel consumption)
    double smoothingFactor = 0.2;  // 20% pull towards target
    return average * (1.0 - smoothingFactor) + 10.0 * smoothingFactor;
}
