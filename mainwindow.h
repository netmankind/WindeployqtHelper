#pragma once

#include <QDir>
#include <QMainWindow>
#include <QProcess>
#include <QSettings>

class QTextStream;

QT_BEGIN_NAMESPACE

namespace Ui
{
    class MainWindow;
}

QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    //VS 安装根目录的首次默认值：配好一次后写入 config.ini，后续沿用保存的值
    static const QString DEFAULT_VS_ROOT_DIR;

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void appendLog(const QString &msg);
    // 只负责生成 evb 并启动 Enigma 控制台进程，随后立即返回；进度与结果由 onEnigmaReadyRead / onEnigmaFinished 异步处理
    void runEnigmaVbPack(const QString &sourceDir, const QString &enigmaConsolePath, const QString &targetExe,
                         const QString &qtVersion, const QString &compilerType);
    bool detectQtCompiler(const QString &qtBinDir, QString &compilerType, QString &qtVersion);

// QT_NO_KEYWORDS 已启用，关键字写法（slots/signals/emit）被禁用，必须用带 Q_ 前缀的宏
private Q_SLOTS:
    void on_btnSelectExe_clicked();
    void on_edtExePath_editingFinished();
    void on_btnSelectQtBin_clicked();
    void on_btnOpenOutput_clicked();
    void on_btnSelectQmlDir_clicked();
    void on_btnSelectEnigmaVb_clicked();
    void on_btnSelectVsRoot_clicked();

    void on_btnStartDeploy_clicked();
    void on_btnStopDeploy_clicked();
    void on_btnSaveConfig_clicked();
    void on_btnLoadConfig_clicked();
    void on_btnClearLog_clicked();
    void on_btnSaveLog_clicked();

    void onDeployFinished(int exitCode, QProcess::ExitStatus status);
    void onDeployReadyReadStdout();
    void onDeployReadyReadStderr();

    void onEnigmaReadyRead();
    void onEnigmaFinished(int exitCode, QProcess::ExitStatus status);

private:
    Ui::MainWindow *ui;
    QProcess       *deployProcess;
    QProcess       *enigmaProcess;       // Enigma 封包进程，异步执行，避免封包期间界面冻结
    QString         enigmaEvbPath;       // 本次封包生成的临时工程文件，进程结束后删除
    QString         enigmaSingleExePath; // 本次封包的目标单文件，用于结束后判定产出
    QString         lastExePath;         // 上一次已知的目标exe路径，用于判断路径是否真的变化
    QString         deployOutputBuffer; // 累积windeployqt输出，用于失败原因分析
    // 本次打包的 Qt 版本与编译器类型，供 Enigma 单文件命名（如 _Single-qt6.8.3-MinGW64.exe）使用
    QString currentQtVersion;
    QString currentCompilerType;

    bool loadSettings();
    bool saveSettings();
    bool checkDeployInput();
    bool clearDirectoryKeepSelf(const QDir &dir, const QStringList &keepFiles);
    QString dialogStartDir(const QString &currentPath, bool currentIsDir) const;

    // ---- Enigma Virtual Box 11.30 的 .evb 工程文件为 XML 格式 ----
    static QString escapeXml(const QString &text);
    void appendEvbFolder(QTextStream &stream, const QDir &dir, const QString &virtualName, int depth);

    // ---- MinGW 运行库 DLL 来源校正（windeployqt 可能从 PATH 其它 MinGW 目录复制错版本） ----
    QString findMinGwRuntimeDir(const QString &qtBinDir) const;
    void fixMinGwRuntimeDlls(const QString &outputDir, const QString &runtimeDir);

    // ---- 目标exe位数识别：单文件名需要形如 MinGW64 / MSVC64 ----
    QString getExeArchBits(const QString &exePath) const;

    // ---- VC 运行库复制（MSVC 部署）：windeployqt --compiler-runtime 在新版 VS 上可能找不到 Redist 路径 ----
    //非 const：选择过程会写日志
    QString findVcRuntimeDir();
    void copyVcRuntimeDlls(const QString &outputDir);
};
