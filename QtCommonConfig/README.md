# QtCommonConfig

Qt 工程通用配置集合，适用于 Qt 5.15 / Qt 6.x，Windows / Linux / macOS，MSVC / MinGW / GCC / Clang。

## 内容

| 文件 | 说明 |
|---|---|
| `config.pri` | qmake 通用工程配置 |

## 使用方法

### config.pri

1. 将 `config.pri` 复制到工程根目录；
2. 在主 `.pro` 中引入：

```qmake
include(config.pri)
```

### 默认开启

- C++17、大资源 / 大 GIF、单配置构建
- 日志上下文（`QT_MESSAGELOGCONTEXT`）、废弃 API 警告、禁用 Qt 5.15 之前的旧 API
- MSVC：`/utf-8`、`/MP` 多进程编译、`/W3`
- GCC / Clang：UTF-8、`-Wall`
- **Qt6 MSVC 自动补齐系统库**（user32、gdi32 等，修复 `LNK2019`）
- TARGET 默认命名（Release 为工程名、Debug 加 `_d`）
- GUI 程序自动关闭控制台、使用 Windows 子系统
- 统一中间目录（moc / rcc / ui / obj）
- 统一输出目录：`工程名-qt版本-工具链-bin`（位于**工程目录的上一级**）
- qm 输出到 exe 同级 `translations/`

### 关于共享使用（把本 config.pri 放共享目录再被各工程 include）

- 工程名取自 `$$_PRO_FILE_`（正在构建的 `.pro`），**不用 `$$PWD`**：
  在被 include 的 `.pri` 里 `$$PWD` 指向 `.pri` 自身目录，会把工程名/输出目录错算到共享目录上。
- 输出目录基准是 `$$_PRO_FILE_PWD_`（`.pro` 所在目录），因此无论 config.pri 放在哪，产物都输出到工程目录的上一级。
- 不再采用"取目录中第一个 `.pro`"的兜底，避免同目录存在多个 `.pro` 时按字母序选错工程。

### 可选开关（写在 `include(config.pri)` 之前）

| 开关 | 作用 |
|---|---|
| `CONFIG += console_app` | 控制台程序（默认 GUI 程序，无控制台黑窗） |
| `CONFIG += no_translations` | 不处理多语言（默认自动 lrelease） |
| `CONFIG += keep_deprecated` | 不禁用 Qt 旧 API |
| `CONFIG += strict_warnings` | 更严格警告（MSVC `/W4`，GCC/Clang 额外 `-Wextra`） |
| `CONFIG += no_warnings` | 不主动追加警告选项 |
| `CONFIG += static_runtime` | MSVC 静态链接 CRT（`/MT`、`/MTd`；Qt 官方 DLL 下不建议） |
| `CONFIG += auto_deploy` | Windows 构建后自动运行 windeployqt |
| `CONFIG += version_stamp` | 每次构建后删除版本目标文件以刷新版本号 |

### 可选变量

| 变量 | 默认 | 说明 |
|---|---|---|
| `VERSION_FILE_BASENAME` | `version` | 版本文件名（不含扩展名），配合 `version_stamp` 使用 |

## 提交者

netmankind <netmankind@outlook.com>
