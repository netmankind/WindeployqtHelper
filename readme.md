# WindeployqtHelper

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Qt 应用一键部署打包助手（GUI 工具）：图形化封装 `windeployqt` 部署流程，并可追加 **Enigma Virtual Box** 单文件便携打包，支持 **Qt 5.15 / Qt 6** 双版本、**MinGW / MSVC** 双编译器。

## 功能特性

- **图形化配置**：目标 EXE、Qt Bin 目录、输出目录、QML 目录、Enigma 控制台路径，一键开始打包。
- **选择对话框跟随输入框**：文件/目录选择器自动定位到当前输入框已填的路径（无效才回退到用户主目录）。
- **自动识别编译器**：调用 `qmake -v` 识别 MinGW / MSVC，并在 windeployqt 失败时提示「版本/编译器不匹配」（如用 Qt6 的 windeployqt 部署 Qt5 程序、MinGW 版部署 MSVC 程序）。
- **安全保护**：
  - 输出目录 = 源 EXE 所在目录时，跳过删除/复制，避免误删原主程序；
  - 清空旧目录时强制保留源 EXE，并支持自定义保留规则（通配符）。
- **windeployqt 参数支持**：`--multimedia`、`--qmldir`、`--no-plugins`（精简插件）。
- **Enigma 单文件打包**：
  - 自动生成 EVB 11.30 所需的 **XML** 工程文件（`.evb`），递归打包输出目录全部文件；
  - 单文件命名：`<目标EXE基名>_Single.exe`，**输出到输出目录内**；
  - 打包时自动跳过已存在的 `*_Single.exe`，防止旧单文件被重复打入。
- **配置持久化**：`config.ini` 保存/加载全部配置。
- **日志面板**：输出 windeployqt / Enigma 完整过程，可保存为 txt。

> 注：界面中「打包后压缩」「复制VC运行库」「自定义保留插件」「版本号」「附加DLL」为已放置的预留选项（随配置保存），尚未接入打包流程。

## 环境要求

| 依赖 | 说明 |
| --- | --- |
| Qt | 5.15 或 6.x（qmake 构建），C++17 |
| 编译器 | MinGW（win32-g++）或 MSVC（win32-msvc*） |
| windeployqt | 随 Qt 自带，位于所选 Qt Bin 目录 |
| Enigma Virtual Box | 11.30+（可选，勾选「启用Enigma单文件打包」时需要其 `enigmavbconsole.exe`） |

## 编译

### Qt Creator

直接用 Qt Creator 打开 `WindeployqtHelper.pro`，选择对应 Kit（Qt 5.15 或 Qt 6，MinGW 或 MSVC）构建即可。

### 命令行（以 Qt 6.8.3 MinGW 为例）

```bat
cd /d F:\WORK\QT_TEST\WindeployqtHelper\WindeployqtHelper
E:\Qt\6.8.3\mingw_64\bin\qmake.exe WindeployqtHelper.pro
E:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe -j8
```

输出目录规则（见 `.pro`）：

```
DESTDIR = $$PWD/../WindeployqtHelper-qt<完整版本号>-<编译器>-bin
```

即生成到工程目录的**上一级**，例如 `WindeployqtHelper-qt6.8.3-mingw-bin\WindeployqtHelper.exe`。

## 使用方法

1. **目标EXE**：选择待部署的 Qt 程序（Release 版）。
2. **Qt Bin目录**：选择与目标 EXE 相同 Qt 版本/编译器的 `bin` 目录（需含 `windeployqt.exe`），例如 `E:\Qt\6.8.3\mingw_64\bin`。
3. **输出目录**：部署产物输出位置；不填 Qt 相关依赖时也可直接作为单文件打包源目录。
4. **QML目录**（可选）：目标程序含 QML 时填写，windeployqt 会带 `--qmldir`。
5. **Enigma控制台**（可选）：勾选「启用Enigma单文件打包」后必填 `enigmavbconsole.exe` 路径。
6. 点击 **开始打包**，流程：
   - 复制主程序到输出目录（同目录时自动跳过）→ 执行 windeployqt → （可选）Enigma 封包单文件。

打包完成后，输出目录内会生成 `<目标EXE基名>_Single.exe` 单文件便携版。

## 配置说明（config.ini）

运行目录下自动生成，`[Paths]` / `[Options]` 两组配置，与界面一一对应：

```ini
[Paths]
exePath=f:\...\App.exe
qtBin=E:/Qt/6.8.3/mingw_64/bin
outputDir=f:\...\App-qt6.8.3-mingw-bin
qmlDir=
enigmaVbConsole=D:/Program Files (x86)/Enigma Virtual Box/enigmavbconsole.exe

[Options]
chkCleanOld=false
chkMultimedia=false
chkZip=false
chkSlimPlugin=false
customKeepPlugins=
chkCopyVcRuntime=false
version=1.0.0
extraDlls=
chkEnigmaPack=true
keepFiles=config.ini
```

## 技术要点

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

源码内 `escapeXml()` 处理特殊字符转义，`appendEvbFolder()` 递归生成文件树。

### Qt5 / Qt6 兼容

代码使用两版本共有的 API（如 `Qt::SkipEmptyParts`，Qt 5.14+ 已提供），**无需依赖 core5compat 模块**；`.pro` 中按 `QT_VERSION` 区分目录命名。

### 常见问题

- **`Warning: Cannot find any version of the dxcompiler.dll and dxil.dll`**：纯 QWidgets 程序无需 D3D12 渲染后端，可忽略（该警告仅与 Qt Quick 的 D3D12 后端相关）。
- **windeployqt 报 `does not seem to be a Qt executable`**：目标 EXE 与所选 Qt Bin 的版本/编译器不匹配，切换为对应的 Qt Bin 目录即可。

## 目录结构

```
WindeployqtHelper/
├── WindeployqtHelper.pro   # 工程文件（Qt5/Qt6 双兼容、DESTDIR 规则）
├── config.pri              # 通用构建配置（编译模式/平台/输出子目录）
├── main.cpp
├── mainwindow.h            # 主窗口声明
├── mainwindow.cpp          # 主窗口实现（部署 + Enigma 打包逻辑）
├── mainwindow.ui           # 界面布局
└── readme.md
```

## License

[MIT](LICENSE) © netmankind
