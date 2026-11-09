#include "mainwindow.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTextStream>
#include <QVersionNumber>
#include <QtEndian>

#include "ui_mainwindow.h"
#include "version.h"

//VS 安装根目录的首次默认值，之后以 config.ini 中保存的值为准
const QString MainWindow::DEFAULT_VS_ROOT_DIR = QStringLiteral("C:/Program Files/Microsoft Visual Studio");

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), shuttingDown(false)
{
    ui->setupUi(this);

    //标题栏显示中文程序名与版本号：工程会产出多份不同 Qt 版本/编译器的构建，
    //靠标题即可区分当前运行的是哪一版；版本号由 Version 模块提供，含本次编译时间
    setWindowTitle(QStringLiteral("%1 v%2").arg(QStringLiteral("Qt 部署助手"), Version::getDisplayVersion()));

    deployProcess = new QProcess(this);

    connect(deployProcess, &QProcess::finished, this, &MainWindow::onDeployFinished);
    connect(deployProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onDeployReadyReadStdout);
    connect(deployProcess, &QProcess::readyReadStandardError, this, &MainWindow::onDeployReadyReadStderr);

    //Enigma 封包同样走异步：一次封包几十兆文件要跑很久，同步 waitForFinished 会让整个界面失去响应
    enigmaProcess = new QProcess(this);
    enigmaProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(enigmaProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onEnigmaReadyRead);
    connect(enigmaProcess, &QProcess::finished, this, &MainWindow::onEnigmaFinished);

    loadSettings();

    //记下当前目标exe路径：之后用于判断用户是否真的改了路径，只有改过才让输出目录跟随
    lastExePath = ui->edtExePath->text();

    //清掉上次打包继承下来的占用备份文件（此时锁已释放）
    cleanupLockedBackups();
}

void MainWindow::stopRunningProcesses()
{
    //windeployqt 与封包器都是异步子进程，窗口关掉后主进程会一直等它们结束；
    //enigmavbconsole 一次封包可能跑很久，不主动结束就会让进程残留，文件也一直被锁
    QProcess *processList[2] = { deployProcess, enigmaProcess };
    for(int i = 0; i < 2; ++i)
    {
        QProcess *proc = processList[i];
        if(proc == Q_NULLPTR || proc->state() == QProcess::NotRunning)
        {
            continue;
        }
        proc->kill();
        proc->waitForFinished(3000);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    //置位后两个 finished 回调不再发起重试或封包，避免关窗这一刻又拉起新进程
    shuttingDown = true;
    stopRunningProcesses();
    QMainWindow::closeEvent(event);
}

MainWindow::~MainWindow()
{
    saveSettings();
    shuttingDown = true;
    stopRunningProcesses();
    //退出前再清一次：打包其它程序时，对方若已退出这些备份文件就能删掉了
    cleanupLockedBackups();
    delete ui;
}

void MainWindow::appendLog(const QString &msg)
{
    QString timeStamp = QDateTime::currentDateTime().toString(QStringLiteral("[yyyy-MM-dd HH:mm:ss] "));
    ui->txtLog->appendPlainText(timeStamp + msg);
}

bool MainWindow::loadSettings()
{
    QSettings settings(QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Paths"));
    ui->edtExePath->setText(settings.value(QStringLiteral("exePath"), QStringLiteral("")).toString());
    ui->edtQtBin->setText(settings.value(QStringLiteral("qtBin"), QStringLiteral("")).toString());
    ui->edtOutputDir->setText(settings.value(QStringLiteral("outputDir"), QStringLiteral("")).toString());
    ui->edtQmlDir->setText(settings.value(QStringLiteral("qmlDir"), QStringLiteral("")).toString());
    ui->edtEnigmaVbPath->setText(settings.value(QStringLiteral("enigmaVbConsole"), QStringLiteral("")).toString());
    //VS 安装根目录：首次给默认路径，之后沿用保存过的值（VS 装在非 C 盘时用户只需改一次）
    QString vsRootDir = settings.value(QStringLiteral("vsRootDir"), DEFAULT_VS_ROOT_DIR).toString();
    ui->edtVsRootDir->setText(vsRootDir);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("Options"));
    ui->chkCleanOld->setChecked(settings.value(QStringLiteral("chkCleanOld"), false).toBool());
    ui->chkMultimedia->setChecked(settings.value(QStringLiteral("chkMultimedia"), false).toBool());
    ui->chkZip->setChecked(settings.value(QStringLiteral("chkZip"), false).toBool());
    ui->chkSlimPlugin->setChecked(settings.value(QStringLiteral("chkSlimPlugin"), false).toBool());
    ui->edtCustomKeepPlugins->setText(settings.value(QStringLiteral("customKeepPlugins"), QStringLiteral("")).toString());
    ui->chkCopyVcRuntime->setChecked(settings.value(QStringLiteral("chkCopyVcRuntime"), false).toBool());
    ui->edtVersion->setText(settings.value(QStringLiteral("version"), QStringLiteral("1.0.0")).toString());
    ui->edtExtraDlls->setText(settings.value(QStringLiteral("extraDlls"), QStringLiteral("")).toString());
    ui->chkEnigmaPack->setChecked(settings.value(QStringLiteral("chkEnigmaPack"), false).toBool());
    ui->edtKeepFiles->setText(settings.value(QStringLiteral("keepFiles"), QStringLiteral("config.ini")).toString());

    settings.endGroup();
    return true;
}

bool MainWindow::saveSettings()
{
    QSettings settings(QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Paths"));
    settings.setValue(QStringLiteral("exePath"), ui->edtExePath->text());
    settings.setValue(QStringLiteral("qtBin"), ui->edtQtBin->text());
    settings.setValue(QStringLiteral("outputDir"), ui->edtOutputDir->text());
    settings.setValue(QStringLiteral("qmlDir"), ui->edtQmlDir->text());
    settings.setValue(QStringLiteral("enigmaVbConsole"), ui->edtEnigmaVbPath->text());
    settings.setValue(QStringLiteral("vsRootDir"), ui->edtVsRootDir->text());
    settings.endGroup();

    settings.beginGroup(QStringLiteral("Options"));
    settings.setValue(QStringLiteral("chkCleanOld"), ui->chkCleanOld->isChecked());
    settings.setValue(QStringLiteral("chkMultimedia"), ui->chkMultimedia->isChecked());
    settings.setValue(QStringLiteral("chkZip"), ui->chkZip->isChecked());
    settings.setValue(QStringLiteral("chkSlimPlugin"), ui->chkSlimPlugin->isChecked());
    settings.setValue(QStringLiteral("customKeepPlugins"), ui->edtCustomKeepPlugins->text());
    settings.setValue(QStringLiteral("chkCopyVcRuntime"), ui->chkCopyVcRuntime->isChecked());
    settings.setValue(QStringLiteral("version"), ui->edtVersion->text());
    settings.setValue(QStringLiteral("extraDlls"), ui->edtExtraDlls->text());
    settings.setValue(QStringLiteral("chkEnigmaPack"), ui->chkEnigmaPack->isChecked());
    settings.setValue(QStringLiteral("keepFiles"), ui->edtKeepFiles->text());

    settings.endGroup();
    return true;
}

bool MainWindow::checkDeployInput()
{
    if(ui->edtExePath->text().trimmed().isEmpty() || !QFile::exists(ui->edtExePath->text()))
    {
        QMessageBox::warning(this, QStringLiteral("校验失败"), QStringLiteral("目标EXE不存在！"));
        return false;
    }
    if(ui->edtQtBin->text().trimmed().isEmpty() || !QDir(ui->edtQtBin->text()).exists())
    {
        QMessageBox::warning(this, QStringLiteral("校验失败"), QStringLiteral("Qt bin目录不存在！"));
        return false;
    }
    if(ui->edtOutputDir->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("校验失败"), QStringLiteral("输出目录不能为空！"));
        return false;
    }
    if(ui->chkEnigmaPack->isChecked())
    {
        QString enigmaPath = ui->edtEnigmaVbPath->text().trimmed();
        if(enigmaPath.isEmpty() || !QFile::exists(enigmaPath))
        {
            QMessageBox::warning(this, QStringLiteral("校验失败"), QStringLiteral("已勾选Enigma打包，请选择有效的 enigmavbconsole.exe！"));
            return false;
        }
    }
    return true;
}

bool MainWindow::clearDirectoryKeepSelf(const QDir &dir, const QStringList &keepPatterns)
{
    if(!dir.exists())
    {
        return true;
    }

    QFileInfoList entryList = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for(const QFileInfo &info: entryList)
    {
        QString fileName = info.fileName();
        bool    needKeep = false;

        //支持通配符匹配 *.ini, readme*
        for(const QString &pattern: keepPatterns)
        {
            QString pat = pattern.trimmed();
            if(pat.isEmpty())
            {
                continue;
            }
            if(QDir::match(pat, fileName))
            {
                needKeep = true;
                break;
            }
        }

        if(needKeep)
        {
            appendLog(QStringLiteral("ℹ️ 跳过保留文件：%1（匹配规则）").arg(info.fileName()));
            continue;
        }

        if(info.isDir())
        {
            QDir subDir(info.absoluteFilePath());
            if(!subDir.removeRecursively())
            {
                appendLog(QStringLiteral("⚠️ 删除子目录失败：%1").arg(info.absoluteFilePath()));
            }
        }
        else
        {
            if(!QFile::remove(info.absoluteFilePath()))
            {
                appendLog(QStringLiteral("⚠️ 删除文件失败：%1").arg(info.absoluteFilePath()));
            }
        }
    }
    return true;
}

//计算文件/目录选择对话框的起始目录：跟随输入框当前已填的路径
QString MainWindow::dialogStartDir(const QString &currentPath, bool currentIsDir) const
{
    QString startDir = QDir::homePath();
    QString path     = currentPath.trimmed();
    if(!path.isEmpty())
    {
        QFileInfo info(path);
        //currentIsDir=true（目录选择框）直接用当前目录；否则（文件选择框）用其所在目录
        QString dir = currentIsDir ? info.absoluteFilePath() : info.absolutePath();
        if(QDir(dir).exists())
        {
            startDir = dir;
        }
    }
    return startDir;
}

void MainWindow::on_btnSelectExe_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("选择目标EXE"), dialogStartDir(ui->edtExePath->text(), false), QStringLiteral("Executable(*.exe)"));
    if(!filePath.isEmpty())
    {
        ui->edtExePath->setText(filePath);
        lastExePath = filePath;
        //选完目标exe后输出目录默认跟随该exe所在目录：原地部署是最常见用法，省去再手动选一次，用户之后仍可改
        ui->edtOutputDir->setText(QFileInfo(filePath).absolutePath());
    }
}

