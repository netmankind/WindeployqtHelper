# ============================================================
# config.pri —— 通用工程配置（qmake）
# 适用：Qt 5.15 / Qt 6.x，Windows / Linux / macOS / Android，MSVC / MinGW / GCC / Clang
#
# 用法：在主 .pro 中  include($$PWD/config.pri)
#   可选开关（需写在 include 之前）：
#     CONFIG += console_app       # 控制台程序（默认 GUI 程序，无控制台黑窗）
#     CONFIG += no_translations   # 不处理多语言（默认自动 lrelease）
#     CONFIG += keep_deprecated   # 不禁用 Qt 旧 API（默认禁用 5.15 之前的废弃 API）
#     CONFIG += strict_warnings   # 更严格的警告（MSVC /W4，GCC/Clang 额外 -Wextra）
#     CONFIG += no_warnings       # 不主动追加警告选项
#     CONFIG += static_runtime    # MSVC 静态链接 CRT（/MT、/MTd），CRT指C/C++运行时库，Qt 官方预编译包（ Qt6Core.dll、Qt5Widgets.dll等）是用 **`/MD`** 动态编译的，用 Qt 官方 DLL 时，保持默认 `/MD`，不要加 `static_runtime`
#     CONFIG += auto_deploy       # Windows 构建后自动运行 windeployqt
#     CONFIG += version_stamp     # 每次构建后删除版本目标文件以刷新版本号（需工程自带版本源文件）
#   其他变量：
#     VERSION_FILE_BASENAME = xxx  # 版本文件名（不含扩展名），默认 version
#     ANDROID_ABIS = arm64-v8a ... # Android 构建 ABI，默认 arm64-v8a
#     ANDROID_MIN_SDK_VERSION = 21      # Android 最低 SDK，默认 21
#     ANDROID_TARGET_SDK_VERSION = 33    # Android 目标 SDK，默认 33
# ============================================================

# ============================================================
# 一、通用 C++ 标准与 Qt 特性
# ============================================================
# C++17（Qt5 / Qt6 均支持；CONFIG *= 去重，避免重复追加）
CONFIG *= c++17
# 大资源 / 大 GIF 支持（资源超过一定体积时需要）
CONFIG *= resources_big
CONFIG *= gif
# 单配置构建：去掉空的 debug / release 目录
CONFIG -= debug_and_release
# 多语言：lrelease负责生成.qm文件，embed_translations负责把生成的.qm塞进可执行文件的资源系统（无 TRANSLATIONS 时不产生副作用）
CONFIG *= lrelease
#CONFIG *= embed_translations
# 预编译头支持（Qt6 qmake 不支持该特性，仅 Qt5 启用；具体头文件由工程 PRECOMPILED_HEADER 指定）
!greaterThan(QT_MAJOR_VERSION, 5): CONFIG *= precompile_header

# Release 下保留日志上下文（文件名、函数名、行号），便于定位问题
DEFINES *= QT_MESSAGELOGCONTEXT
# 使用 Qt 废弃 API 时发出警告
DEFINES *= QT_DEPRECATED_WARNINGS
# 默认禁用 Qt 5.15 之前的废弃 API（Qt5 / Qt6 均识别）
!CONFIG(keep_deprecated) {
    DEFINES *= QT_DISABLE_DEPRECATED_BEFORE=0x050F00
    # Qt6.5+ 专属宏，Qt5 不识别，条件添加
    greaterThan(QT_MAJOR_VERSION, 5): DEFINES *= QT_DISABLE_DEPRECATED_UP_TO=0x050F00
}

DEFINES *= QT_USE_QSTRINGBUILDER      # QString 的 a+b 走 QStringBuilder 优化拼接，官方推荐

