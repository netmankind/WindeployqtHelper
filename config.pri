
# += 表示添加，不会去重，而 *= 是去重添加，存在则不添加

#编译模式
CONFIG(debug, debug|release):{
    FILE_POSTFIX = D
    DIR_COMPILEMODE = Debug
}
else:CONFIG(release, debug|release):{
    FILE_POSTFIX =
    DIR_COMPILEMODE = Release
}

#x86-linux
linux-g++ || linux-clang {
    contains(QT_ARCH, x86_64) {
        DIR_PLATFORM = Linux-x86_64
    } else { #i386
        DIR_PLATFORM = Linux-x86
    }

    #CONFIG(GCC, GCC|GCC32|GCC64):{#构建步骤里定义CONFIG+=GCC64
    if(contains(DEFINES,GCC)) {
        DIR_COMPILER = GCC
    }
    #else:CONFIG(GCC32, GCC|GCC32|GCC64):{
    if(contains(DEFINES,GCC32)) {
        DIR_COMPILER = GCC32
    }
    #else:CONFIG(GCC64, GCC|GCC32|GCC64):{
    if(contains(DEFINES,GCC64)) {
        DIR_COMPILER = GCC64
    }
}
#ARM-linux
my-linux-arm-qt || linux-arm-gnueabi-g++ {
    DIR_PLATFORM = Linux-arm
    DEFINES += BUILD_FOR_ARM
    target.path = /home/root
    INSTALLS += target
    #contains(DEFINES,AM437X)#构建步骤里定义DEFINES+=A5D3和DEFINES+=AM437X
    #CONFIG(AM437X, AM437X|A5D3):{ #构建步骤里定义CONFIG+=AM437X
    if(contains(DEFINES,AM437X)) {
        DIR_COMPILER = AM437X
    }
    #else:CONFIG(A5D3, AM437X|A5D3):{
    if(contains(DEFINES,A5D3)) {
        DIR_COMPILER = A5D3
    }
}

#Windows
win32 {
    contains(QT_ARCH, x86_64) {
        DIR_PLATFORM = Win64
    } else { #i386
        DIR_PLATFORM = Win32
    }
    if(contains(DEFINES,MinGW)) {
    #ONFIG(MinGW, MinGW|MSVC):{#构建步骤里定义CONFIG+=MinGW
        DIR_COMPILER = MinGW
    }
    #else:CONFIG(MSVC, MinGW|MSVC):{
    if(contains(DEFINES,MSVC)) {
        DIR_COMPILER = MSVC
    }
}

MOC_DIR     = $${DIR_PLATFORM}/$${DIR_COMPILEMODE}/moc
RCC_DIR     = $${DIR_PLATFORM}/$${DIR_COMPILEMODE}/rcc
UI_DIR      = $${DIR_PLATFORM}/$${DIR_COMPILEMODE}/ui
OBJECTS_DIR = $${DIR_PLATFORM}/$${DIR_COMPILEMODE}/obj

DEST_BIN  = $${DIR_PLATFORM}/$${DIR_COMPILEMODE}/bin
DIR_DEPEND_DEST = $${DIR_PLATFORM}/$${DIR_COMPILEMODE}
DEST_LIB = $${DEST_BIN}

DESTDIR = $${DEST_BIN}

if(contains(DEFINES, MSVC)) {
    #构建qmake额外参数加DEFINES+=MSVC
    QMAKE_CXXFLAGS *= /utf-8
    #QMAKE_LFLAGS += /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'" #以管理员运行
    #QMAKE_LFLAGS += /SUBSYSTEM:WINDOWS,"5.01" #VS2013 在XP运行
}

#target.path = /home/root/
#!isEmpty(target.path): INSTALLS += target