void MainWindow::on_edtExePath_editingFinished()
{
    //手输路径与按钮选择保持一致：exe 换了，输出目录跟着换。
    //只在路径确实变化时跟随，否则用户单独编辑输出目录后一失焦就会被这里覆盖掉
    QString exePath = ui->edtExePath->text().trimmed();
    if(exePath == lastExePath)
    {
        return;
    }
    lastExePath = exePath;

    if(exePath.isEmpty())
    {
        return;
    }
    ui->edtOutputDir->setText(QFileInfo(exePath).absolutePath());
}

void MainWindow::on_btnSelectQtBin_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择Qt bin目录"), dialogStartDir(ui->edtQtBin->text(), true));
    if(!dir.isEmpty())
    {
        ui->edtQtBin->setText(dir);
    }
}

void MainWindow::on_btnOpenOutput_clicked()
{
    QString path = ui->edtOutputDir->text().trimmed();
    if(QDir(path).exists())
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
    else
    {
        appendLog(QStringLiteral("⚠️ 输出目录不存在"));
    }
}

void MainWindow::on_btnSelectQmlDir_clicked()
{
    //支持多个 QML 目录：输入框内用英文分号分隔，再点一次“选择”就追加到末尾
    const QStringList currentDirs = ui->edtQmlDir->text().split(QStringLiteral(";"), Qt::SkipEmptyParts);
    const QString startDir = currentDirs.isEmpty() ? QString() : currentDirs.first().trimmed();

    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择QML源码目录"), dialogStartDir(startDir, true));
    if(dir.isEmpty())
    {
        return;
    }
    dir = QDir::toNativeSeparators(dir);
    if(currentDirs.contains(dir))
    {
        return;    //同一个目录不重复添加
    }
    QString newText = ui->edtQmlDir->text().trimmed();
    if(newText.isEmpty())
    {
        ui->edtQmlDir->setText(dir);
        return;
    }
    ui->edtQmlDir->setText(newText + QStringLiteral(";") + dir);
}

void MainWindow::on_btnSelectEnigmaVb_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("选择 enigmavbconsole.exe"), dialogStartDir(ui->edtEnigmaVbPath->text(), false), QStringLiteral("Enigma Console(enigmavbconsole.exe)"));
    if(!filePath.isEmpty())
    {
        ui->edtEnigmaVbPath->setText(filePath);
    }
}

void MainWindow::on_btnSelectVsRoot_clicked()
{
    QString dirPath = QFileDialog::getExistingDirectory(this,
                                                        QStringLiteral("选择 Visual Studio 安装根目录"),
                                                        dialogStartDir(ui->edtVsRootDir->text(), true));
    if(!dirPath.isEmpty())
    {
        ui->edtVsRootDir->setText(QDir::toNativeSeparators(dirPath));
    }
}

