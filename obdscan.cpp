#include "obdscan.h"
#include "ui_obdscan.h"
#include "global.h"
#include "connectionmanager.h"

ObdScan::ObdScan(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ObdScan)
{
    ui->setupUi(this);

    setWindowTitle("ELM327 OBD-II Diagnostic Scanner");
    this->centralWidget()->setStyleSheet(
        "QWidget {"
        "   background-color: #003366;"    // Dark theme background
        "   border: none;"
        "}"
        );

    setMaximumHeight(720);

    applyStyles();
    setupInitialValues();

    removeCommand(PIDS_SUPPORTED20);
    removeCommand(THROTTLE_POSITION);
    removeCommand(OBD_STANDARDS);

    if(runtimeCommands.isEmpty())
    {       
        runtimeCommands.append(ENGINE_LOAD);
        runtimeCommands.append(COOLANT_TEMP);
        runtimeCommands.append(MAN_ABSOLUTE_PRESSURE);
        runtimeCommands.append(ENGINE_RPM);
        runtimeCommands.append(VEHICLE_SPEED);
        runtimeCommands.append(INTAKE_AIR_TEMP);
        runtimeCommands.append(MAF_AIR_FLOW);
    }

    // Debug remaining commands
    qDebug() << "\nCurrent runtime commands:";
    for (const auto& cmd : runtimeCommands) {
        qDebug() << " -" << cmd;
    }

    startQueue();
}

ObdScan::~ObdScan()
{
    delete ui;
}

void ObdScan::removeCommand(const QString& commandToRemove)
{
    // Check if command exists before removing
    if (runtimeCommands.contains(commandToRemove)) {
        runtimeCommands.removeAll(commandToRemove);
    } else {
        qDebug() << "Command not found in runtime commands:" << commandToRemove;
    }
}

void ObdScan::setupInitialValues()
{
    connect(ui->pushClear, &QPushButton::clicked, this, &ObdScan::onClearClicked);
    connect(ui->pushExit, &QPushButton::clicked, this, &ObdScan::onExitClicked);

    // Initialize other default values as needed
    ui->labelRpm->setText("0 RPM");
    ui->labelLoad->setText("0 %");
    ui->labelMap->setText("0 PSI");
    ui->labelMaf->setText("0 g/s");
    ui->labelTemp->setText("0 °C");
    ui->labelCoolant->setText("0 °C");
}

void ObdScan::applyStyles()
{
    // Marine theme color palette
    const QString PRIMARY_COLOR = "#005999";      // Lighter marine blue
    const QString SECONDARY_COLOR = "#002D4D";    // Darker marine blue
    const QString SUCCESS_COLOR = "#006B54";      // Deep teal
    const QString TEXT_COLOR = "#E6F3FF";         // Light blue-white
    const QString BORDER_COLOR = "#004C80";       // Mid marine blue

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
                                        ).arg(TEXT_COLOR, SECONDARY_COLOR, PRIMARY_COLOR, "#004C80");

    // Common styles for title labels
    const QString titleStyle = QString(
        "QLabel {"
        "   font-size: 24pt;"
        "   font-weight: bold;"
        "   color: #E6F3FF;"              // Light text for contrast
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
        "   background-color: #002D4D;"    // Darker marine blue
        "   border: 1px solid #004C80;"    // Marine blue border
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
    QList<QLabel*> largeDisplays = {
        ui->labelAvgConsumption
    };
    for (QLabel* label : largeDisplays) {
        label->setStyleSheet(largeDisplayStyle);
    }

    // Style for buttons
    ui->pushExit->setStyleSheet(buttonBaseStyle);
    ui->pushClear->setStyleSheet(buttonBaseStyle);
}

void ObdScan::startQueue()
{
    connect(&m_timer, &QTimer::timeout, this, &ObdScan::onTimeout);
    m_timer.start(interval);
}

void ObdScan::stopQueue()
{
    if (m_timer.isActive()) {
        m_timer.stop();
    }
}

void ObdScan::closeEvent (QCloseEvent *event)
{
    Q_UNUSED(event);
    mRunning = false;
    stopQueue();
}

void ObdScan::onExitClicked()
{
    mRunning = false;
    close();
}

QString ObdScan::send(const QString &command)
{
    if(mRunning && ConnectionManager::getInstance())
    {       
        ConnectionManager::getInstance()->send(command);
    }

    return QString();
}


