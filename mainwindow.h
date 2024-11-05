#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScreen>
#include <QRegularExpression>
#include "connectionmanager.h"
#include "settingsmanager.h"
#include "elm.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void applyStyles();

private slots:
    void connected();
    void disconnected();
    void dataReceived(QString);
    void stateChanged(QString);
    void getPids();
    QString cleanData(const QString& input);

    void onConnectClicked();
    void onSendClicked();
    void onReadClicked();
    void onSetProtocolClicked();
    void onGetProtocolClicked();
    void onClearClicked();
    void onReadFaultClicked();
    void onClearFaultClicked();
    void onScanClicked();
    void onIntervalSliderChanged(int value);
    void onExitClicked();
    void onSearchPidsStateChanged(int state);

private:
    void saveSettings();
    QString send(const QString &);
    QString getData(const QString &);
    void analysData(const QString &);
    bool isError(std::string msg);
    void setupIntervalSlider();

    ConnectionManager *m_connectionManager{};
    SettingsManager *m_settingsManager{};
    ELM *elm{};

    int commandOrder{0};
    bool m_connected{false};
    bool m_initialized{false};
    bool m_reading{false};
    bool m_searchPidsEnable{false};
    QRect desktopRect;
    std::vector<uint32_t> cmds{};

    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
