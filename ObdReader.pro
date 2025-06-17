QT       += core gui network bluetooth
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
TARGET = ObdReader
TEMPLATE = app
CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    connectionmanager.cpp \
    elm.cpp \
    elmbluetoothmanager.cpp \
    elmtcpsocket.cpp \
    global.cpp \
    main.cpp \
    mainwindow.cpp \
    settingsmanager.cpp

HEADERS += \
    connectionmanager.h \
    elm.h \
    elmbluetoothmanager.h \
    elmtcpsocket.h \
    global.h \
    mainwindow.h \
    settingsmanager.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Android-specific configuration
android {
    # Define app icons for different densities
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
    DISTFILES += \
        android/AndroidManifest.xml \
        android/build.gradle \
        android/res/values/libs.xml

    # Make sure these icon files exist in your android/res directory
    DISTFILES += \
        android/res/drawable-ldpi/ic_launcher.png \
        android/res/drawable-mdpi/ic_launcher.png \
        android/res/drawable-hdpi/ic_launcher.png \
        android/res/drawable-xhdpi/ic_launcher.png \
        android/res/drawable-xxhdpi/ic_launcher.png \
        android/res/drawable-xxxhdpi/ic_launcher.png \
        android/res/drawable/splash.xml

    # Add Bluetooth permissions
    DISTFILES += \
        android/src/org/qtproject/qt/android/bluetooth/QtBluetoothBroadcastReceiver.java
}

# Simulator notes (kept as comments for reference)
# simulator https://github.com/Ircama/ELM327-emulator/releases
# python -m pip install ELM327-emulator
# elm -n 35000 -s car
# elm -p COM8 -s car
# elm -n 35000 -s car