bool ObdScan::isError(std::string msg) {
    std::vector<std::string> errors(ERROR, ERROR + 18);
    for(unsigned int i=0; i < errors.size(); i++) {
        if(msg.find(errors[i]) != std::string::npos)
            return true;
    }
    return false;
}

QString ObdScan::getData(const QString &command)
{
    auto dataReceived =ConnectionManager::getInstance()->readData(command);

    dataReceived.remove("\r");
    dataReceived.remove(">");
    dataReceived.remove("?");
    dataReceived.remove(",");

    if(isError(dataReceived.toUpper().toStdString()))
    {
        return "error";
    }

    dataReceived = dataReceived.trimmed().simplified();
    dataReceived.remove(QRegularExpression("[\\n\\t\\r]"));
    dataReceived.remove(QRegularExpression("[^a-zA-Z0-9]+"));
    return dataReceived;
}

void ObdScan::onTimeout()
{

    if(!ConnectionManager::getInstance()->isConnected())
        return;

    if(runtimeCommands.size() == 0)
        return;

    if(runtimeCommands.size() == commandOrder)
    {
        commandOrder = 0;
    }

    if(commandOrder < runtimeCommands.size())
    {
        auto dataReceived = getData(runtimeCommands[commandOrder]);
        if(dataReceived != "error")
            analysData(dataReceived);        
        commandOrder++;
    }
}


void ObdScan::dataReceived(QString dataReceived)
{
    if(!mRunning)return;

    if(runtimeCommands.size() == commandOrder)
    {
        commandOrder = 0;
        send(runtimeCommands[commandOrder]);
        //ui->labelCommand->setText(runtimeCommands.join(", ") + "\n" + runtimeCommands[commandOrder]);
    }

    if(commandOrder < runtimeCommands.size())
    {
        send(runtimeCommands[commandOrder]);
        //ui->labelCommand->setText(runtimeCommands.join(", ") + "\n" + runtimeCommands[commandOrder]);
        commandOrder++;
    }

    try
    {
        dataReceived = dataReceived.trimmed().simplified();
        dataReceived.remove(QRegularExpression("[\\n\\t\\r]"));
        dataReceived.remove(QRegularExpression("[^a-zA-Z0-9]+"));
        analysData(dataReceived);
    }
    catch (const std::exception& e)
    {       
    }
    catch (...)
    {        
    }

}

void ObdScan::onClearClicked()
{
    mFuelConsumptionPer100.clear();
    mFuelConsumption.clear();

    // Initialize other default values as needed
    ui->labelRpm->setText("0 RPM");
    ui->labelLoad->setText("0 %");
    ui->labelMap->setText("0 PSI");
    ui->labelMaf->setText("0 g/s");
    ui->labelCoolant->setText("0 °C");
}

