#include "obdscan.h"
#include "ui_obdscan.h"
#include "global.h"
#include "connectionmanager.h"

ObdScan::ObdScan(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ObdScan)
{
    ui->setupUi(this);
    this->centralWidget()->setStyleSheet("background-color:#17202A ; border: none;");

    setWindowTitle("Elm327 Obd2");

    ui->labelRpmTitle->setStyleSheet("font-size: 24pt; font-weight: bold; color: #ECF0F1; padding: 3px; spacing: 3px;");
    ui->labelRpm->setStyleSheet("font-size: 24pt; font-weight: bold; color: #ECF0F1; background-color: #154360 ; padding: 3px; spacing: 3px;");
    ui->labelLoadTitle->setStyleSheet("font-size: 24pt; font-weight: bold; color: #ECF0F1; padding: 3px; spacing: 3px;");
    ui->labelLoad->setStyleSheet("font-size: 24pt; font-weight: bold; color: #ECF0F1; background-color: #154360 ; padding: 3px; spacing: 3px;");
    ui->labelMapTitle->setStyleSheet("font-size: 24pt; font-weight: bold; color: #ECF0F1; padding: 3px; spacing: 3px;");
    ui->labelMap->setStyleSheet("font-size: 24pt; font-weight: bold; color: #ECF0F1; background-color: #154360 ; padding: 3px; spacing: 3px;");
    ui->labelMafTitle->setStyleSheet("font-size: 24pt; font-weight: bold; color: #ECF0F1; padding: 3px; spacing: 3px;");
    ui->labelMaf->setStyleSheet("font-size: 24pt; font-weight: bold; color: #ECF0F1; background-color: #154360 ; padding: 3px; spacing: 3px;");
    ui->labelCoolantTitle->setStyleSheet("font-size: 24pt; font-weight: bold; color: #ECF0F1; padding: 3px; spacing: 3px;");
    ui->labelCoolant->setStyleSheet("font-size: 24pt; font-weight: bold; color: #ECF0F1; background-color: #154360 ; padding: 3px; spacing: 3px;");
    ui->labelFuelConsumption->setStyleSheet("font-size: 32pt; font-weight: bold; color: #ECF0F1; background-color: #154360 ; padding: 3px; spacing: 3px;");
    ui->labelAvgConsumption->setStyleSheet("font-size: 32pt; font-weight: bold; color: #ECF0F1; background-color: #154360 ; padding: 3px; spacing: 3px;");
    ui->labelVoltage->setStyleSheet("font-size: 32pt; font-weight: bold; color: #ECF0F1; background-color: #154360 ; padding: 3px; spacing: 3px;");
    ui->labelVoltage->setText(QString::number(0, 'f', 1) + " V");    
    ui->labelCommand->setStyleSheet("font-size: 32pt; font-weight: bold; color: #ECF0F1; background-color: #154360 ; padding: 3px; spacing: 3px;");

    ui->pushExit->setStyleSheet("font-size: 32pt; font-weight: bold; color: #ECF0F1; background-color: #512E5F; padding: 3px; spacing: 3px;");

    runtimeCommands.clear();

    if(runtimeCommands.isEmpty())
    {
        runtimeCommands.append(VOLTAGE);
        runtimeCommands.append(VEHICLE_SPEED);
        runtimeCommands.append(ENGINE_RPM);
        runtimeCommands.append(ENGINE_LOAD);
        runtimeCommands.append(COOLANT_TEMP);
        runtimeCommands.append(MAN_ABSOLUTE_PRESSURE);
        runtimeCommands.append(MAF_AIR_FLOW);
    }

     startQueue();
}