CONFIG(strict_qt) {
    DEFINES *= QT_STRICT_ITERATORS    # 迭代器 const/可变混用报错，帮助抓隐患
}
# 需要代码纪律更强的可自行打开（会大量拒绝隐式转换/关键字冲突，老代码需清理）：
# QT_NO_KEYWORDS要转换signals->Q_SIGNALS,slots->Q_SLOTS,emit->Q_EMIT,foreach / forever-> C++11 range-for / Q_FOREACH
#DEFINES *= QT_NO_KEYWORDS
# QT_NO_CAST_FROM_ASCII，无法用char* → QString 的隐式转换，
# 如QString s = "abc";、func("abc")（形参 QString）->QStringLiteral("abc") / tr("abc") / QString::fromUtf8("abc"),
# qs == "abc"、ba == qs 等全部比较重载->qs == QLatin1String("abc") 或 QStringLiteral
# QT_NO_CAST_TO_ASCII —— 反方向：堵 QString → QByteArray
#DEFINES *= QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII
# 如果全量切换错误面太大，QT_RESTRICTED_CAST_FROM_ASCII 是折衷——只保留 QString(const char(&)[N])（按 fromUtf8 处理的字符串字面量），
# 删掉 char* 变量和 QByteArray 通道（L85-86）；注意它和 QT_NO_CAST_FROM_ASCII 互斥，同时定义直接 #error（L5）。
#DEFINES *= QT_RESTRICTED_CAST_FROM_ASCII

# ============================================================
# 二、编译器通用选项
# ============================================================
# ---------- MSVC ----------
win32-msvc* {
    # 源文件 / 执行字符集统一 UTF-8（否则中文在源码中易乱码）
    QMAKE_CXXFLAGS *= /utf-8
    # /MP：多进程并行编译，显著提升多文件工程编译速度
    QMAKE_CXXFLAGS *= /MP
    #让 MSVC 正确报告 __cplusplus 宏（否则永远报 199711L，影响大量现代库的版本探测）
    QMAKE_CXXFLAGS *= /Zc:__cplusplus
    # /permissive-：严格标准合规；老第三方头文件若报警可去掉这一行
    QMAKE_CXXFLAGS *= /permissive-
    # MSVC Release 默认不带符号，崩溃 dump 无法符号化。/Zi 生成独立 PDB 且不影响优化
    # 链接侧 /DEBUG 输出 PDB；注意开启 /DEBUG 后链接器默认变回 /OPT:NOREF /OPT:NOICF，要显式补回裁剪选项
    QMAKE_CXXFLAGS_RELEASE *= /Zi
    QMAKE_LFLAGS_RELEASE   *= /DEBUG /OPT:REF /OPT:ICF
    # 静态链接CRT默认动态链接（/MD、/MDd），CRT指C/C++运行时库（`vcruntime140.dll`、`msvcp140.dll` 等）
    CONFIG(static_runtime) {
        QMAKE_CXXFLAGS_DEBUG   *= /MTd
        QMAKE_CXXFLAGS_RELEASE *= /MT
    }
}
# ---------- MinGW / GCC / Clang ----------
win32-g++ | gcc | clang {
    # 统一源码字符集为 UTF-8
    QMAKE_CXXFLAGS *= -finput-charset=UTF-8 -fexec-charset=UTF-8
}
# -fexec-charset是 GCC 专属 flag（clang 直到很新的版本才支持）
# 如果clang版本较旧，改为下面的，gcc scope 覆盖 MinGW/所有 GCC；clang 不再被带进来
#gcc {
#    QMAKE_CXXFLAGS *= -finput-charset=UTF-8 -fexec-charset=UTF-8
#}


# ---------- 警告级别 ----------
!CONFIG(no_warnings) {
    win32-msvc* {
        QMAKE_CXXFLAGS *= /W3
        CONFIG(strict_warnings): QMAKE_CXXFLAGS *= /W4
    }
    win32-g++ | gcc | clang {
        QMAKE_CXXFLAGS *= -Wall
        CONFIG(strict_warnings): QMAKE_CXXFLAGS *= -Wextra
    }
}

# ============================================================
# 三、Qt6 MSVC qmake 系统库补全
# ============================================================
# Qt6 的 qmake 不会把 mkspecs 里 QMAKE_LIBS_GUI 的系统库自动加到链接行，
# 而 Qt5 qmake 会自动加。这里仅对 Qt6 + MSVC 补齐，避免 LNK2019
# （SetWindowPos、GetWindowLongPtrW 等均位于 user32）
win32-msvc*:greaterThan(QT_MAJOR_VERSION, 5) {
    LIBS *= gdi32.lib comdlg32.lib oleaut32.lib imm32.lib winmm.lib \
            ws2_32.lib ole32.lib uuid.lib user32.lib advapi32.lib
}
#MinGW 构建的 exe 默认依赖 libgcc_s_seh-1.dll、libstdc++-6.dll，windeployqt 会多拷两个 DLL；链接期直接静态进来更省事
#但 **Qt6Core.dll/ Qt6Gui.dll 等仍依赖**
win32-g++ {
    QMAKE_LFLAGS *= -static-libgcc -static-libstdc++
}