void MainWindow::on_btnStartDeploy_clicked()
{
    if(deployProcess->state() != QProcess::NotRunning)
    {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("打包进程正在运行！"));
        return;
    }
    if(!checkDeployInput())
    {
        return;
    }

    appendLog(QStringLiteral("===== 开始准备打包 ====="));
    deployOutputBuffer.clear();
    QString exePath   = ui->edtExePath->text().trimmed();
    QString qtBinDir  = ui->edtQtBin->text().trimmed();
    QString outputDir = ui->edtOutputDir->text().trimmed();

    QString compilerType;
    QString qtVersion;
    detectQtCompiler(qtBinDir, compilerType, qtVersion);
    //记录本次打包的 Qt 版本/编译器，供 Enigma 单文件命名使用
    currentQtVersion    = qtVersion;
    currentCompilerType = compilerType;

    //输出目录若与目标exe所在目录相同，源exe必须始终保留，防止清空旧目录时删掉原文件
    QFileInfo exeInfo(exePath);
    QDir      outDir(outputDir);
    QString   destExe = outDir.filePath(exeInfo.fileName());
    //就地打包：输出目录就是源 exe 所在目录，此时目标文件与源文件是同一个
    const bool inPlacePack = (exeInfo.canonicalFilePath() == QFileInfo(destExe).canonicalFilePath());
    //打包自身：目标 exe 就是当前正在运行的这个程序
    const bool packSelf = (QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath() == exeInfo.canonicalFilePath());
    if(packSelf)
    {
        appendLog(QStringLiteral("ℹ️ 检测到正在打包程序自身：运行中的 exe 与已加载的 DLL 被系统锁定，"
                                 "已存在的同名文件不会覆盖，建议另选一个空输出目录"));
    }

    if(ui->chkCleanOld->isChecked())
    {
        if(inPlacePack)
        {
            //就地打包时清空会把目录里的第三方 DLL 一起删掉，正在使用的文件也删不掉，
            //所以这种情况下跳过清空，只补部署缺失的文件
            appendLog(QStringLiteral("⚠️ 输出目录与源 exe 同目录，跳过“清空旧目录”，避免误删目录内其它 DLL"));
        }
        else
        {
            appendLog(QStringLiteral("⚠️ 清空旧输出目录（保留目录本身，跳过配置的保留文件）"));
            QString     rawKeep  = ui->edtKeepFiles->text().trimmed();
            QStringList keepList = rawKeep.split(QStringLiteral(","), Qt::SkipEmptyParts);
            keepList << exeInfo.fileName();
            clearDirectoryKeepSelf(outDir, keepList);
        }
    }
    outDir.mkpath(QStringLiteral("."));

    //拷贝exe到输出目录：目标与源为同一文件（输出目录=源exe目录）时跳过删除与复制，避免删掉原exe
    if(inPlacePack)
    {
        appendLog(QStringLiteral("ℹ️ 输出目录与源exe目录相同，主程序已就位，无需复制：%1").arg(destExe));
    }
    else
    {
        QFile::remove(destExe);
        if(!QFile::copy(exePath, destExe))
        {
            appendLog(QStringLiteral("❌ 复制主程序到输出目录失败：%1").arg(destExe));
            return;
        }
        appendLog(QStringLiteral("✅ 复制主程序到输出目录：%1").arg(destExe));
    }

    //运行库提前让位：windeployqt 对这些 DLL 是先删后拷，目标程序运行中删不掉就会以退出码 1 结束
    preBackupRuntimeDlls(outputDir, compilerType, ui->chkCopyVcRuntime->isChecked());

    QString     windeployqt = QDir(qtBinDir).filePath(QStringLiteral("windeployqt.exe"));
    QStringList args;
    args << destExe;
    if(ui->chkMultimedia->isChecked())
    {
        args << QStringLiteral("--multimedia");
    }
    //QML 目录可填多个（英文分号分隔）：windeployqt 支持重复传 --qmldir，每个目录各扫一遍 import
    const QStringList qmlDirs = ui->edtQmlDir->text().split(QStringLiteral(";"), Qt::SkipEmptyParts);
    for(const QString &qmlDir: qmlDirs)
    {
        const QString trimmedDir = qmlDir.trimmed();
        if(!trimmedDir.isEmpty())
        {
            args << QStringLiteral("--qmldir") << trimmedDir;
        }
    }
    if(ui->chkSlimPlugin->isChecked())
    {
        args << QStringLiteral("--no-plugins");
    }
    if(ui->chkCopyVcRuntime->isChecked())
    {
        //MSVC 部署需要随包带上 VC 运行库；windeployqt 自身可能取不到 Redist 路径，
        //部署完成后还会由 copyVcRuntimeDlls() 兜底复制
        //就地打包时 windeployqt 对已存在的运行库文件是先删后拷，目标程序运行中删不掉会直接失败；
        //MinGW 运行库在输出目录里已齐全时不必再传该参数
        bool mingwRuntimeAllPresent = false;
        if(inPlacePack)
        {
            const QStringList mingwRuntimeDlls = QStringList()
                                                  << QStringLiteral("libgcc_s_seh-1.dll")
                                                  << QStringLiteral("libstdc++-6.dll")
                                                  << QStringLiteral("libwinpthread-1.dll");
            mingwRuntimeAllPresent = true;
            for(const QString &dllName: mingwRuntimeDlls)
            {
                if(!QFile::exists(outDir.filePath(dllName)))
                {
                    mingwRuntimeAllPresent = false;
                    break;
                }
            }
        }
        if(mingwRuntimeAllPresent)
        {
            appendLog(QStringLiteral("ℹ️ 输出目录已存在完整的 MinGW 运行库，本次跳过 --compiler-runtime（避免删除被占用的同名 DLL）"));
        }
        else
        {
            args << QStringLiteral("--compiler-runtime");
        }
    }

    appendLog(QStringLiteral("执行 windeployqt: %1 %2").arg(windeployqt, args.join(QStringLiteral(" "))));
    deployProcess->setProgram(windeployqt);
    deployProcess->setArguments(args);
    deployProcess->setWorkingDirectory(qtBinDir);

    //将配套 MinGW 工具链 bin 前置到 PATH，避免 windeployqt 从 PATH 中其它 MinGW 目录复制版本不匹配的运行库 DLL
    QString mingwRuntimeDir = findMinGwRuntimeDir(qtBinDir);
    if(!mingwRuntimeDir.isEmpty())
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("PATH"), mingwRuntimeDir + QDir::listSeparator() + env.value(QStringLiteral("PATH")));
        deployProcess->setProcessEnvironment(env);
        appendLog(QStringLiteral("ℹ️ 已前置 MinGW 运行库目录到 PATH：%1").arg(mingwRuntimeDir));
    }

    //保存本次启动参数，被锁文件改名让位后需要原样重跑
    lastWindeployqtPath = windeployqt;
    lastDeployArgs      = args;
    lastDeployWorkDir   = qtBinDir;
    lastDeployRuntimeDir = mingwRuntimeDir;
    deployRetryCount    = 0;
    deployProcess->start();
}

void MainWindow::on_btnStopDeploy_clicked()
{
    if(deployProcess->state() != QProcess::NotRunning)
    {
        deployProcess->kill();
        appendLog(QStringLiteral("⚠️ 打包进程已终止"));
    }
}

void MainWindow::onDeployFinished(int exitCode, QProcess::ExitStatus status)
{
    if(shuttingDown)
    {
        return;    //窗口已在关闭，不再发起重试或封包
    }
    if(status == QProcess::NormalExit && exitCode == 0)
    {
        appendLog(QStringLiteral("✅ windeployqt执行完成"));
        QString outputDir = ui->edtOutputDir->text().trimmed();
        //windeployqt 完成后校正 MinGW 运行库 DLL 来源，防止输出目录混入从其它 MinGW 目录复制来的版本
        fixMinGwRuntimeDlls(outputDir, findMinGwRuntimeDir(ui->edtQtBin->text().trimmed()));
        if(ui->chkCopyVcRuntime->isChecked())
        {
            copyVcRuntimeDlls(outputDir);
        }
        if(ui->chkEnigmaPack->isChecked())
        {
            QString enigmaConsolePath = ui->edtEnigmaVbPath->text().trimmed();
            //以界面填写的目标EXE为准确定主程序与Single命名（目录内存在其它exe时不再取错）
            QString targetExe = QDir(outputDir).filePath(QFileInfo(ui->edtExePath->text().trimmed()).fileName());
            runEnigmaVbPack(outputDir, enigmaConsolePath, targetExe, currentQtVersion, currentCompilerType);
        }
        else
        {
            //未封包时部署到此结束，立即尝试清理部署过程中改名让位的备份文件
            cleanupLockedBackups(outputDir);
        }
    }
    else
    {
        //被锁文件让位后重跑：windeployqt 对 MinGW 运行库是先删后拷，
        //目标程序正在运行时同名 DLL 删不掉，重跑前把文件改名绕过文件锁
        if(deployRetryCount < 2)
        {
            const int backedUpCount = backupLockedFilesFromOutput();
            if(backedUpCount > 0)
            {
                ++deployRetryCount;
                restartDeployProcess();
                return;
            }
        }
        //彻底失败：先把提前让位的运行库改回原名，别让输出目录少 DLL
        restorePreBackups();
        appendLog(QStringLiteral("❌ windeployqt失败 exitCode=%1").arg(exitCode));
        //常见失败原因提示：目标exe与所选Qt Bin版本/编译器不匹配
        if(deployOutputBuffer.contains(QStringLiteral("does not seem to be a Qt executable"), Qt::CaseInsensitive))
        {
            appendLog(QStringLiteral("⚠️ 提示：目标exe与所选“Qt Bin目录”版本/编译器不匹配（例如用Qt6的windeployqt部署Qt5程序，或MinGW版windeployqt部署MSVC程序）。"));
            appendLog(QStringLiteral("   请将Qt Bin目录切换为目标exe实际使用的Qt版本与编译器对应的bin目录（含windeployqt.exe），并重新打包。"));
        }
    }
}