void ObdScan::analysData(const QString &dataReceived)
{
    uint8_t A = 0;
    uint8_t B = 0;
    uint8_t PID = 0;
    double value = 0;

    std::vector<QString> vec;
    auto resp = elm->prepareResponseToDecode(dataReceived);

    if(resp.size() > 2 && !resp[0].compare("41", Qt::CaseInsensitive))
    {
        QRegularExpression hexMatcher("^[0-9A-F]{2}$", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = hexMatcher.match(resp[1]);
        if (!match.hasMatch())
            return;

        // Limit PID to byte size
        PID = static_cast<uint8_t>(std::stoi(resp[1].toStdString(), nullptr, 16) & 0xFF);

        vec.insert(vec.begin(), resp.begin()+2, resp.end());
        if(vec.size() >= 2)
        {
            // Limit A and B to byte size
            A = static_cast<uint8_t>(std::stoi(vec[0].toStdString(), nullptr, 16) & 0xFF);
            B = static_cast<uint8_t>(std::stoi(vec[1].toStdString(), nullptr, 16) & 0xFF);
        }
        else if(vec.size() >= 1)
        {
            A = static_cast<uint8_t>(std::stoi(vec[0].toStdString(), nullptr, 16) & 0xFF);
            B = 0;
        }

        switch (PID)
        {

        case 4://PID(04): Engine Load
            // A*100/255
            value = A * 100 / 255;
            ui->labelLoad->setText(QString::number(value, 'f', 0) + " %");
            break;
        case 5://PID(05): Coolant Temperature
            // A-40
            value = A - 40;
            ui->labelCoolant->setText(QString::number(value, 'f', 0) + " C°");
            break;
        case 10://PID(0A): Fuel Pressure
            // A * 3
            value = A * 3;
            break;
        case 11://PID(0B): Manifold Absolute Pressure
        {
            // A represents pressure in kPa
            value = A;

            // Default atmospheric pressure if not set (14.7 PSI = 101.325 kPa)
            if (barometric_pressure == 0.0) {
                barometric_pressure = 14.7; // PSI at sea level
            }

            // Convert kPa to PSI
            // 1 kPa = 0.145038 PSI
            const double KPA_TO_PSI = 0.145038;
            double manifoldPressurePsi = value * KPA_TO_PSI;

            // Calculate boost (difference from atmospheric pressure)
            double boostPsi = manifoldPressurePsi - barometric_pressure;

            // Display with 2 decimal places
            // If boost is negative, it means vacuum
            QString boostValue = QString::number(boostPsi, 'f', 2) + " PSI";
            ui->labelMap->setText(boostValue);
        }
            break;
        case 12: //PID(0C): RPM
            //((A*256)+B)/4
            value = ((A * 256) + B) / 4;
            ui->labelRpm->setText(QString::number(value) + " RPM");
            break;
        case 13://PID(0D): KM Speed
            // A
            value = A;
            m_speed = value;
            //ui->labelSpeed->setText(QString::number(value) + " km/h");
            break;
        case 15: //PID(0F): Intake Air Temperature
        {
            // Convert to signed int16_t before subtraction
            int16_t temp = static_cast<int16_t>(A) - 40;

            // Validate temperature range (-40°C to 215°C)
            if (temp >= -40 && temp <= 215) {
                value = temp;
                air_temp = value;
                ui->labelTemp->setText(QString::number(value, 'f', 0) + " C°");
            } else {
                // Handle invalid temperature
                ui->labelTemp->setText("Error");
            }
            break;
        }
        case 16://PID(10): MAF air flow rate grams/sec
        {
            value = ((256*A)+B) / 100;  // MAF in g/s
            ui->labelMaf->setText(QString::number(value) + " g/s");

            if(air_temp <= 0)
                air_temp = 15.0;

            // Calculate L/h
            double fuelConsumption = calculateInstantFuelConsumption(value, air_temp);
            mFuelConsumption.append(fuelConsumption);

            // Calculate L/100km
            double consumption100km = calculateL100km(fuelConsumption, m_speed);

            if (consumption100km > 0.0) {  // Only store valid readings
                mFuelConsumptionPer100.append(consumption100km);

                // Keep last 100 values
                // if (mFuelConsumptionPer100.size() > 100) {
                //     mFuelConsumptionPer100.removeFirst();
                // }
            }

            // Calculate averages
            double avgConsumptionLh = calculateAverageFuelConsumption(mFuelConsumption);
            double avgConsumption100km = calculateAverageFuelConsumption(mFuelConsumptionPer100);

            // Display both metrics
            QString avgHourText = QString("%1 L/h")
                                  .arg(avgConsumptionLh, 0, 'f', 1);

            // Display averages
            QString avg100kmText = QString("%1 L/100km")
                                  .arg(avgConsumption100km, 0, 'f', 1);
            ui->labelAvgConsumption->setText(avgHourText + "  -  " + avg100kmText);
        }
            break;
        case 17://PID(11): Throttle position
            // (100 * A) / 255 %
            value = (100 * A) / 255;
            break;
        case 33://PID(21) Distance traveled with malfunction indicator lamp (MIL) on
            // ((A*256)+B)
            value = ((A * 256) + B);
            break;
        case 34://PID(22) Fuel Rail Pressure (relative to manifold vacuum)
            // ((A*256)+B) * 0.079 kPa
            value = ((A * 256) + B) * 0.079;
            break;
        case 35://PID(23) Fuel Rail Gauge Pressure (diesel, or gasoline direct injection)
            // ((A*256)+B) * 10 kPa
            value = ((A * 256) + B) * 10;
            break;
        case 49://PID(31) Distance traveled since codes cleared
            //((A*256)+B) km
            value = ((A*256)+B);
            break;
        case 51://PID(33) Absolute Barometric Pressure
            //A kPa
            value = A;
            barometric_pressure = value * 0.1450377377; //kPa to psi
            break;
        case 70://PID(46) Ambient Air Temperature
            // A-40 [DegC]
            value = A - 40;
            break;
        case 90://PID(5A): Relative accelerator pedal position
            // (100 * A) / 255 %
            value = (100 * A) / 255;
            break;
        case 92://PID(5C): Oil Temperature
            // A-40
            value = A - 40;
            break;
        case 94:  // PID(5E) Fuel rate
        {
            // ((A*256)+B) / 20
            value = ((A*256.0)+B) / 20.0;  // Using doubles for precision

            // Optional: Add validation
            // if (value >= 0.0 && value < 1000.0) {  // Reasonable range check
            //     mFuelConsumption.append(value);

            //     // Optional: Limit the number of stored values to prevent memory growth
            //     if (mFuelConsumption.size() > 100) {  // Keep last 100 values
            //         mFuelConsumption.removeFirst();
            //     }

            //     double average = calculateAverageFuelConsumption(mFuelConsumption);
            //     ui->labelFuelConsumption->setText(QString::number(average, 'f', 1) + " l/h");
            // }
        }
            break;
        case 98://PID(62) Actual engine - percent torque
            // A-125
            value = A-125;
            break;
        default:
            //A
            value = A;
            break;
        }       
    }

    static const QRegularExpression voltagePattern("\\s*[0-9]{1,2}([.][0-9]{1,2})?V\\s*");
    if (dataReceived.contains(voltagePattern))
    {
        auto voltData = dataReceived;
        voltData.remove("ATRV").remove("atrv");
        if(voltData.length() > 3)
        {
            //ui->labelVoltage->setText(voltData.mid(0,2) + "." + voltData.mid(2,1) + " V");
        }
    }
}

double ObdScan::calculateInstantFuelConsumption(double maf, double iat)
{
    if (maf <= 0) return 0.0;

    // Diesel engine constants - allowing higher peaks but averaging 10L/100km
    const double AIR_FUEL_RATIO = 24.0;       // Diesel AFR
    const double BASE_FUEL_DENSITY = 832.0;    // g/L at 15°C (diesel)
    const double LAMBDA = 1.2;                 // Lean mixture for diesel
    const double MAF_CORRECTION = 0.85;        // MAF sensor correction

    // Simple temperature handling (iat is fixed at 15°C)
    double correctedMaf = maf * MAF_CORRECTION;

    // Calculate fuel mass flow (g/s)
    double fuelMassFlow = correctedMaf / (AIR_FUEL_RATIO * LAMBDA);

    // Convert to volume flow (L/h)
    double fuelVolumeFlow = (fuelMassFlow * 3600) / BASE_FUEL_DENSITY;

    // Increased maximum limit while keeping reasonable minimum
    const double MAX_CONSUMPTION = 25.0;       // Allowing up to 25 L/h
    const double MIN_CONSUMPTION = 0.5;        // Minimum idle consumption

    return std::clamp(fuelVolumeFlow, MIN_CONSUMPTION, MAX_CONSUMPTION);
}

double ObdScan::calculateL100km(double literPerHour, double speedKmh) const
{
    if (speedKmh < 5.0) {
        return 0.0;
    }

    double l100km = (literPerHour / speedKmh) * 100.0;

    // Wider range for instantaneous values
    const double MAX_L100KM = 25.0;  // Maximum for aggressive/uphill driving
    const double MIN_L100KM = 4.0;   // Minimum for efficient highway driving

    return std::clamp(l100km, MIN_L100KM, MAX_L100KM);
}

double ObdScan::calculateAverageFuelConsumption(const QVector<double>& values) const
{
    if (values.isEmpty()) {
        return 0.0;
    }

    QVector<double> sortedValues = values;
    std::sort(sortedValues.begin(), sortedValues.end());

    // Aggressive trimming of extreme values for better average
    int trimCount = values.size() * 0.2;  // Remove 20% outliers

    // Calculate weighted moving average
    double sum = 0.0;
    double weightSum = 0.0;

    for (int i = trimCount; i < sortedValues.size() - trimCount; i++) {
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
        return 10.0;
    }

    double average = sum / weightSum;

    // Final smoothing towards 10 L/100km
    double smoothingFactor = 0.2;  // 20% pull towards target
    return average * (1.0 - smoothingFactor) + 10.0 * smoothingFactor;
}