# ============================================================
# 四、打印系统信息
# ============================================================
message("Computer system OS:"$$QMAKE_HOST.os)
message("CPU cores:"$$QMAKE_HOST.cpu_count)
message("Qt Version="$$[QT_VERSION])
message("QT_ARCH "$$QT_ARCH)

# ============================================================
# 五、获取工程名
# 【关键】在“被 include 的 .pri”里，$$PWD 指向的是 .pri 自己所在的目录，
# 而不是工程的 .pro 所在目录。config.pri 一旦被放到共享目录（如 QtCommonConfig）
# 再被各工程 include，用 $$PWD 推导工程名/输出目录会全部落到共享目录上。
# 因此这里统一使用 qmake 内置变量：
#   $$_PRO_FILE_      = 当前正在被构建的 .pro 的完整路径（最准确）
#   $$_PRO_FILE_PWD_  = 当前正在被构建的 .pro 所在目录
# 且不再用“取目录里第一个 .pro”的兜底（多个 .pro 时会按字母序选错工程）。
# ============================================================
PROJECT_DIR = $$_PRO_FILE_PWD_                                # 工程目录（.pro 所在目录，非 .pri 目录）
isEmpty(PROJECT_DIR): PROJECT_DIR = $$PWD                     # 兜底：极少数环境无该变量时退回 PWD

PROJECT_FILE = $$_PRO_FILE_                                   # 正在构建的 .pro 完整路径
isEmpty(PROJECT_FILE) {
    # 兜底：理论上不应发生；退回“工程目录名.pro”
    DIR_NAME     = $$basename(PROJECT_DIR)
    PROJECT_FILE = $$PROJECT_DIR/$${DIR_NAME}.pro
}

PRO_BASE     = $$basename(PROJECT_FILE)                       # 例如 SpringLeafTester.pro
MAIN_PROJECT = $$replace(PRO_BASE, \.pro$|\.PRO$|\.Pro$, "")  # 去扩展名（大小写都兼容）

!isEmpty(MAIN_PROJECT) {
    PROJECT_NAME = $$MAIN_PROJECT
} else {
    # 兜底：实在取不到 .pro 名，就用工程目录名
    DIR_NAME = $$basename(PROJECT_DIR)
    PROJECT_NAME = $$DIR_NAME
}
message("PROJECT_FILE = $$PROJECT_FILE")
message("PROJECT_DIR  = $$PROJECT_DIR")
message("PROJECT_NAME = $$PROJECT_NAME")

isEmpty(PRODUCT_NAME) {
    PRODUCT_NAME = $$PROJECT_NAME
}

# ============================================================
# 六、TARGET 默认命名（工程未显式指定 TARGET 时）
#   Release: <工程名>   Debug: <工程名>_d
# 说明：不能再拿 TARGET 反过来当工程名兜底——此处 TARGET 尚未确定，
# 用它兜底会形成“空赋值空”的死循环，最终 TARGET/DESTDIR 都变空。
# ============================================================
isEmpty(TARGET) {
    TARGET = $$PROJECT_NAME
    CONFIG(debug, debug|release): TARGET = $${TARGET}_d
}
isEmpty(TARGET) {
    message(警告: 未能确定工程名，TARGET 为空，请在 .pro 中显式设置 TARGET)
}

# ============================================================
# 七、操作系统位数与编译器标识
# ============================================================
contains(QT_ARCH, x86_64) | contains(QT_ARCH, arm64) | contains(QT_ARCH, aarch64) | \
contains(QT_ARCH, riscv64) | contains(QT_ARCH, mips64) | contains(QT_ARCH, arm64-v8a) {
    BITS = 64
} else: contains(QT_ARCH, i.86) | contains(QT_ARCH, x86) | contains(QT_ARCH, arm) | \
        contains(QT_ARCH, mips) | contains(QT_ARCH, riscv32) | contains(QT_ARCH, armeabi-v7a) {
    BITS = 32
} else {
    BITS =
    message(警告: 未识别的架构 $$QT_ARCH)
}

gcc {
    DIR_COMPILER = GCC$$BITS
} else: msvc {
    DIR_COMPILER = MSVC$$BITS
} else: clang {
    DIR_COMPILER = CLANG$$BITS
} else {
    DIR_COMPILER = UNKNOWN
    message(警告: 未识别的编译器 QMAKE_XSPEC = $$QMAKE_XSPEC)
}
message(DIR_COMPILER = $$DIR_COMPILER)

