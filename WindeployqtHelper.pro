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

# ===================== 版本号 =====================
# 版本信息集中在 version.h / version.cpp；version_stamp 必须在 include config.pri 之前
# 打开——该开关的判断发生在 config.pri 被解析时，放在后面打开不会生效。
# 作用：每次构建结束删除 version.o，迫使下次构建重编该文件，
# 使 __DATE__/__TIME__ 始终等于本次编译时间（版本号随编译自动更新）
CONFIG *= version_stamp

include(QtCommonConfig/config.pri)

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
#DESTDIR = $$clean_path($$PWD/../WindeployqtHelper-qt$${QT_VERSION}-$${COMPILER}-bin)
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
    mainwindow.cpp \
    version.cpp

HEADERS += \
    mainwindow.h \
    version.h

FORMS += \
    mainwindow.ui

#RESOURCES += resources.qrc

win32 {
    #RC_FILE += resources/WindeployqtHelper.rc
}
