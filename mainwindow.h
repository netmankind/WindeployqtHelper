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
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void appendLog(const QString &msg);
    bool runEnigmaVbPack(const QString &sourceDir, const QString &enigmaConsolePath, const QString &targetExe,
                         const QString &qtVersion, const QString &compilerType);
    bool detectQtCompiler(const QString &qtBinDir, QString &compilerType, QString &qtVersion);

// QT_NO_KEYWORDS 已启用，关键字写法（slots/signals/emit）被禁用，必须用带 Q_ 前缀的宏
private Q_SLOTS:
    void on_btnSelectExe_clicked();
    void on_btnSelectQtBin_clicked();
    void on_btnOpenOutput_clicked();
    void on_btnSelectQmlDir_clicked();
    void on_btnSelectEnigmaVb_clicked();

    void on_btnStartDeploy_clicked();
    void on_btnStopDeploy_clicked();
    void on_btnSaveConfig_clicked();
    void on_btnLoadConfig_clicked();
    void on_btnClearLog_clicked();
    void on_btnSaveLog_clicked();

    void onDeployFinished(int exitCode, QProcess::ExitStatus status);
    void onDeployReadyReadStdout();
    void onDeployReadyReadStderr();

private:
    Ui::MainWindow *ui;
    QProcess       *deployProcess;
    QString         deployOutputBuffer; // 累积windeployqt输出，用于失败原因分析
    // 本次打包的 Qt 版本与编译器类型，供 Enigma 单文件命名（如 _Single-qt6.8.3-mingw.exe）使用
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
};