# ============================================================
# 八、平台检测
# ============================================================
# ---------- Windows ----------
win32 {
    contains(QT_ARCH, x86_64) {
        DIR_PLATFORM = Win64
    } else {
        DIR_PLATFORM = Win32
    }
    win32-g++ {
        DIR_COMPILER = MinGW$$BITS
        TOOLCHAIN_TAG = mingw
    }
    win32-msvc* {
        DIR_COMPILER = MSVC$$BITS
        TOOLCHAIN_TAG = msvc
    }
}
# ---------- x86 Linux ----------
linux-g++ | linux-clang {
    contains(QT_ARCH, x86_64) {
        DIR_PLATFORM = Linux-x86_64
    } else {
        DIR_PLATFORM = Linux-x86
    }
    DIR_COMPILER = GCC$$BITS
    TOOLCHAIN_TAG = gcc
}
# ---------- ARM Linux ----------
my-linux-arm-qt | linux-arm-gnueabi-g++ {
    DIR_PLATFORM = Linux-arm
    DEFINES *= BUILD_FOR_ARM
    target.path = /home/root
    INSTALLS *= target
    # 构建步骤里用 DEFINES += AM437X / A5D3 区分板卡
    contains(DEFINES, AM437X): DIR_COMPILER = AM437X
    contains(DEFINES, A5D3):   DIR_COMPILER = A5D3
}
# ---------- macOS ----------
macx {
    DIR_PLATFORM = MacOS
    contains(QT_ARCH, arm64): DIR_PLATFORM = MacOS-arm64
}
# ---------- Android（构建主机可为 Windows / Linux，目标为 Android） ----------
# qmake 中用 android scope 命中；NDK 工具链已由 Qt mkspecs（android-clang）配置好，
# 这里只补充平台标识与打包常用变量默认值，工程可在 include 之前覆盖。
android {
    DIR_PLATFORM  = Android
    # 覆盖第七节识别出的，明确为 Android NDK 工具链
    DIR_COMPILER  = Android
    TOOLCHAIN_TAG = android

    # 默认构建 ABI：arm64-v8a（现代设备主流）
    # 工程可在 include 前覆盖为多 ABI：ANDROID_ABIS = arm64-v8a armeabi-v7a x86_64
    isEmpty(ANDROID_ABIS): ANDROID_ABIS = arm64-v8a

    # 默认 SDK 版本（工程可覆盖）
    isEmpty(ANDROID_MIN_SDK_VERSION):    ANDROID_MIN_SDK_VERSION = 21
    isEmpty(ANDROID_TARGET_SDK_VERSION): ANDROID_TARGET_SDK_VERSION = 33

    #qmake 有专门的 Android 版本变量，写进 Manifest，系统设置里能看到人类可读版本
    isEmpty(ANDROID_VERSION_NAME): ANDROID_VERSION_NAME = $$VERSION
    isEmpty(ANDROID_VERSION_CODE): ANDROID_VERSION_CODE = 1
    # 自带打包模板目录时：
    # ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
}

# ============================================================
# 九、编译模式
# ============================================================
CONFIG(debug, debug|release) {
    FILE_POSTFIX = D
    DIR_COMPILEMODE = Debug
} else: CONFIG(release, debug|release) {
    FILE_POSTFIX =
    DIR_COMPILEMODE = Release
}

# ============================================================
# 十、应用类型（GUI / 控制台）
# 仅对 TEMPLATE=app 生效；库工程不受影响
# ============================================================
equals(TEMPLATE, app) {
    CONFIG(console_app) {
        # 控制台程序：保留控制台输出
        win32: CONFIG *= console
    } else {
        # GUI 程序：关闭控制台黑窗，使用 Windows 子系统
        CONFIG -= console
        win32: CONFIG *= windows
    }
}

