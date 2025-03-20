#ifndef OBDSCAN_H
#define OBDSCAN_H

#include <QMainWindow>
#include <QTimer>
#include <QCloseEvent>
#include <QMutex>
#include <QVector>
#include <QDebug>
#include "elm.h"

namespace Ui {
class ObdScan;
}

class ObdScan : public QMainWindow
{
    Q_OBJECT

public:
    explicit ObdScan(QWidget *parent = nullptr);
    ~ObdScan() override;

private:
    // UI and styling methods
    void applyStyles();
    void setupInitialValues();
    void setupConnections();

    // Command management methods
    void removeCommand(const QString& commandToRemove);
    void addCommand(const QString& command);

    // Data processing methods
    double calculateInstantFuelConsumption(double maf, double iat = 15.0);
    double calculateL100km(double literPerHour, double speedKmh) const;
    double calculateAverageFuelConsumption(const QVector<double>& values) const;

    // Communication methods
    QString send(const QString& command);
    QString getData(const QString& command);
    void analysData(const QString& dataReceived);
    bool isError(std::string msg);

    // Queue management
    void startQueue();
    void stopQueue();

private slots:
    void onTimeout();
    void dataReceived(QString data);
    void onClearClicked();
    void onExitClicked();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    Ui::ObdScan* ui;

    // Communication and threading
    QMutex m_mutex;
    QTimer m_timer;
    bool m_running{true};
    int m_commandOrder{0};

    // Data storage
    QVector<double> m_fuelConsumption;
    QVector<double> m_fuelConsumptionPer100;

    // Vehicle state data
    double m_barometricPressure{0.0}; // PSI
    double m_airTemp{15.0};           // Celsius
    double m_speed{0.0};              // km/h

    // Constants
    static constexpr double DIESEL_AIR_FUEL_RATIO{24.0};  // For diesel engines
    static constexpr double DIESEL_FUEL_DENSITY{832.0};   // g/L at 15°C

    // Reference to ELM instance
    ELM* m_elm{ELM::getInstance()};
};

#endif // OBDSCAN_H
