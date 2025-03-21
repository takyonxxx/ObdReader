QT       += core gui network
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
    elmtcpsocket.cpp \
    global.cpp \
    main.cpp \
    mainwindow.cpp \
    obdscan.cpp \
    settingsmanager.cpp

HEADERS += \
    connectionmanager.h \
    elm.h \
    elmtcpsocket.h \
    global.h \
    mainwindow.h \
    obdscan.h \
    settingsmanager.h

FORMS += \
    mainwindow.ui \
    obdscan.ui

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Android specific configuration
android {
    # Only build for one architecture at a time
    # Choose whichever you want to build
    # ANDROID_ABIS = armeabi-v7a
    # ANDROID_ABIS = arm64-v8a

    # Basic Android settings
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
}

# Simulator notes (kept as comments for reference)
# simulator https://github.com/Ircama/ELM327-emulator/releases
# python -m pip install ELM327-emulator
# elm -n 35000 -s car
# elm -p COM8 -s car
# elm -n 35000 -s car