# ============================================================
# 十一、中间文件目录（moc / rcc / ui / obj）
# ============================================================
android {
    MOC_DIR      = $${QT_ARCH}/$${DIR_COMPILEMODE}/moc
    RCC_DIR      = $${QT_ARCH}/$${DIR_COMPILEMODE}/rcc
    UI_DIR       = $${QT_ARCH}/$${DIR_COMPILEMODE}/ui
    OBJECTS_DIR  = $${QT_ARCH}/$${DIR_COMPILEMODE}/obj
    QMLCACHE_DIR = $${QT_ARCH}/$${DIR_COMPILEMODE}/qmlcache
} else {
    MOC_DIR      = $${DIR_COMPILEMODE}/moc
    RCC_DIR      = $${DIR_COMPILEMODE}/rcc
    UI_DIR       = $${DIR_COMPILEMODE}/ui
    OBJECTS_DIR  = $${DIR_COMPILEMODE}/obj
    QMLCACHE_DIR = $${DIR_COMPILEMODE}/qmlcache
}
# ============================================================
# 十二、输出目录（工程上层目录的影子构建产物目录）
# 用 PROJECT_DIR（.pro 所在目录）而不是 $$PWD：
# config.pri 被放到共享目录时 $$PWD 会指向共享目录，产物会输出到那里去。
# ============================================================
!android {
    DEST_LIB = $$clean_path($$PROJECT_DIR/../$${PROJECT_NAME}-qt$$[QT_VERSION]-$${DIR_COMPILER}-bin)
    DESTDIR  = $$clean_path($$PROJECT_DIR/../$${PROJECT_NAME}-qt$$[QT_VERSION]-$${DIR_COMPILER}-bin)
}
# ============================================================
# 十三、多语言 qm 输出目录
# 必须放在 DESTDIR 定义之后
# ============================================================
!CONFIG(no_translations) {
    !android {
        LRELEASE_DIR = $$DESTDIR/translations
        !exists($$LRELEASE_DIR): mkpath($$LRELEASE_DIR)
    } else {
        #LRELEASE_DIR = $${QT_ARCH}/translations
    }
}

# ============================================================
# 十四、可选：自动部署 windeployqt（Windows）
# 启用 CONFIG += auto_deploy
# ============================================================
win32:CONFIG(auto_deploy) {
    WINDEPLOYQT = $$shell_quote($$shell_path($$[QT_INSTALL_BINS]/windeployqt.exe))
    QMAKE_POST_LINK += $$WINDEPLOYQT $$shell_quote($$shell_path($$DESTDIR/$$TARGET.exe)) $$escape_expand(\\n\\t)
}

# ============================================================
# 十五、可选：版本号每次构建刷新
# 启用 CONFIG += version_stamp（需工程自带版本源文件，编译后删除其目标文件强制重编）
# 版本文件名（不含扩展名）默认 version，其他工程可覆盖：
#   VERSION_FILE_BASENAME = appversion
# ============================================================
CONFIG(version_stamp) {
    isEmpty(VERSION_FILE_BASENAME): VERSION_FILE_BASENAME = version
    VERSION_OBJ = $$OBJECTS_DIR/$${VERSION_FILE_BASENAME}.*
    win32 {
        QMAKE_POST_LINK += $$QMAKE_DEL_FILE /q /f $$shell_quote($$system_path($$VERSION_OBJ)) || exit 0 $$escape_expand(\\n\\t)
    } else {
        QMAKE_POST_LINK += $$QMAKE_DEL_FILE $$system_path($$VERSION_OBJ) || exit 0 $$escape_expand(\\n\\t)
    }
}

# ============================================================
# 十六、可选：git 哈希注入版本，非 git 工程自动回退，不报错
# 代码里 qDebug() << APP_BUILD_GIT 即可打出构建来源。
# .git 不在 config.pri 所在目录及其祖先时，工程在 include 前覆盖：GIT_REPO_DIR = $$PWD/../..
# ============================================================
isEmpty(GIT_REPO_DIR): GIT_REPO_DIR = $$PROJECT_DIR

equals(QMAKE_HOST.os, Windows): GIT_SILENT = 2>nul
else: GIT_SILENT = 2>/dev/null           # 灭掉 fatal / "git 不是内部或外部命令"

GIT_HASH = $$system(git -C $$system_path($$GIT_REPO_DIR) rev-parse --short HEAD $$GIT_SILENT)
isEmpty(GIT_HASH): GIT_HASH = dev         # 非 git 工程 / git 不在 PATH → 统一回退值

DEFINES    *= APP_BUILD_GIT=\\\"$$GIT_HASH\\\"   # C++ 里：QString hash = APP_BUILD_GIT;
QMAKE_RCFLAGS *= /DAPP_BUILD_GIT=\\\"$$GIT_HASH\\\"   # 顺带传给 rc.exe

