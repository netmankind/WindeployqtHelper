# WindeployqtHelper

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Qt 应用一键部署打包助手（GUI 工具）：图形化封装 `windeployqt` 部署流程，并可追加 **Enigma Virtual Box** 单文件便携打包，支持 **Qt 5.15 / Qt 6** 双版本、**MinGW / MSVC** 双编译器。

## 功能特性

- **图形化配置**：目标 EXE、Qt Bin 目录、输出目录、QML 目录、Enigma 控制台路径、VS 安装根目录，一键开始打包。
- **输出目录跟随目标 EXE**：选定或填入目标 EXE 后，输出目录自动填成该 EXE 所在目录（手动输入路径同样生效），便于就地打包。
- **就地打包**：输出目录与目标 EXE 目录相同时，跳过删除/复制主程序，跳过「清空旧目录」，不会误删目录内其它 DLL；目标 EXE 正是正在运行的本程序时，日志会提示运行中的 exe/DLL 被占用、同名文件不会被覆盖。
- **文件占用自愈**：启动 windeployqt 前先把输出目录里的运行库一次性改名让位，避免它去删正在运行的程序所加载的 DLL；仍报 `Cannot remove existing file ...` 时，再把被占用文件在同目录改名 `*.wdh-locked` 后重跑（最多重试 2 次），无需手动关程序。
- **选择对话框跟随输入框**：文件/目录选择器自动定位到当前输入框已填的路径（无效才回退到用户主目录）。
- **打包前 Qt 版本校验**：点「开始打包」后、动任何文件之前，先核对所选 Qt 与目标 EXE 是否配套——Qt 主版本（Qt5/Qt6）、位数（32/64）、编译器（MinGW/MSVC）三项，判定依据是目标 EXE 的 **PE 导入表**里真实依赖的 `Qt5Core.dll`/`Qt6Core.dll`、`libgcc*`/`libwinpthread-1.dll`、`vcruntime140.dll` 等。不匹配直接报错中止、不产生任何文件改动；判不出的情况（静态链接 Qt 或运行库）只提示不阻断（详见[技术要点](#打包前-qt-版本校验)）。
- **MinGW 运行库来源固定到所选 Qt**：windeployqt 是按 PATH 里的 `g++.exe` 定位运行库来源的，Qt kit 的 bin 没有 g++ 时会命中系统里其它 MinGW（如 Strawberry Perl 自带的 2024 版工具链），版本对不上就会触发「先删后拷」。工具在部署前先让位被占用的运行库、把配套工具链前置 PATH，部署后再用**所选 Qt bin 目录自带的运行库**强制覆盖——哪个 Qt 版本就用哪个的 DLL（详见[技术要点](#mingw-运行库来源校正)）。
- **复制 VC 运行库**：MSVC 部署时随包带上 `vcruntime140*.dll`、`msvcp140*.dll`、`concrt140.dll`。源优先取 Visual Studio Redist，按 Qt Bin 里的 `msvc<年份>` 选年份最接近的版本；无 VS Redist 时回退 `C:/Windows/System32`（详见[技术要点](#vc-运行库来源选择)）。
- **安全保护**：
  - 输出目录 = 源 EXE 所在目录时，跳过删除/复制，避免误删原主程序；
  - 清空旧目录时强制保留源 EXE，并支持自定义保留规则（通配符）。
- **多 QML 目录**：QML 目录可用英文分号分隔填多个，逐个传给 `windeployqt --qmldir`；「选择」按钮为追加而非覆盖，同一目录自动去重。
- **windeployqt 参数支持**：`--multimedia`、`--qmldir`（可多次）、`--no-plugins`（精简插件）、`--compiler-runtime`（复制运行库）。
- **Enigma 单文件打包**：
  - 自动生成 EVB 11.30 所需的 **XML** 工程文件（`.evb`），递归打包输出目录全部文件；
  - 单文件命名：`<目标EXE基名>_Single-qt<版本>-<编译器><位数>.exe`（如 `App_Single-qt6.8.3-MinGW64.exe`、`App_Single-qt5.15.2-MSVC64.exe`），版本与编译器取自 `qmake -v` 识别结果，位数取自目标 EXE 的 PE 头（识别失败则不加位数），**输出到输出目录内**；
  - 打包时自动跳过已存在的单文件（`*_Single*.exe`）、临时工程 `enigma_temp.evb` 与占用备份 `*.wdh-locked`，防止旧产物被重复打入；
  - 自动跳过运行时数据目录 `data`（历史记录 db / 导出文件等由程序动态增删的文件），避免 Enigma 报 `File does not exist`；运行时数据打进单文件本就无益（程序写入不回写 Single）；
  - 封包走异步进程，封包期间界面保持可响应。
- **配置持久化**：`config.ini` 保存/加载全部配置。
- **日志面板**：输出 windeployqt / Enigma 完整过程，可保存为 txt。

> 注：界面中「打包后压缩」「自定义保留插件」「版本号」「附加DLL」为已放置的预留选项（随配置保存），尚未接入打包流程。其中压缩由 EVB 工程固定写入 `<CompressFiles>True</CompressFiles>`，勾不勾该开关结果一致。

## 环境要求

| 依赖 | 说明 |
| --- | --- |
| Qt | 5.15 或 6.x（qmake 构建），C++17 |
| 编译器 | MinGW（win32-g++）或 MSVC（win32-msvc*） |
| windeployqt | 随 Qt 自带，位于所选 Qt Bin 目录 |
| Enigma Virtual Box | 11.30+（可选，勾选「启用Enigma单文件打包」时需要其 `enigmavbconsole.exe`） |
| Visual Studio | 可选，仅 MSVC 部署复制 VC 运行库时用于定位 Redist；未安装则从 System32 取 |

## 编译

### Qt Creator

直接用 Qt Creator 打开 `WindeployqtHelper.pro`，选择对应 Kit（Qt 5.15 或 Qt 6，MinGW 或 MSVC）构建即可。

### 命令行（以 Qt 6.8.3 MinGW 为例）

```bat
cd /d F:\WORK\QT_TEST\WindeployqtHelper\WindeployqtHelper
E:\Qt\6.8.3\mingw_64\bin\qmake.exe WindeployqtHelper.pro
E:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe -j8
```

输出目录由 `QtCommonConfig/config.pri` 统一拼接：

```
DESTDIR = $$clean_path($$PROJECT_DIR/../$${PROJECT_NAME}-qt$$[QT_VERSION]-$${DIR_COMPILER}-bin)
```

`DIR_COMPILER` 按 `QT_ARCH` 判定位数后取 `MinGW64` / `MSVC64`，即生成到工程目录的**上一级**，例如：

- `WindeployqtHelper-qt6.8.3-MinGW64-bin\WindeployqtHelper.exe`
- `WindeployqtHelper-qt5.15.2-MSVC64-bin\WindeployqtHelper.exe`

> 重新编译前先关掉正在运行的旧实例：Windows 不允许覆盖正在运行的 exe，否则 MSVC 会报 `LNK1104: 无法打开文件 ...WindeployqtHelper.exe`。

### 版本号

版本号形如 `1.0.0.<编译日期>-<编译时间>`，例如 `1.0.0.20261109-1303`，窗口标题显示为 `Qt 部署助手 v1.0.0.20261109-1303`。日期时间取自编译期的 `__DATE__` / `__TIME__`，`.pro` 中 `CONFIG *= version_stamp` 会在每次构建结束删除 `version.o`，使版本号随每次编译自动更新。

## 使用方法

1. **目标EXE**：选择待部署的 Qt 程序（Release 版）。输出目录会自动跟随到该 EXE 所在目录。
2. **Qt Bin目录**：选择与目标 EXE 相同 Qt 版本/编译器的 `bin` 目录（需含 `windeployqt.exe`），例如 `E:\Qt\6.8.3\mingw_64\bin`。
3. **输出目录**：部署产物输出位置；不填 Qt 相关依赖时也可直接作为单文件打包源目录。与目标 EXE 同目录即为就地打包。
4. **QML目录**（可选）：目标程序含 QML 时填写，多个目录用英文分号分隔，例如 `D:/src/qml;D:/src/views`。
5. **Enigma控制台**（可选）：勾选「启用Enigma单文件打包」后必填 `enigmavbconsole.exe` 路径。
6. **VS安装目录**（可选）：复制 VC 运行库时用于定位 Redist；首次默认 `C:/Program Files/Microsoft Visual Studio`，VS 装在其它盘时改一次并保存即可。
7. 点击 **开始打包**，流程：
   - **校验所选 Qt 与目标 EXE 是否配套**（不匹配直接中止，不改动任何文件）→ 复制主程序到输出目录（同目录时自动跳过）→ 提前让位被占用的运行库 → 执行 windeployqt（失败且为文件占用时改名让位后重试）→ 用所选 Qt bin 的运行库校正 →（勾选时）复制 VC 运行库 →（可选）Enigma 封包单文件。

打包完成后，输出目录内会生成 `<目标EXE基名>_Single-qt<版本>-<编译器><位数>.exe` 单文件便携版（例如 `WindeployqtHelper_Single-qt6.8.3-MinGW64.exe`），同一目录下不同 Qt 版本/编译器的产物可按文件名区分。

## 配置说明（config.ini）

运行目录下自动生成，`[Paths]` / `[Options]` 两组配置，与界面一一对应：

```ini
[Paths]
exePath=f:\...\App.exe
qtBin=E:/Qt/6.8.3/mingw_64/bin
outputDir=f:\...\App-qt6.8.3-MinGW64-bin
qmlDir=D:/src/qml;D:/src/views
enigmaVbConsole=D:/Program Files (x86)/Enigma Virtual Box/enigmavbconsole.exe
vsRootDir=d:/Program Files/Microsoft Visual Studio

[Options]
chkCleanOld=false
chkMultimedia=false
chkZip=false
chkSlimPlugin=false
customKeepPlugins=
chkCopyVcRuntime=true
version=1.0.0
extraDlls=
chkEnigmaPack=true
keepFiles=config.ini
```

## 技术要点

### 打包前 Qt 版本校验

选错 Qt 的 `bin` 目录，windeployqt 就会把不相关版本的 DLL 拷进输出目录，甚至覆盖掉程序真正需要的那份。所以点「开始打包」后、清空目录/复制主程序/让位运行库之前，先由 `checkQtTargetMatch()` 做三项核对：

| 维度 | 目标 EXE 侧 | 所选 Qt 侧 | 不符时 |
| --- | --- | --- | --- |
| Qt 主版本 | 导入表含 `Qt5Core.dll` 还是 `Qt6Core.dll` | `bin` 目录里装的是哪一个（二者不会同时存在） | 报错中止 |
| 位数 | PE 头 Machine 字段（`0x8664`=64，`0x014C`=32） | 同上，读该 bin 里的 Core DLL | 报错中止 |
| 编译器 | 含 `libgcc*`/`libstdc++-6.dll`/`libwinpthread-1.dll` 判为 MinGW；含 `vcruntime140*.dll`/`msvcp140*.dll` 判为 MSVC | `qmake -v` 的识别结果 | 报错中止 |

判定依据是**真解析 PE 导入表**（`readImportDllNames()`：DOS 头 → `0x3C` 处 `e_lfanew` → PE 头 → OptionalHeader 的 `DataDirectory[1]` → 节表把 RVA 换算成文件偏移 → 遍历 `IMAGE_IMPORT_DESCRIPTOR` 取 `Name` 字段读 DLL 名），而不是在整个文件里搜 DLL 名。后者在部署工具这类程序上必然失效——本程序源码里就写满了 `libgcc_s_seh-1.dll`、`vcruntime140.dll` 这些常量，字符串会原样进 exe，全文搜索会让一个 Qt5 MinGW 目标同时"命中" Qt5、Qt6、MinGW、MSVC 四项。

判不出来时只打 `⚠️` 不阻断：目标程序静态链接了 Qt（导入表里没有 `QtXCore.dll`），或运行库全静态（既无 `libgcc`/`libwinpthread` 也无 `vcruntime140`）。注意 MinGW 程序用 `-static-libgcc -static-libstdc++` 后导入表只剩 `libwinpthread-1.dll` 一个 MinGW 特征，所以它也被算进编译器判定。

### 就地打包与文件占用自愈

`windeployqt` 对 Qt 自身 DLL 会先比对内容，相同则输出 `is up to date` 跳过；但对编译器运行库（如 `libgcc_s_seh-1.dll`）是**先删后拷**。输出目录就是目标 EXE 所在目录、而该 EXE 正在运行时，这些文件被系统锁定、删不掉，`windeployqt` 直接以 `exitCode=1` 失败。工具从三个层面处理：

1. **不传多余参数**：就地打包且 `libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll` 已在输出目录时，不再传 `--compiler-runtime`——文件本就齐全，传了只会去删被占用的同名文件；
2. **改名让位 + 重试**：仍出现 `Cannot remove existing file <路径>` 时，解析出被占用文件，在**同目录内**改名为 `<原文件名>.wdh-locked`（Windows 允许改名已加载的 DLL，进程继续使用旧文件、运行不受影响），随后用同一组参数重跑 `windeployqt`，最多 2 次；
3. **清理备份**：封包时跳过 `*.wdh-locked`；这些备份在部署完成后、封包结束后、程序退出时各尝试删除一次，仍被占用的（例如打包程序自身时锁在自己进程里）留到下次启动清理。

### VC 运行库来源选择

MSVC 版 exe 在未安装运行库的机器上会因缺少 `vcruntime140.dll` 等直接启动失败，因此勾选「复制VC运行库」时会：

1. `--compiler-runtime` 交给 windeployqt（它可能自己取不到 Redist 路径）；
2. 部署完成后由 `copyVcRuntimeDlls()` 兜底，从以下位置按顺序找源目录：
   - `<VS根>/<年份>/<版本>/VC/Redist/MSVC/<号>/x64/Microsoft.VC*.CRT`：用 Qt Bin 目录名里的 `msvc<年份>` 过滤，**按年份数值距离挑最接近的**（目标 2019 而本机只有 2022 / 2026 时选 2022，不盲目取最高版本）；年份距离相同再比 Redist 版本号，有目标年份取更高版本、无年份信息取更低版本以兼容老机器；
   - 都找不到时回退 `C:/Windows/System32`（系统当前生效的那份运行库）。
3. 复制时若目标文件已存在且大小一致则跳过；大小不同则先改名 `*.wdh-locked` 让位再放入新文件，同样受「文件被运行中程序加载」影响。

`onecore\x64`、`x86` 等子目录里 `vcruntime140_1.dll` 可能缺失，故只取 `x64` 主目录下的 CRT。

### MinGW 运行库来源校正

`windeployqt` 复制 MinGW 运行库（`libstdc++-6.dll`、`libgcc_s_seh-1.dll`、`libwinpthread-1.dll`）时，并不是从 Qt kit 的 `bin` 取，而是顺着 `PATH` 找 `g++.exe` 所在的那套工具链。Qt kit 的 `bin` 里只有 DLL、没有 g++，所以只前置它也仍会命中系统里其它 MinGW——典型是 Strawberry Perl 自带的 `D:\Strawberry\c\bin`（740183 / 2304720 / 94355，2024 年文件），与 Qt5 kit 的 76288 / 1417216 / 52224 完全不同。

版本一旦不一致，windeployqt 每次判定都要「先删后拷」，撞上正在运行的程序锁定的 DLL 就 `exitCode=1`。工具分三步处理：

1. **部署前让位**：`preBackupRuntimeDlls()` 按编译器类型把输出目录中的同名 DLL 改名为 `*.wdh-locked`（MinGW 三件套；勾选复制 VC 运行库时再加 VC 六件），windeployqt 此时就无需删除任何被占用文件；失败时 `restorePreBackups()` 全部改回原名，目录不会缺文件；
2. **前置 PATH**：调用 windeployqt 前，把配套工具链 `bin`（`findMinGwRuntimeDir()` 按 `libstdc++-6.dll` 大小与 kit 完全一致来挑选）插到 PATH 首位，让它从正确来源复制；
3. **固定来源校正**：部署完成后 `fixMinGwRuntimeDlls()` 以**界面上所选 Qt 的 `bin` 目录**为唯一来源覆盖运行库，不再推导、不再去系统里找同名 DLL。MSVC 的 kit `bin` 里没有这些运行库，源文件不存在即跳过，不影响 MSVC 部署。

各版本自带值互不通用，可作为核对依据：

| 来源 | libgcc_s_seh-1 | libstdc++-6 | libwinpthread-1 |
| --- | --- | --- | --- |
| `Qt\5.15.2\mingw81_64\bin` | 76288 | 1417216 | 52224 |
| `Qt\6.8.3\mingw_64\bin` | 109056 | 2243072 | 53248 |
| Strawberry Perl 自带（错误来源） | 740183 | 2304720 | 94355 |

### .evb 工程文件（EVB 11.30）

Enigma Virtual Box 11.30 的 `.evb` 工程文件为 **XML 格式**（旧版 INI 模板会被控制台拒绝：`Project file is invalid!`）：

```xml
<Project>
  <InputFile>...</InputFile>
  <OutputFile>...</OutputFile>
  <Files>
    <Enabled>True</Enabled>
    <DeleteExtractedOnExit>False</DeleteExtractedOnExit>
    <CompressFiles>True</CompressFiles>
    <Files>
      <!-- 文件: <Type>2</Type>，虚拟目录: <Type>3</Type>（根目录名为 %DEFAULT FOLDER%） -->
    </Files>
  </Files>
</Project>
```

源码内 `escapeXml()` 处理特殊字符转义，`appendEvbFolder()` 递归生成文件树，并在递归时**跳过名为 `data` 的目录**（大小写不敏感，日志输出 `ℹ️ 跳过运行时数据目录：...`）、跳过 `*.wdh-locked`。

### Qt5 / Qt6 兼容

代码使用两版本共有的 API（如 `Qt::SkipEmptyParts`，Qt 5.14+ 已提供），**无需依赖 core5compat 模块**；`.pro` 中按 `QT_VERSION` 区分构建，输出目录按 `DIR_COMPILER` 区分编译器与位数。

### 退出行为

关闭窗口时先置内部 `shuttingDown` 标志，再 `kill()` 仍在运行的 `windeployqt` / `enigmavbconsole` 子进程（各等待最多 3 秒），两个进程回调遇到该标志直接返回。否则打包进行中点关闭，主进程会一直等子进程结束，表现为「界面已消失但进程仍在」、目录内 exe 与 DLL 一直被锁。

### 常见问题

- **`Warning: Cannot find any version of the dxcompiler.dll and dxil.dll`**：纯 QWidgets 程序无需 D3D12 渲染后端，可忽略（该警告仅与 Qt Quick 的 D3D12 后端相关）。
- **日志出现 `❌ 版本不匹配` / `❌ 位数不匹配` / `❌ 编译器不匹配`**：所选 Qt Bin 与目标 EXE 不配套，按日志里写的原因换成对应 Qt 版本/位数的 `bin` 目录。工具在动任何文件之前就中止，输出目录不会被污染。
- **windeployqt 报 `does not seem to be a Qt executable`**：同样是版本/编译器不匹配，但说明程序绕过了校验（例如目标程序静态链接 Qt、判不出主版本），换对应 Qt Bin 即可。
- **`Cannot remove existing file ...` 或 `exitCode=1`**：目标目录里的同名 DLL 正被运行中的程序加载。工具会自动改名让位重试；若日志仍报失败，说明文件被其它进程独占，关掉对应程序后重试即可。
- **VC 运行库复制失败**：同上，目标 DLL 被运行中的程序占用；工具已改为大小一致跳过、不一致改名让位。
- **重新编译报 `LNK1104: 无法打开文件 ...WindeployqtHelper.exe`**：该 exe 正在运行（哪怕窗口已关，也可能因打包未结束而仍在）。结束对应进程后再编译。
- **Enigma 报 `File does not exist` / `Cannot open file`**：部署目录内的运行时数据目录（如 `data`，含按天生成/清理的历史记录 db、导出文件）在打包瞬间文件可能已被删除或正被占用。工具已自动跳过名为 `data` 的目录；其它动态数据目录请勿放入部署目录。

## 目录结构

```
WindeployqtHelper/
├── WindeployqtHelper.pro   # 工程文件（Qt5/Qt6 双兼容、版本资源、构建开关）
├── QtCommonConfig/config.pri # 通用构建配置（编译模式/平台/DESTDIR 输出目录）
├── main.cpp
├── mainwindow.h            # 主窗口声明
├── mainwindow.cpp          # 主窗口实现（部署 + VC/MinGW 运行库 + Enigma 打包逻辑）
├── mainwindow.ui           # 界面布局
├── version.h               # 版本号常量与编译时间接口
├── version.cpp             # 由 __DATE__ / __TIME__ 生成版本号
├── resources/WindeployqtHelper.rc # 自定义版本资源（UTF-8，避免中文乱码）
└── readme.md
```

## License

[MIT](LICENSE) © netmankind