//启动 windeployqt 之前，把部署目录里会被它「先删后拷」的运行库一次性改名让位：
//目标程序正在运行时这些 DLL 被锁，删不掉就会以 exitCode=1 结束；逐个等它报错再让位
//要重试好几轮，提前让位后目标文件不存在，windeployqt 会直接拷贝新文件，一次到位
void MainWindow::preBackupRuntimeDlls(const QString &outputDir, const QString &compilerType, bool copyVcRuntime)
{
    preBackupOriginals.clear();

    QStringList runtimeDllNames;
    if(compilerType.compare(QStringLiteral("MinGW"), Qt::CaseInsensitive) == 0)
    {
        runtimeDllNames << QStringLiteral("libgcc_s_seh-1.dll")
                        << QStringLiteral("libstdc++-6.dll")
                        << QStringLiteral("libwinpthread-1.dll");
    }
    //VC 运行库只有在勾选「复制VC运行库」时才会被补回（windeployqt --compiler-runtime 或本程序兜底），
    //否则让位后目录里就缺这几个 DLL，因此跟勾选状态绑定
    if(copyVcRuntime)
    {
        runtimeDllNames << QStringLiteral("vcruntime140.dll")
                        << QStringLiteral("vcruntime140_1.dll")
                        << QStringLiteral("msvcp140.dll")
                        << QStringLiteral("msvcp140_1.dll")
                        << QStringLiteral("msvcp140_2.dll")
                        << QStringLiteral("concrt140.dll");
    }

    const QString backupSuffix = QStringLiteral(".wdh-locked");
    QDir          dir(outputDir);
    for(const QString &name: runtimeDllNames)
    {
        const QString filePath = dir.filePath(name);
        if(!QFile::exists(filePath))
        {
            continue;
        }
        const QString backupPath = filePath + backupSuffix;
        if(QFile::exists(backupPath))
        {
            QFile::remove(backupPath);    //上次的备份此时已不被占用，先清掉
        }
        if(QFile::rename(filePath, backupPath))
        {
            preBackupOriginals << filePath;
        }
    }
    if(!preBackupOriginals.isEmpty())
    {
        appendLog(QStringLiteral("ℹ️ 已提前让位 %1 个运行库（运行中的程序会锁住它们，改名不影响其继续运行）")
                      .arg(preBackupOriginals.size()));
    }
}

//部署彻底失败时把提前让位的运行库改回原名，避免输出目录里少了这些 DLL
void MainWindow::restorePreBackups()
{
    for(const QString &filePath: preBackupOriginals)
    {
        const QString backupPath = filePath + QStringLiteral(".wdh-locked");
        if(!QFile::exists(backupPath))
        {
            continue;
        }
        QFile::remove(filePath);    //windeployqt 可能已放入同名新文件，先清掉再恢复旧文件
        QFile::rename(backupPath, filePath);
    }
    if(!preBackupOriginals.isEmpty())
    {
        appendLog(QStringLiteral("ℹ️ 部署失败，已恢复提前让位的 %1 个运行库").arg(preBackupOriginals.size()));
    }
    preBackupOriginals.clear();
}

int MainWindow::backupLockedFilesFromOutput()
{
    //windeployqt 失败时会输出「Cannot remove existing file <完整路径>」：把这些文件在同目录内改名，
    //文件句柄仍指向原文件，运行中的程序不受影响，而同名新文件可以正常写入
    static const QRegularExpression lockedPattern(QStringLiteral("Cannot remove existing file ([^\\r\\n]+)"));
    const QString backupSuffix = QStringLiteral(".wdh-locked");

    QStringList lockedFilePaths;
    QRegularExpressionMatchIterator matchIt = lockedPattern.globalMatch(deployOutputBuffer);
    while(matchIt.hasNext())
    {
        QRegularExpressionMatch match = matchIt.next();
        //windeployqt 在路径后还跟一个冒号作分隔（"...\libgcc_s_seh-1.dll:"），
        //不剥掉就会拿着一个带冒号、实际不存在的文件名去改名，让位必然失败
        QString capturedPath = match.captured(1).trimmed();
        while(capturedPath.endsWith(QLatin1Char(':')))
        {
            capturedPath.chop(1);
            capturedPath = capturedPath.trimmed();
        }
        const QString filePath = QDir::fromNativeSeparators(capturedPath);
        if(!lockedFilePaths.contains(filePath))
        {
            lockedFilePaths << filePath;
        }
    }
    if(lockedFilePaths.isEmpty())
    {
        return 0;
    }

    int backedUpCount = 0;
    for(const QString &filePath: lockedFilePaths)
    {
        const QString backupPath = filePath + backupSuffix;
        if(QFile::exists(backupPath))
        {
            QFile::remove(backupPath);    //上次的备份此时已不被占用，先清掉
        }
        QFile lockedFile(filePath);
        if(lockedFile.exists() && lockedFile.rename(backupPath))
        {
            ++backedUpCount;
            appendLog(QStringLiteral("ℹ️ 文件被占用，已改名让位：%1 → %2")
                          .arg(QFileInfo(filePath).fileName(), QFileInfo(backupPath).fileName()));
        }
        else
        {
            //日志带上具体系统错误，便于区分「路径解析错」和「被其它进程独占」
            appendLog(QStringLiteral("⚠️ 无法改名占用文件（%1）：%2")
                          .arg(lockedFile.exists() ? lockedFile.errorString() : QStringLiteral("文件不存在"),
                               filePath));
        }
    }
    return backedUpCount;
}

void MainWindow::restartDeployProcess()
{
    deployOutputBuffer.clear();
    deployProcess->setProgram(lastWindeployqtPath);
    deployProcess->setArguments(lastDeployArgs);
    deployProcess->setWorkingDirectory(lastDeployWorkDir);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if(!lastDeployRuntimeDir.isEmpty())
    {
        env.insert(QStringLiteral("PATH"), lastDeployRuntimeDir + QDir::listSeparator() + env.value(QStringLiteral("PATH")));
    }
    deployProcess->setProcessEnvironment(env);
    appendLog(QStringLiteral("↻ 文件让位完成，重新执行 windeployqt（第 %1 次重试）").arg(deployRetryCount));
    deployProcess->start();
}