# ============================================================
# 十七、Windows 版本信息与图标（qmake 自动生成 .rc 并链接）
# ============================================================
win32 {
    QMAKE_RC *= -c 65001    #详细信息里正确显示中文
    isEmpty(RC_FILE): {
        #用 RC_FILE 后，QMAKE_TARGET_* / RC_ICONS / VERSION 等变量失效
        isEmpty(VERSION): VERSION = 1.0.0.0    # major.minor.patch.build
        isEmpty(QMAKE_TARGET_COMPANY):     QMAKE_TARGET_COMPANY     = "中地装（重庆）地质装备有限公司"
        isEmpty(QMAKE_TARGET_PRODUCT):     QMAKE_TARGET_PRODUCT     = $$PRODUCT_NAME
        isEmpty(QMAKE_TARGET_DESCRIPTION): QMAKE_TARGET_DESCRIPTION = $$PRODUCT_NAME
        isEmpty(QMAKE_TARGET_COPYRIGHT):   QMAKE_TARGET_COPYRIGHT   = "Copyright 2026 中地装（重庆）地质装备有限公司"
        RC_LANG      = 0x0804                    # 简体中文；纯英文项目改 0x0409
        RC_CODEPAGE = 1200 #Unicode
        #RC_CODEPAGE  = 936                       # 字符串含中文时用 GBK；.pro 存 UTF-8 且含中文需自查乱码
        #RC_LANG = 0x0004   #中文（不区分简繁）
    }
} else {
    VERSION = 1.0.0    # major.minor.patch
}

# ============================================================
# 十八、MSVC .exp / .lib 导出残留处理
# .exp 是链接器中间导出文件；.lib 在存在导出符号时生成。
# 常见成因：工程把“库风格”第三方源码直接编进 exe，却定义了它的
# *_LIBRARY 导出宏（本工程 FluentUIStyle.pri 的
# FLUENTUI3STYLE_LIBRARY / EXWIDGETS_LIBRARY → Q_DECL_EXPORT），
# 于是 exe 导出一堆无用符号，链接后残留 .exp 与 .lib。
#   1) 源头：非库工程去掉 *_LIBRARY 等导出宏；但不可简单换成
#      Q_DECL_IMPORT——库类方法在 .cpp 中定义会触发 MSVC C2491。
#   2) 本段兜底：/NOEXP、/NOIMPLIB 从源头禁止生成；
#      删除命令保留，用于清理历史残留并纳入 clean 目标。
#      .lib 仅对“非库工程”处理——DLL/静态库工程的 .lib
#      是导入库/库本体，必须保留（不加 /NOIMPLIB、不删）。
# ============================================================
win32-msvc* {
    # 链接器开关：/NOEXP 禁 .exp（任何工程都无用）；
    # /NOIMPLIB 禁导入库 .lib（实测：带 dllexport 的 DLL 加
    # /NOIMPLIB /NOEXP 后只生成 .dll，无 .lib/.exp）。
    # 仅非库工程加 /NOIMPLIB——DLL/静态库的 .lib 是导入库/库本体，
    # 加了这个选项依赖方将无法链接。
    QMAKE_LFLAGS *= /NOEXP
    !contains(TEMPLATE, lib): QMAKE_LFLAGS *= /NOIMPLIB
    QMAKE_CLEAN += $$shell_path($$OUT_PWD)\*.exp
    !isEmpty(DESTDIR): QMAKE_CLEAN += $$shell_path($$DESTDIR)\*.exp
    QMAKE_POST_LINK += $$QMAKE_DEL_FILE /q /f $$shell_quote($$shell_path($$OUT_PWD)\*.exp) $$shell_quote($$shell_path($$DESTDIR)\*.exp) || exit 0 $$escape_expand(\\n\\t)

    # .lib：仅非库工程（TEMPLATE != lib）删除；库工程的导入库必须保留
    !contains(TEMPLATE, lib) {
        QMAKE_CLEAN += $$shell_path($$OUT_PWD)\*.lib
        !isEmpty(DESTDIR): QMAKE_CLEAN += $$shell_path($$DESTDIR)\*.lib
        QMAKE_POST_LINK += $$QMAKE_DEL_FILE /q /f $$shell_quote($$shell_path($$OUT_PWD)\*.lib) $$shell_quote($$shell_path($$DESTDIR)\*.lib) || exit 0 $$escape_expand(\\n\\t)
    }
}
