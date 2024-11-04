#include "mainwindow.h"
#include <QtWidgets/QStyleFactory>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setStyle(QStyleFactory::create("Fusion"));    // fusion look & feel of controls
    QPalette p (QColor(47, 47, 47));
    a.setPalette(p);

    // Setup the software details used by the settings system
    a.setOrganizationName("Türkay Biliyor");
    a.setOrganizationDomain("www.turkaybiliyor.com");
    a.setApplicationName("Obd Reader");

    MainWindow w;
    w.show();
    return a.exec();
}