void MainWindow::cleanupLockedBackups(const QString &dirPath)
{
    //被占用改名让位留下的备份：锁一释放就能删，所以在打包结束、封包结束、程序退出时各试一次
    const QString scanDir = dirPath.isEmpty() ? QCoreApplication::applicationDirPath() : dirPath;
    QDir          scanQDir(scanDir);
    if(!scanQDir.exists())
    {
        return;
    }
    const QFileInfoList backupFiles = scanQDir.entryInfoList(QStringList() << QStringLiteral("*.wdh-locked"), QDir::Files);
    if(backupFiles.isEmpty())
    {
        return;
    }

    int removedCount = 0;
    int leftCount    = 0;
    for(const QFileInfo &fileInfo: backupFiles)
    {
        if(QFile::remove(fileInfo.absoluteFilePath()))
        {
            ++removedCount;
        }
        else
        {
            ++leftCount;
        }
    }
    if(removedCount > 0)
    {
        appendLog(QStringLiteral("🧹 已清理占用备份文件 %1 个").arg(removedCount));
    }
    if(leftCount > 0)
    {
        appendLog(QStringLiteral("ℹ️ 仍有 %1 个备份文件被占用删不掉，将在下次启动时清理").arg(leftCount));
    }
}

void MainWindow::onDeployReadyReadStdout()
{
    QString out = QString::fromUtf8(deployProcess->readAllStandardOutput());
    deployOutputBuffer.append(out);
    appendLog(out);
}

void MainWindow::onDeployReadyReadStderr()
{
    QString err = QString::fromUtf8(deployProcess->readAllStandardError());
    deployOutputBuffer.append(err);
    appendLog(err);
}

void MainWindow::on_btnSaveConfig_clicked()
{
    saveSettings();
    appendLog(QStringLiteral("✅ 配置已保存到 config.ini"));
}

void MainWindow::on_btnLoadConfig_clicked()
{
    loadSettings();
    appendLog(QStringLiteral("✅ 已加载 config.ini"));
}

void MainWindow::on_btnClearLog_clicked()
{
    ui->txtLog->clear();
}

void MainWindow::on_btnSaveLog_clicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, QStringLiteral("保存日志"), QStringLiteral("deploy_log.txt"), QStringLiteral("Text(*.txt)"));
    if(filePath.isEmpty())
    {
        return;
    }
    QFile f(filePath);
    if(f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream ts(&f);
        ts << ui->txtLog->toPlainText();
        f.close();
        appendLog(QStringLiteral("✅ 日志保存至 %1").arg(filePath));
    }
    else
    {
        appendLog(QStringLiteral("❌ 日志保存失败"));
    }
}

bool MainWindow::detectQtCompiler(const QString &qtBinDir, QString &compilerType, QString &qtVersion)
{
    compilerType      = QStringLiteral("Unknown");
    qtVersion         = QString();
    QString qmakePath = QDir(qtBinDir).filePath(QStringLiteral("qmake.exe"));
    if(!QFile::exists(qmakePath))
    {
        appendLog(QStringLiteral("⚠️ qmake.exe不存在，无法识别编译器"));
        return false;
    }
    QProcess proc;
    proc.setProgram(qmakePath);
    proc.setArguments({QStringLiteral("-v")});
    proc.start();
    if(!proc.waitForFinished(3000))
    {
        appendLog(QStringLiteral("⚠️ qmake -v 执行超时"));
        proc.kill();
        return false;
    }
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    appendLog(QStringLiteral("qmake -v 输出:\n") + output);

    //解析版本号：qmake -v 输出形如 "Using Qt version 6.8.3 in E:/Qt/6.8.3/mingw_64/lib"
    const QRegularExpression      versionRe(QStringLiteral("Using Qt version\\s+([0-9]+(?:\\.[0-9]+)*)"));
    const QRegularExpressionMatch versionMatch = versionRe.match(output);
    if(versionMatch.hasMatch())
    {
        qtVersion = versionMatch.captured(1);
    }
    else
    {
        appendLog(QStringLiteral("⚠️ 未能从 qmake -v 输出解析 Qt 版本号，单文件名将使用 unknown 占位"));
    }

    if(output.contains(QStringLiteral("MSVC"), Qt::CaseInsensitive) || output.contains(QStringLiteral("Microsoft Visual Studio"), Qt::CaseInsensitive))
    {
        compilerType = QStringLiteral("MSVC");
    }
    else if(output.contains(QStringLiteral("MinGW"), Qt::CaseInsensitive) || output.contains(QStringLiteral("g++"), Qt::CaseInsensitive))
    {
        compilerType = QStringLiteral("MinGW");
    }
    appendLog(QStringLiteral("✅ 识别编译器类型：%1（Qt 版本：%2）").arg(compilerType, qtVersion.isEmpty() ? QStringLiteral("未知") : qtVersion));
    return true;
}

//定位与所选 Qt Bin 配套的 MinGW 工具链 bin 目录（含 libstdc++-6.dll 等运行库 DLL）
QString MainWindow::findMinGwRuntimeDir(const QString &qtBinDir) const
{
    QDir          qtBin(qtBinDir);
    const QString kitStdCppPath = qtBin.filePath(QStringLiteral("libstdc++-6.dll"));
    const qint64  kitStdCppSize = QFileInfo(kitStdCppPath).size();

    //1) 优先 <Qt根>/Tools/mingw*/bin：windeployqt 是靠 PATH 里的 g++.exe 定位 MinGW 运行库来源的，
    //   Qt kit 的 bin 里只有 DLL 没有 g++，只前置它仍会命中系统里别的 MinGW（例如 Strawberry Perl
    //   自带的 D:\Strawberry\c\bin），部署出来的运行库版本与 Qt 不匹配，下次打包就因旧文件删不掉而失败。
    //   工具链有多个版本时，按 libstdc++-6.dll 大小与 Qt kit 完全一致来认定同版本
    QDir qtRoot = qtBin;
    qtRoot.cdUp();    //<Qt根>/<版本>/<kit>
    qtRoot.cdUp();    //<Qt根>/<版本>
    qtRoot.cdUp();    //<Qt根>
    const QDir toolsDir(qtRoot.filePath(QStringLiteral("Tools")));
    QStringList candidates;
    QStringList sameVersionCandidates;
    if(toolsDir.exists())
    {
        const QStringList mingwDirs = toolsDir.entryList(QStringList() << QStringLiteral("mingw*"), QDir::Dirs | QDir::NoDotAndDotDot);
        for(const QString &dirName: mingwDirs)
        {
            QDir    binDir(toolsDir.filePath(dirName + QStringLiteral("/bin")));
            QString gppPath    = binDir.filePath(QStringLiteral("g++.exe"));
            QString stdCppPath = binDir.filePath(QStringLiteral("libstdc++-6.dll"));
            if(!QFile::exists(gppPath) || !QFile::exists(stdCppPath))
            {
                continue;
            }
            candidates << binDir.absolutePath();
            if(kitStdCppSize > 0 && QFileInfo(stdCppPath).size() == kitStdCppSize)
            {
                sameVersionCandidates << binDir.absolutePath();
            }
        }
    }
    if(!sameVersionCandidates.isEmpty())
    {
        return sameVersionCandidates.first();
    }

    //2) 没有严格同版本的工具链时，退回 Qt kit 自带运行库：版本与 Qt 一致，能保证程序可运行
    if(QFile::exists(kitStdCppPath))
    {
        return qtBin.absolutePath();
    }
    if(!candidates.isEmpty())
    {
        return candidates.first();
    }
    return QString();
}

