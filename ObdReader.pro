QT += core gui network bluetooth
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Qt 6.9 specific modules
QT += opengl

# Android specific modules (Qt 6.9 doesn't use androidextras)
android {
    QT += core-private
    LIBS += -lGLESv2
    LIBS += -llog
    LIBS += -landroid
}

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
    # Qt 6.9 Android configuration
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

    # Android API level
    ANDROID_MIN_SDK_VERSION = 24
    ANDROID_TARGET_SDK_VERSION = 34

    # Android permissions in AndroidManifest.xml
    DISTFILES += \
        android/AndroidManifest.xml \
        android/build.gradle \
        android/res/values/libs.xml

    # App icons for different densities
    DISTFILES += \
        android/res/drawable-ldpi/ic_launcher.png \
        android/res/drawable-mdpi/ic_launcher.png \
        android/res/drawable-hdpi/ic_launcher.png \
        android/res/drawable-xhdpi/ic_launcher.png \
        android/res/drawable-xxhdpi/ic_launcher.png \
        android/res/drawable-xxxhdpi/ic_launcher.png \
        android/res/drawable/splash.xml

    # Bluetooth and location permissions required for BLE scanning
    DISTFILES += \
        android/src/org/qtproject/qt/android/bluetooth/QtBluetoothBroadcastReceiver.java

    # Additional Android libraries
    LIBS += -ljnigraphics

    # Define application metadata
    DEFINES += ANDROID_PACKAGE_NAME=\\\"com.wjdiagnostics.obdreader\\\"
}

macos {
    message("macx enabled")
    QMAKE_INFO_PLIST = ./macos/Info.plist
}

# Windows specific configuration
win32 {
    # Windows libraries for system information
    LIBS += -lkernel32 -luser32

    # Windows version compatibility
    DEFINES += WINVER=0x0601 _WIN32_WINNT=0x0601
}

# Linux specific configuration
linux:!android {
    # Linux system libraries
    LIBS += -lpthread
}

# Compiler specific settings
gcc {
    QMAKE_CXXFLAGS += -Wno-deprecated-declarations
}

msvc {
    QMAKE_CXXFLAGS += /wd4996
}

# Debug/Release specific settings
CONFIG(debug, debug|release) {
    DEFINES += DEBUG_BUILD
    TARGET = $${TARGET}_debug
}

CONFIG(release, debug|release) {
    DEFINES += RELEASE_BUILD
    QMAKE_CXXFLAGS_RELEASE += -O2
}

# Simulator notes (kept as comments for reference)
# simulator https://github.com/Ircama/ELM327-emulator/releases
# python -m pip install ELM327-emulator
# elm -n 35000 -s car
# elm -p COM8 -s car
# elm -n 35000 -s car