ObdScan::~ObdScan()
{
    delete ui;
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

void ObdScan::on_pushExit_clicked()
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
        ui->labelCommand->setText(runtimeCommands[commandOrder]);
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

void ObdScan::analysData(const QString &dataReceived)
{
    unsigned A = 0;
    unsigned B = 0;
    unsigned PID = 0;
    double value = 0;

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
            double value = A;

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
            ui->labelMap->setText(QString::number(value, 'f', 1) + " kPa" + " / " + boostValue);
        }
            break;
        case 12: //PID(0C): RPM
            //((A*256)+B)/4
            value = ((A * 256) + B) / 4;
            ui->labelRpm->setText(QString::number(value) + " rpm");
            break;
        case 13://PID(0D): KM Speed
            // A
            value = A;
            m_speed = value;
            //ui->labelSpeed->setText(QString::number(value) + " km/h");
            break;
        case 15://PID(0F): Intake Air Temperature
            // A - 40
            value = A - 40;
            break;
        case 16://PID(10): MAF air flow rate grams/sec
        {
            value = ((256*A)+B) / 100;  // MAF in g/s
            ui->labelMaf->setText(QString::number(value) + " g/s");

            // Calculate L/h
            double fuelConsumption = calculateInstantFuelConsumption(value, m_currentIat);
            mFuelConsumption.append(fuelConsumption);

            // Calculate L/100km
            double consumption100km = calculateL100km(fuelConsumption, m_speed);

            if (consumption100km > 0.0) {  // Only store valid readings
                mFuelConsumptionPer100.append(consumption100km);

                // Keep last 100 values
                if (mFuelConsumptionPer100.size() > 100) {
                    mFuelConsumptionPer100.removeFirst();
                }
            }

            // Calculate averages
            double avgConsumptionLh = calculateAverageFuelConsumption(mFuelConsumption);
            double avgConsumption100km = calculateAverageFuelConsumption(mFuelConsumptionPer100);

            // Display both metrics
            QString displayText;
            if (m_speed < 5.0) {
                displayText = QString("Idle: %1 L/h")
                                  .arg(fuelConsumption, 0, 'f', 1);
            } else {
                displayText = QString("%1 L/100km\n%2 L/h")
                                  .arg(consumption100km, 0, 'f', 1)
                                  .arg(fuelConsumption, 0, 'f', 1);
            }
            ui->labelFuelConsumption->setText(displayText);

            // Optional: Display averages
            QString avgText = QString("Avg: %1 L/100km")
                                  .arg(avgConsumption100km, 0, 'f', 1);
            ui->labelAvgConsumption->setText(avgText);
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
            double value = ((A*256.0)+B) / 20.0;  // Using doubles for precision

            // Optional: Add validation
            if (value >= 0.0 && value < 1000.0) {  // Reasonable range check
                mFuelConsumption.append(value);

                // Optional: Limit the number of stored values to prevent memory growth
                if (mFuelConsumption.size() > 100) {  // Keep last 100 values
                    mFuelConsumption.removeFirst();
                }

                double average = calculateAverageFuelConsumption(mFuelConsumption);
                ui->labelFuelConsumption->setText(QString::number(average, 'f', 1) + " l/h");
            }
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
            ui->labelVoltage->setText(voltData.mid(0,2) + "." + voltData.mid(2,1) + " V");
        }
    }
}

double ObdScan::calculateInstantFuelConsumption(double maf, double iat)
{
    if (maf <= 0) return 0.0;

    // Engine specific constants
    const double ENGINE_DISPLACEMENT = 2.7;    // Liters
    const double AIR_FUEL_RATIO = 18.0;       // Diesel ratio
    const double BASE_FUEL_DENSITY = 832.0;    // g/L at 15°C
    const double LAMBDA = 1.0;                // Actual lambda from sensor if available
    const double COMMON_RAIL_PRESSURE = 1600.0; // Bar (typical for diesel CR)

    // Air density correction for temperature
    const double STANDARD_TEMP = 293.15;      // 20°C in Kelvin
    double actualTemp = iat + 273.15;         // Convert to Kelvin
    double tempCorrection = STANDARD_TEMP / actualTemp;

    // Adjust MAF based on engine displacement and temperature
    double correctedMaf = maf * tempCorrection * (ENGINE_DISPLACEMENT / 2.0); // Normalized to 2.0L

    // Temperature compensation for fuel density
    double tempDiff = iat - 15.0;  // Difference from base temp of 15°C
    double actualFuelDensity = BASE_FUEL_DENSITY - (0.7 * tempDiff);

    // Calculate fuel mass flow with displacement factor
    double fuelMassFlow = correctedMaf / (AIR_FUEL_RATIO * LAMBDA);

    // Convert to volume flow (L/h)
    double fuelVolumeFlow = (fuelMassFlow * 3600) / actualFuelDensity;

    // Common rail pressure compensation (higher pressure = better atomization)
    double pressureCorrection = 1.0 + ((COMMON_RAIL_PRESSURE - 1500.0) / 10000.0);
    fuelVolumeFlow *= pressureCorrection;

    // Displacement-based limits
    const double MAX_CONSUMPTION = ENGINE_DISPLACEMENT * 13.0; // Approx max L/h per liter
    const double MIN_CONSUMPTION = ENGINE_DISPLACEMENT * 0.2;  // Approx min L/h per liter

    return std::clamp(fuelVolumeFlow, MIN_CONSUMPTION, MAX_CONSUMPTION);
}

double ObdScan::calculateAverageFuelConsumption(const QVector<double>& values) const
{
    if (values.isEmpty()) {
        return 0.0;
    }

    double sum = 0.0;
    for (const double& value : values) {
        sum += value;
    }

    return sum / values.size();
}

double ObdScan::calculateL100km(double literPerHour, double speedKmh) const
{
    if (speedKmh < 5.0) {  // Below 5 km/h consider it idle
        return 0.0;
    }

    // Formula: (L/h) / (km/h) * 100 = L/100km
    double l100km = (literPerHour / speedKmh) * 100.0;

    // Reasonable limits for diesel
    const double MAX_L100KM = 30.0;  // Maximum reasonable consumption
    const double MIN_L100KM = 3.0;   // Minimum reasonable consumption

    return std::clamp(l100km, MIN_L100KM, MAX_L100KM);
}

void ObdScan::setCurrentIat(double newCurrentIat)
{
    m_currentIat = newCurrentIat;
}