//windeployqt 部署完成后，把输出目录中的 MinGW 运行库 DLL 从配套工具链重新复制覆盖，
//防止 windeployqt 从 PATH 中其它 MinGW 目录复制来版本不匹配的 DLL
void MainWindow::fixMinGwRuntimeDlls(const QString &outputDir, const QString &runtimeDir)
{
    if(runtimeDir.isEmpty())
    {
        appendLog(QStringLiteral("⚠️ 未定位到 MinGW 运行库目录，跳过运行库校正"));
        return;
    }
    const QStringList runtimeDlls = QStringList()
                                 << QStringLiteral("libstdc++-6.dll")
                                 << QStringLiteral("libgcc_s_seh-1.dll")
                                 << QStringLiteral("libgcc_s_dw2-1.dll")
                                 << QStringLiteral("libwinpthread-1.dll");
    for(const QString &dllName: runtimeDlls)
    {
        QFileInfo src(runtimeDir + QStringLiteral("/") + dllName);
        if(!src.exists())
        {
            continue;    //工具链无此 DLL（如 32 位无 seh）则跳过
        }
        QFileInfo dst(outputDir + QStringLiteral("/") + dllName);
        if(!dst.exists())
        {
            continue;    //输出目录没有该 DLL（如 MSVC 部署）则不动
        }
        QString srcPath = QDir::toNativeSeparators(src.absoluteFilePath());
        QString dstPath = QDir::toNativeSeparators(dst.absoluteFilePath());
        //先复制临时文件再替换，避免复制失败时破坏原文件
        if(QFile::copy(src.absoluteFilePath(), dst.absoluteFilePath() + QStringLiteral(".tmp")))
        {
            QFile::remove(dst.absoluteFilePath());
            if(QFile::rename(dst.absoluteFilePath() + QStringLiteral(".tmp"), dst.absoluteFilePath()))
            {
                appendLog(QStringLiteral("✅ 运行库校正：%1 ← %2（%3 字节）").arg(dstPath, srcPath).arg(src.size()));
            }
            else
            {
                //目标文件被正在运行的程序锁定：同目录改名让位后再放入新文件，
                //运行中的进程继续用旧句柄，重启后即为新版本
                const QString lockedPath = dst.absoluteFilePath() + QStringLiteral(".wdh-locked");
                QFile::remove(lockedPath);
                if(QFile::rename(dst.absoluteFilePath(), lockedPath)
                   && QFile::rename(dst.absoluteFilePath() + QStringLiteral(".tmp"), dst.absoluteFilePath()))
                {
                    appendLog(QStringLiteral("ℹ️ 运行库被占用，已改名让位后更新：%1").arg(dstPath));
                    continue;
                }
                QFile::remove(dst.absoluteFilePath() + QStringLiteral(".tmp"));
                appendLog(QStringLiteral("⚠️ 运行库校正失败：无法替换 %1").arg(dstPath));
            }
        }
        else
        {
            appendLog(QStringLiteral("⚠️ 运行库校正失败：无法复制 %1").arg(srcPath));
        }
    }
}

//读取目标exe的 PE 头判断位数，返回 "64" 或 "32"；不是有效 PE 文件时返回空字符串
QString MainWindow::getExeArchBits(const QString &exePath) const
{
    QFile peFile(exePath);
    if(!peFile.open(QIODevice::ReadOnly))
    {
        return QString();
    }
    if(peFile.read(2) != QByteArrayLiteral("MZ"))
    {
        return QString();
    }
    //e_lfanew：PE 头相对文件起始的偏移，位于 DOS 头 0x3C 处
    if(!peFile.seek(0x3C))
    {
        return QString();
    }
    const QByteArray offsetBytes = peFile.read(4);
    if(offsetBytes.size() != 4)
    {
        return QString();
    }
    const quint32 peHeaderOffset = qFromLittleEndian<quint32>(offsetBytes.constData());
    if(!peFile.seek(peHeaderOffset))
    {
        return QString();
    }
    const QByteArray peHeader = peFile.read(6);    //4 字节签名 "PE\0\0" + 2 字节 Machine
    if(peHeader.size() != 6)
    {
        return QString();
    }
    const quint16 machine = qFromLittleEndian<quint16>(peHeader.constData() + 4);
    if(machine == 0x8664)    //IMAGE_FILE_MACHINE_AMD64
    {
        return QStringLiteral("64");
    }
    if(machine == 0x014C)    //IMAGE_FILE_MACHINE_I386
    {
        return QStringLiteral("32");
    }
    return QString();
}

