#ifndef OBDSCAN_H
#define OBDSCAN_H

#include <QMainWindow>
#include <QTime>
#include <QMutex>
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

    void applyStyles();
    void setupInitialValues();
    void removeCommand(const QString& commandToRemove);

private:
    QMutex m_mutex{};

    int m_timerId{};
    float m_realTime{};
    QTimer m_timer{};
    int map{0};
    double barometric_pressure{0.0};
    double air_temp{0.0};

    bool mRunning{false};
    int commandOrder{0};

    ELM *elm{};

    QVector<double> mFuelConsumption;  // Store fuel consumption values
    double calculateInstantFuelConsumption(double maf, double iat = 15.0);
    double calculateAverageFuelConsumption(const QVector<double>& values) const;
    QVector<double> mFuelConsumptionPer100;
    double calculateL100km(double literPerHour, double speedKmh) const;
    const double AIR_FUEL_RATIO = 14.7; // For gasoline (stoichiometric ratio)
    const double FUEL_DENSITY = 745.0;  // Gasoline density in g/L at 15°C    
    double m_speed = 0.0;

    QString send(const QString &);
    QString getData(const QString &);
    bool isError(std::string);

    void analysData(const QString &);
    void startQueue();
    void stopQueue();
    void onTimeout();

public slots:
    void dataReceived(QString);

private slots:
    void onClearClicked();
    void onExitClicked();

protected:
    void closeEvent (QCloseEvent *) override;

private:
    Ui::ObdScan *ui;
};

#endif // OBDSCAN_H
