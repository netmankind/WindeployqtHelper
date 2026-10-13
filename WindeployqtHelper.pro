#-------------------------------------------------
# WindeployqtHelper.pro
# Qt5.15 / Qt6 双兼容，MinGW/MSVC
#-------------------------------------------------
QT += core gui widgets
CONFIG *= c++17
CONFIG *= resources_big
CONFIG -= debug_and_release
CONFIG *= lrelease



DEFINES *= QT_MESSAGELOGCONTEXT
DEFINES *= QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050F00

TEMPLATE = app
TARGET = WindeployqtHelper

include(config.pri)

# ===================== Qt版本判断 =====================
contains(QT_VERSION, ^5\..*) {
    QT_VER = qt5
}
contains(QT_VERSION, ^6\..*) {
    QT_VER = qt6
}

# ===================== 编译器判断 MinGW / MSVC =====================
win32-g++ {
    COMPILER = mingw
}
win32-msvc* {
    COMPILER = msvc
    QMAKE_CXXFLAGS += /utf-8
}

# ===================== 拼接输出目录 DESTDIR =====================
#DESTDIR = $$clean_path($$PWD/../WindeployqtHelper-bin)
#DESTDIR = $$clean_path($$PWD/../WindeployqtHelper-$${QT_VER}-$${COMPILER}-bin)
DESTDIR = $$clean_path($$PWD/../WindeployqtHelper-qt$${QT_VERSION}-$${COMPILER}-bin)
message("=====================================")
message("Qt: $${QT_VER}")
message("COMPILER: $${COMPILER}")
message("DESTDIR: $${DESTDIR}")
message("=====================================")

# 去掉多余控制台窗口（release模式）
CONFIG(debug, debug|release) {
    TARGET = WindeployqtHelper_d
} else {
    TARGET = WindeployqtHelper
    CONFIG += windows
}



SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

#RESOURCES += resources.qrc

win32 {
    #RC_FILE += resources/WindeployqtHelper.rc
}
