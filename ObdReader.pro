QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    connectionmanager.cpp \
    elm.cpp \
    elmtcpsocket.cpp \
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

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#simulator https://github.com/Ircama/ELM327-emulator/releases
#python3 -m pip install ELM327-emulator
#C:\Users\MarenCompEng\AppData\Local\Packages\PythonSoftwareFoundation.Python.3.12_qbz5n2kfra8p0\LocalCache\local-packages\Python312\Scripts\elm.exe -n 35000 -s car
#C:\Users\MarenCompEng\AppData\Local\Packages\PythonSoftwareFoundation.Python.3.12_qbz5n2kfra8p0\LocalCache\local-packages\Python312\Scripts\elm.exe -p COM8 -s car
#elm -n 35000 -s car

contains(ANDROID_TARGET_ARCH,arm64-v8a) {
    ANDROID_PACKAGE_SOURCE_DIR = \
        $$PWD/android
}

RESOURCES += \
    resources.qrc