//定位 VC 运行库目录：优先取 Visual Studio 的 x64 Redist 官方副本，取不到时回退本机已安装的 System32
QString MainWindow::findVcRuntimeDir()
{
    //VS 安装根目录由界面配置（首次默认 DEFAULT_VS_ROOT_DIR），VS 装在其它盘时改一次即可，不做全盘搜索
    const QString vsRoot = ui->edtVsRootDir->text().trimmed();
    //根目录下常同时装着多个 VS 版本：用 Qt Bin 目录名里的 msvc 年份做"最接近"判据，
    //例如 Qt6 msvc2022_64 优先取 2022，Qt5 msvc2019_64 优先取 2019
    QRegularExpression yearRegExp(QStringLiteral("msvc(\\d{4})"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch yearMatch = yearRegExp.match(ui->edtQtBin->text());
    const QString wantYear = yearMatch.hasMatch() ? yearMatch.captured(1) : QString();

    QStringList candidateYears;    //候选所在的 VS 年份目录名
    QStringList candidateVersions; //候选所在的 Redist 版本号
    QStringList candidateDirs;     //候选 CRT 目录

    QDir rootDir(vsRoot);
    if(!vsRoot.isEmpty() && rootDir.exists())
    {
        //目录层级：<VS根>/<年份>/<版本>/VC/Redist/MSVC/<号>/x64/Microsoft.VC*.CRT
        const QStringList yearDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for(const QString &yearName: yearDirs)
        {
            const QStringList editionDirs = QDir(rootDir.filePath(yearName)).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for(const QString &editionName: editionDirs)
            {
                QDir redistDir(QDir(rootDir.filePath(yearName)).filePath(editionName + QStringLiteral("/VC/Redist/MSVC")));
                if(!redistDir.exists())
                {
                    continue;
                }
                const QStringList versionDirs = redistDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                for(const QString &versionName: versionDirs)
                {
                    QDir crtRoot(redistDir.filePath(versionName + QStringLiteral("/x64")));
                    const QStringList crtDirs = crtRoot.entryList(QStringList() << QStringLiteral("Microsoft.VC*.CRT"),
                                                                 QDir::Dirs | QDir::NoDotAndDotDot,
                                                                 QDir::Name);
                    if(crtDirs.isEmpty())
                    {
                        continue;
                    }
                    candidateYears.append(yearName);
                    candidateVersions.append(versionName);
                    //同一 Redist 版本下可能并存多个 CRT 子目录（VC143/VC145），取排序最后一个即更接近当前工具集
                    candidateDirs.append(crtRoot.filePath(crtDirs.last()));
                }
            }
        }
    }

    if(!candidateDirs.isEmpty())
    {
        const int wantYearValue = wantYear.toInt();    //无年份信息时为 0
        int bestIndex = 0;
        for(int i = 1; i < candidateDirs.size(); ++i)
        {
            //按年份数值距离挑选：目标工具集是 2019 而本机没有 2019 时，2022 比 2026 更接近，
            //不要盲目取最高版本（过新的运行库在部分机器上反而更容易出问题）
            const int distance = qAbs(candidateYears.at(i).toInt() - wantYearValue);
            const int bestDistance = qAbs(candidateYears.at(bestIndex).toInt() - wantYearValue);
            if(distance != bestDistance)
            {
                if(distance < bestDistance)
                {
                    bestIndex = i;
                }
                continue;
            }
            //年份距离相同才比版本号：有目标年份时取同一年份内更高的版本；
            //没有年份信息时取更低的版本，对老机器兼容性更好
            const QVersionNumber version = QVersionNumber::fromString(candidateVersions.at(i));
            const QVersionNumber bestVersion = QVersionNumber::fromString(candidateVersions.at(bestIndex));
            if(wantYearValue > 0 ? version > bestVersion : version < bestVersion)
            {
                bestIndex = i;
            }
        }
        appendLog(QStringLiteral("ℹ️ VC运行库候选 %1 个（目标工具集年份：%2），选中 %3 / Redist %4")
                  .arg(candidateDirs.size())
                  .arg(wantYear.isEmpty() ? QStringLiteral("未识别") : wantYear)
                  .arg(candidateYears.at(bestIndex), candidateVersions.at(bestIndex)));
        if(wantYearValue > 0 && candidateYears.at(bestIndex).toInt() != wantYearValue)
        {
            appendLog(QStringLiteral("ℹ️ 本机未找到 VS %1，已改用年份最接近的 VS %2 运行库").arg(wantYear, candidateYears.at(bestIndex)));
        }
        return candidateDirs.at(bestIndex);
    }

    //回退：本机已装运行时（System32 的 vcruntime140.dll 即当前系统生效的那份）
    const QString systemDir = QStringLiteral("C:/Windows/System32");
    if(QFile::exists(systemDir + QStringLiteral("/vcruntime140.dll")))
    {
        appendLog(QStringLiteral("ℹ️ “%1”下未找到 VC Redist，改用 System32 中已安装的运行库").arg(vsRoot));
        return systemDir;
    }
    return QString();
}

//把 VC 运行库 DLL 复制到输出目录：MSVC 版 exe 在未装运行库的机器上会直接报缺少 DLL
void MainWindow::copyVcRuntimeDlls(const QString &outputDir)
{
    const QString srcDir = findVcRuntimeDir();
    if(srcDir.isEmpty())
    {
        appendLog(QStringLiteral("⚠️ 未找到 VC 运行库目录（Visual Studio Redist 与 System32 均不可用），跳过复制"));
        return;
    }
    appendLog(QStringLiteral("ℹ️ VC运行库来源目录：%1").arg(QDir::toNativeSeparators(srcDir)));

    const QStringList dllNames = QStringList() << QStringLiteral("vcruntime140.dll") << QStringLiteral("vcruntime140_1.dll")
                                               << QStringLiteral("msvcp140.dll") << QStringLiteral("msvcp140_1.dll")
                                               << QStringLiteral("msvcp140_2.dll") << QStringLiteral("concrt140.dll");
    for(const QString &dllName: dllNames)
    {
        QFileInfo srcFile(QDir(srcDir).filePath(dllName));
        if(!srcFile.exists())
        {
            continue;    //运行库不保证每个 DLL 都存在，缺哪个跳过哪个
        }
        const QString dstPath = QDir(outputDir).filePath(dllName);
        QFileInfo     dstFile(dstPath);
        if(dstFile.exists() && dstFile.size() == srcFile.size())
        {
            appendLog(QStringLiteral("ℹ️ VC运行库已存在且大小一致，跳过：%1").arg(dllName));
            continue;
        }
        QFile::remove(dstPath);
        if(QFile::copy(srcFile.absoluteFilePath(), dstPath))
        {
            appendLog(QStringLiteral("✅ VC运行库：%1（%2 字节）").arg(dllName).arg(srcFile.size()));
            continue;
        }
        //目标文件被正在运行的程序加载时删不掉：同目录改名让位后再放入新文件
        const QString lockedPath = dstPath + QStringLiteral(".wdh-locked");
        QFile::remove(lockedPath);
        if(QFile::rename(dstPath, lockedPath) && QFile::copy(srcFile.absoluteFilePath(), dstPath))
        {
            appendLog(QStringLiteral("ℹ️ VC运行库被占用，已改名让位后更新：%1").arg(dllName));
            continue;
        }
        appendLog(QStringLiteral("⚠️ VC运行库复制失败：%1").arg(dllName));
    }
}

void MainWindow::runEnigmaVbPack(const QString &sourceDir, const QString &enigmaConsolePath, const QString &targetExe, const QString &qtVersion, const QString &compilerType)
{
    appendLog(QStringLiteral("\n===== Enigma Virtual Box Console 开始封包单文件 ====="));
    QDir srcDir(sourceDir);

    //主程序必须以界面选择的目标EXE为准：Single文件名 = 目标EXE基名 + "_Single" + 版本/编译器后缀
    //目录里若存在其它exe（如旧版本exe、附带工具），枚举第一个exe会取错主程序与命名，故不能再用exeList.first()
    QString mainExe;
    QString exeBaseName;
    if(!targetExe.isEmpty() && QFile::exists(targetExe))
    {
        mainExe     = targetExe;
        exeBaseName = QFileInfo(mainExe).completeBaseName();
        appendLog(QStringLiteral("✅ 主程序(目标EXE)：%1").arg(mainExe));
    }
    else
    {
        //兼容旧行为：未传入目标exe时回退取目录内第一个exe（含多个exe时可能取错，仅作兜底）
        QFileInfoList exeList = srcDir.entryInfoList(QStringList() << QStringLiteral("*.exe"), QDir::Files);
        if(exeList.isEmpty())
        {
            appendLog(QStringLiteral("❌ 源目录找不到exe主程序，终止Enigma打包"));
            return;
        }
        mainExe     = exeList.first().absoluteFilePath();
        exeBaseName = exeList.first().completeBaseName();
        appendLog(QStringLiteral("⚠️ 未找到目标EXE，回退使用目录内第一个exe：%1").arg(mainExe));
    }

    //单文件名追加 Qt 版本、编译器与位数标识，同目录可区分不同 Qt 版本/编译器产物：
    //如 App_Single-qt6.8.3-MinGW64.exe、App_Single-qt5.15.2-MSVC64.exe；位数取目标exe的 PE 头，识别失败时不加位数
    const QString versionTag    = qtVersion.isEmpty() ? QStringLiteral("unknown") : qtVersion;
    const QString archBits      = getExeArchBits(mainExe);
    const QString compilerTag   = compilerType.isEmpty() ? QStringLiteral("unknown") : compilerType + archBits;
    const QString singleName    = QStringLiteral("%1_Single-qt%2-%3.exe").arg(exeBaseName, versionTag, compilerTag);
    QString       singleExePath = srcDir.filePath(singleName);
    QString       evbPath       = srcDir.filePath(QStringLiteral("enigma_temp.evb"));
    appendLog(QStringLiteral("ℹ️ 单文件命名：%1（Qt %2 / %3）").arg(singleName, versionTag, compilerTag));

    QFile::remove(evbPath);
    QFile evbFile(evbPath);
    if(!evbFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        appendLog(QStringLiteral("❌ 无法创建evb工程文件：%1").arg(evbPath));
        appendLog(QStringLiteral("打开失败原因：%1").arg(evbFile.errorString()));
        return;
    }

    //==================================================================
    //Enigma Virtual Box 11.30 的 .evb 工程文件是 XML 格式（非 INI）。
    //旧版 INI 模板已被官方控制台拒绝（"Project file is invalid!"），
    //这里保留注释以便追溯：
    //
    //  QString evbTemplate = R"([Enigma Project]
    //MainExe=%1
    //OutputFile=%2
    //CompressFiles=1
    //DeleteExtractedOnExit=0
    //VirtualizeFiles=1
    //[Files]
    //AddFolderRecursive=%3
    //)";
    //  QString evbContent  = evbTemplate.arg(
    //      QDir::toNativeSeparators(mainExe),
    //      QDir::toNativeSeparators(singleExePath),
    //      QDir::toNativeSeparators(sourceDir));
    //  evbFile.write(evbContent.toUtf8());
    //==================================================================

    QTextStream evbStream(&evbFile);
    evbStream << QStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    evbStream << QStringLiteral("<Project>\n");
    evbStream << QStringLiteral("  <InputFile>") << escapeXml(QDir::toNativeSeparators(mainExe)) << QStringLiteral("</InputFile>\n");
    evbStream << QStringLiteral("  <OutputFile>") << escapeXml(QDir::toNativeSeparators(singleExePath)) << QStringLiteral("</OutputFile>\n");
    evbStream << QStringLiteral("  <Files>\n");
    evbStream << QStringLiteral("    <Enabled>True</Enabled>\n");
    evbStream << QStringLiteral("    <DeleteExtractedOnExit>False</DeleteExtractedOnExit>\n");
    evbStream << QStringLiteral("    <CompressFiles>True</CompressFiles>\n");
    evbStream << QStringLiteral("    <Files>\n");
    appendEvbFolder(evbStream, srcDir, QStringLiteral("%DEFAULT FOLDER%"), 1);
    evbStream << QStringLiteral("    </Files>\n");
    evbStream << QStringLiteral("  </Files>\n");
    evbStream << QStringLiteral("</Project>\n");
    evbStream.flush();
    evbFile.close();
    appendLog(QStringLiteral("✅ 生成临时evb：%1").arg(evbPath));

    if(enigmaProcess->state() != QProcess::NotRunning)
    {
        appendLog(QStringLiteral("⚠️ 上一次 Enigma 封包仍在进行，本次不重复启动"));
        QFile::remove(evbPath);
        return;
    }

    //异步启动：路径先存成员，进程结束后由 onEnigmaFinished 判定产出并清理临时工程文件
    enigmaEvbPath       = evbPath;
    enigmaSingleExePath = singleExePath;

    QStringList args;
    args << evbPath;
    appendLog(QStringLiteral("执行EnigmaVB Console: %1 %2").arg(enigmaConsolePath, args.join(QStringLiteral(" "))));
    appendLog(QStringLiteral("Enigma Output:"));
    enigmaProcess->setProgram(enigmaConsolePath);
    enigmaProcess->setArguments(args);
    enigmaProcess->start();
}

void MainWindow::onEnigmaReadyRead()
{
    QByteArray chunk = enigmaProcess->readAllStandardOutput();
    if(chunk.isEmpty())
    {
        return;
    }
    appendLog(QString::fromUtf8(chunk));
}

void MainWindow::onEnigmaFinished(int exitCode, QProcess::ExitStatus status)
{
    if(shuttingDown)
    {
        return;    //窗口已在关闭，不再处理封包结果
    }
    //进程结束后可能还有未读的输出尾巴
    QByteArray tail = enigmaProcess->readAllStandardOutput();
    if(!tail.isEmpty())
    {
        appendLog(QString::fromUtf8(tail));
    }

    if(!enigmaEvbPath.isEmpty())
    {
        QFile::remove(enigmaEvbPath);
        enigmaEvbPath.clear();
    }

    if(status == QProcess::NormalExit && exitCode == 0 && QFile::exists(enigmaSingleExePath))
    {
        appendLog(QStringLiteral("✅ Enigma打包成功！单文件输出：%1").arg(enigmaSingleExePath));
    }
    else
    {
        appendLog(QStringLiteral("❌ EnigmaVB失败，exitCode=%1").arg(exitCode));
    }
    enigmaSingleExePath.clear();
    appendLog(QStringLiteral("已删除临时evb工程文件"));

    //封包结束，尝试清理部署时改名让位的备份文件
    cleanupLockedBackups(ui->edtOutputDir->text().trimmed());
}

QString MainWindow::escapeXml(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    escaped.replace(QLatin1Char('\''), QStringLiteral("&apos;"));
    return escaped;
}

void MainWindow::appendEvbFolder(QTextStream &stream, const QDir &dir, const QString &virtualName, int depth)
{
    const QString pad = QString(depth * 2, QLatin1Char(' '));
    stream << pad << QStringLiteral("<File>\n");
    stream << pad << QStringLiteral("  <Type>3</Type>\n");
    stream << pad << QStringLiteral("  <Name>") << escapeXml(virtualName) << QStringLiteral("</Name>\n");
    stream << pad << QStringLiteral("  <Files>\n");

    //当前目录下的普通文件（跳过临时evb工程文件与已生成的单文件自身）
    const QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    for(const QFileInfo &fileInfo: fileList)
    {
        //兼容新旧单文件命名：旧 xxx_Single.exe、新 xxx_Single-qt<版本>-<编译器>.exe，均不重复打入
        const QString fileName = fileInfo.fileName();
        if(fileName == QStringLiteral("enigma_temp.evb") || (fileName.contains(QStringLiteral("_Single")) && fileName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)))
        {
            continue;
        }
        //被占用改名让位留下的旧文件不参与封包：内容已被新文件替代，打入只会增大体积
        if(fileName.endsWith(QStringLiteral(".wdh-locked")))
        {
            continue;
        }
        const QString filePad = pad + QStringLiteral("    ");
        stream << filePad << QStringLiteral("<File>\n");
        stream << filePad << QStringLiteral("  <Type>2</Type>\n");
        stream << filePad << QStringLiteral("  <Name>") << escapeXml(fileInfo.fileName()) << QStringLiteral("</Name>\n");
        stream << filePad << QStringLiteral("  <File>") << escapeXml(QDir::toNativeSeparators(fileInfo.absoluteFilePath())) << QStringLiteral("</File>\n");
        stream << filePad << QStringLiteral("</File>\n");
    }

    //递归子目录，保持部署目录的相对结构
    const QFileInfoList subDirList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for(const QFileInfo &subDirInfo: subDirList)
    {
        //跳过运行时数据目录：历史记录db/导出文件等由程序动态生成删除，打包瞬间文件可能不存在或正被占用，
        //会导致 Enigma 报 "File does not exist"；且运行时写入不回写 Single，打入也无益
        if(subDirInfo.fileName().compare(QStringLiteral("data"), Qt::CaseInsensitive) == 0)
        {
            appendLog(QStringLiteral("ℹ️ 跳过运行时数据目录：%1").arg(subDirInfo.absoluteFilePath()));
            continue;
        }
        appendEvbFolder(stream, QDir(subDirInfo.absoluteFilePath()), subDirInfo.fileName(), depth + 1);
    }

    stream << pad << QStringLiteral("  </Files>\n");
    stream << pad << QStringLiteral("</File>\n");
}
