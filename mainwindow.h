#pragma once

#include <QDir>
#include <QMainWindow>
#include <QProcess>
#include <QSettings>
#include <QStringList>

class QTextStream;
class QCloseEvent;

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
    //打包前先核对「所选 Qt」与「目标 EXE」是否配套：Qt 主版本（Qt5/Qt6）、位数、编译器类型
    //任一不符就返回 false 并中止，避免用错版本的 Qt 去部署，把不相关的 DLL 拷进输出目录
    bool checkQtTargetMatch(const QString &exePath, const QString &qtBinDir, const QString &compilerType);

protected:
    //关闭窗口时先结束还在跑的子进程，否则界面消失后主进程会一直等它退出，看起来像“程序没关掉”
    void closeEvent(QCloseEvent *event) override;

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
    bool    shuttingDown;    // 窗口正在关闭：置位后回调不再发起重试或封包，避免退出被拖住

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
    //解析 PE 导入表，取出目标 EXE 真正依赖的 DLL 名。不能直接在文件里搜字符串：
    //本程序自身代码里就写满了 libgcc_s_seh-1.dll、vcruntime140.dll 这些常量，全文扫描会全部命中
    QStringList readImportDllNames(const QString &exePath) const;

    // ---- 被锁文件自愈 ----
    //windeployqt（Qt5 的 MinGW 运行库分支）对同名 DLL 是先删除再复制，目标程序正在运行时
    //这些 DLL 被系统锁定，删除失败会让 windeployqt 以退出码 1 结束。Windows 不允许删除或覆盖
    //已加载的 DLL，但允许在同一目录内改名，因此把被锁文件改名让位后重跑即可完成部署。
    int  backupLockedFilesFromOutput();
    void restartDeployProcess();
    void stopRunningProcesses();
    //启动 windeployqt 之前，把部署目录里会被它「先删后拷」的运行库一次性改名让位：
    //目标程序运行中锁着这些 DLL 时，逐个等它报错再让位要重试好几轮，提前让位则一次到位
    void preBackupRuntimeDlls(const QString &outputDir, const QString &compilerType, bool copyVcRuntime);
    //部署失败时把提前让位的运行库改回原名，避免目录里缺 DLL
    void restorePreBackups();
    //dirPath 为空时清理程序自身所在目录；打包结束、封包结束、程序退出时都会调用一次，
    //真正被占用删不掉的文件留到下次启动再清
    void cleanupLockedBackups(const QString &dirPath = QString());

    int         deployRetryCount = 0;    //本次打包因文件占用重跑的次数
    QStringList preBackupOriginals;      //提前让位的运行库原路径，部署失败时按此恢复
    QString     lastWindeployqtPath;     //以下四项用于重跑时复用上次 windeployqt 的启动参数
    QStringList lastDeployArgs;
    QString     lastDeployWorkDir;
    QString     lastDeployRuntimeDir;

    // ---- VC 运行库复制（MSVC 部署）：windeployqt --compiler-runtime 在新版 VS 上可能找不到 Redist 路径 ----
    //非 const：选择过程会写日志
    QString findVcRuntimeDir();
    void copyVcRuntimeDlls(const QString &outputDir);
};
