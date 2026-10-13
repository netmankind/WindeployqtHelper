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
    bool runEnigmaVbPack(const QString &sourceDir, const QString &enigmaConsolePath, const QString &targetExe);
    bool detectQtCompiler(const QString &qtBinDir, QString &compilerType);

private slots:
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

    bool loadSettings();
    bool saveSettings();
    bool checkDeployInput();
    bool clearDirectoryKeepSelf(const QDir &dir, const QStringList &keepFiles);
    QString dialogStartDir(const QString &currentPath, bool currentIsDir) const;

    // ---- Enigma Virtual Box 11.30 的 .evb 工程文件为 XML 格式 ----
    static QString escapeXml(const QString &text);
    static void appendEvbFolder(QTextStream &stream, const QDir &dir, const QString &virtualName, int depth);
};
