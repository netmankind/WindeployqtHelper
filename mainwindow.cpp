#include "mainwindow.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    deployProcess = new QProcess(this);

    connect(deployProcess, &QProcess::finished, this, &MainWindow::onDeployFinished);
    connect(deployProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onDeployReadyReadStdout);
    connect(deployProcess, &QProcess::readyReadStandardError, this, &MainWindow::onDeployReadyReadStderr);

    loadSettings();
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::appendLog(const QString &msg)
{
    QString timeStamp = QDateTime::currentDateTime().toString("[yyyy-MM-dd HH:mm:ss] ");
    ui->txtLog->appendPlainText(timeStamp + msg);
}

bool MainWindow::loadSettings()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    settings.beginGroup("Paths");
    ui->edtExePath->setText(settings.value("exePath", "").toString());
    ui->edtQtBin->setText(settings.value("qtBin", "").toString());
    ui->edtOutputDir->setText(settings.value("outputDir", "").toString());
    ui->edtQmlDir->setText(settings.value("qmlDir", "").toString());
    ui->edtEnigmaVbPath->setText(settings.value("enigmaVbConsole", "").toString());
    settings.endGroup();

    settings.beginGroup("Options");
    ui->chkCleanOld->setChecked(settings.value("chkCleanOld", false).toBool());
    ui->chkMultimedia->setChecked(settings.value("chkMultimedia", false).toBool());
    ui->chkZip->setChecked(settings.value("chkZip", false).toBool());
    ui->chkSlimPlugin->setChecked(settings.value("chkSlimPlugin", false).toBool());
    ui->edtCustomKeepPlugins->setText(settings.value("customKeepPlugins", "").toString());
    ui->chkCopyVcRuntime->setChecked(settings.value("chkCopyVcRuntime", false).toBool());
    ui->edtVersion->setText(settings.value("version", "1.0.0").toString());
    ui->edtExtraDlls->setText(settings.value("extraDlls", "").toString());
    ui->chkEnigmaPack->setChecked(settings.value("chkEnigmaPack", false).toBool());
    ui->edtKeepFiles->setText(settings.value("keepFiles", "config.ini").toString());

    settings.endGroup();
    return true;
}

bool MainWindow::saveSettings()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    settings.beginGroup("Paths");
    settings.setValue("exePath", ui->edtExePath->text());
    settings.setValue("qtBin", ui->edtQtBin->text());
    settings.setValue("outputDir", ui->edtOutputDir->text());
    settings.setValue("qmlDir", ui->edtQmlDir->text());
    settings.setValue("enigmaVbConsole", ui->edtEnigmaVbPath->text());
    settings.endGroup();

    settings.beginGroup("Options");
    settings.setValue("chkCleanOld", ui->chkCleanOld->isChecked());
    settings.setValue("chkMultimedia", ui->chkMultimedia->isChecked());
    settings.setValue("chkZip", ui->chkZip->isChecked());
    settings.setValue("chkSlimPlugin", ui->chkSlimPlugin->isChecked());
    settings.setValue("customKeepPlugins", ui->edtCustomKeepPlugins->text());
    settings.setValue("chkCopyVcRuntime", ui->chkCopyVcRuntime->isChecked());
    settings.setValue("version", ui->edtVersion->text());
    settings.setValue("extraDlls", ui->edtExtraDlls->text());
    settings.setValue("chkEnigmaPack", ui->chkEnigmaPack->isChecked());
    settings.setValue("keepFiles", ui->edtKeepFiles->text());

    settings.endGroup();
    return true;
}

bool MainWindow::checkDeployInput()
{
    if(ui->edtExePath->text().trimmed().isEmpty() || !QFile::exists(ui->edtExePath->text()))
    {
        QMessageBox::warning(this, "校验失败", "目标EXE不存在！");
        return false;
    }
    if(ui->edtQtBin->text().trimmed().isEmpty() || !QDir(ui->edtQtBin->text()).exists())
    {
        QMessageBox::warning(this, "校验失败", "Qt bin目录不存在！");
        return false;
    }
    if(ui->edtOutputDir->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, "校验失败", "输出目录不能为空！");
        return false;
    }
    if(ui->chkEnigmaPack->isChecked())
    {
        QString enigmaPath = ui->edtEnigmaVbPath->text().trimmed();
        if(enigmaPath.isEmpty() || !QFile::exists(enigmaPath))
        {
            QMessageBox::warning(this, "校验失败", "已勾选Enigma打包，请选择有效的 enigmavbconsole.exe！");
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
            appendLog(QString("ℹ️ 跳过保留文件：%1（匹配规则）").arg(info.fileName()));
            continue;
        }

        if(info.isDir())
        {
            QDir subDir(info.absoluteFilePath());
            if(!subDir.removeRecursively())
            {
                appendLog(QString("⚠️ 删除子目录失败：%1").arg(info.absoluteFilePath()));
            }
        }
        else
        {
            if(!QFile::remove(info.absoluteFilePath()))
            {
                appendLog(QString("⚠️ 删除文件失败：%1").arg(info.absoluteFilePath()));
            }
        }
    }
    return true;
}

// 计算文件/目录选择对话框的起始目录：跟随输入框当前已填的路径
QString MainWindow::dialogStartDir(const QString &currentPath, bool currentIsDir) const
{
    QString startDir = QDir::homePath();
    QString path = currentPath.trimmed();
    if(!path.isEmpty())
    {
        QFileInfo info(path);
        // currentIsDir=true（目录选择框）直接用当前目录；否则（文件选择框）用其所在目录
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
    QString filePath = QFileDialog::getOpenFileName(this, "选择目标EXE", dialogStartDir(ui->edtExePath->text(), false), "Executable(*.exe)");
    if(!filePath.isEmpty())
    {
        ui->edtExePath->setText(filePath);
    }
}

void MainWindow::on_btnSelectQtBin_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择Qt bin目录", dialogStartDir(ui->edtQtBin->text(), true));
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
        appendLog("⚠️ 输出目录不存在");
    }
}

void MainWindow::on_btnSelectQmlDir_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择QML源码目录", dialogStartDir(ui->edtQmlDir->text(), true));
    if(!dir.isEmpty())
    {
        ui->edtQmlDir->setText(dir);
    }
}

void MainWindow::on_btnSelectEnigmaVb_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择 enigmavbconsole.exe", dialogStartDir(ui->edtEnigmaVbPath->text(), false), "Enigma Console(enigmavbconsole.exe)");
    if(!filePath.isEmpty())
    {
        ui->edtEnigmaVbPath->setText(filePath);
    }
}

void MainWindow::on_btnStartDeploy_clicked()
{
    if(deployProcess->state() != QProcess::NotRunning)
    {
        QMessageBox::information(this, "提示", "打包进程正在运行！");
        return;
    }
    if(!checkDeployInput())
    {
        return;
    }

    appendLog("===== 开始准备打包 =====");
    deployOutputBuffer.clear();
    QString exePath   = ui->edtExePath->text().trimmed();
    QString qtBinDir  = ui->edtQtBin->text().trimmed();
    QString outputDir = ui->edtOutputDir->text().trimmed();

    QString compilerType;
    detectQtCompiler(qtBinDir, compilerType);

    // 输出目录若与目标exe所在目录相同，源exe必须始终保留，防止清空旧目录时删掉原文件
    QFileInfo exeInfo(exePath);
    QDir outDir(outputDir);
    if(ui->chkCleanOld->isChecked())
    {
        appendLog("⚠️ 清空旧输出目录（保留目录本身，跳过配置的保留文件）");
        QString     rawKeep  = ui->edtKeepFiles->text().trimmed();
        QStringList keepList = rawKeep.split(",", Qt::SkipEmptyParts);
        keepList << exeInfo.fileName();
        clearDirectoryKeepSelf(outDir, keepList);
    }
    outDir.mkpath(".");

    // 拷贝exe到输出目录：目标与源为同一文件（输出目录=源exe目录）时跳过删除与复制，避免删掉原exe
    QString destExe = outDir.filePath(exeInfo.fileName());
    if(exeInfo.canonicalFilePath() == QFileInfo(destExe).canonicalFilePath())
    {
        appendLog(QString("ℹ️ 输出目录与源exe目录相同，主程序已就位，无需复制：%1").arg(destExe));
    }
    else
    {
        QFile::remove(destExe);
        if(!QFile::copy(exePath, destExe))
        {
            appendLog(QString("❌ 复制主程序到输出目录失败：%1").arg(destExe));
            return;
        }
        appendLog(QString("✅ 复制主程序到输出目录：%1").arg(destExe));
    }

    QString     windeployqt = QDir(qtBinDir).filePath("windeployqt.exe");
    QStringList args;
    args << destExe;
    if(ui->chkMultimedia->isChecked())
    {
        args << "--multimedia";
    }
    if(!ui->edtQmlDir->text().trimmed().isEmpty())
    {
        args << "--qmldir" << ui->edtQmlDir->text().trimmed();
    }
    if(ui->chkSlimPlugin->isChecked())
    {
        args << "--no-plugins";
    }

    appendLog(QString("执行 windeployqt: %1 %2").arg(windeployqt, args.join(" ")));
    deployProcess->setProgram(windeployqt);
    deployProcess->setArguments(args);
    deployProcess->setWorkingDirectory(qtBinDir);
    deployProcess->start();
}

void MainWindow::on_btnStopDeploy_clicked()
{
    if(deployProcess->state() != QProcess::NotRunning)
    {
        deployProcess->kill();
        appendLog("⚠️ 打包进程已终止");
    }
}

void MainWindow::onDeployFinished(int exitCode, QProcess::ExitStatus status)
{
    if(status == QProcess::NormalExit && exitCode == 0)
    {
        appendLog("✅ windeployqt执行完成");
        QString outputDir = ui->edtOutputDir->text().trimmed();
        if(ui->chkEnigmaPack->isChecked())
        {
            QString enigmaConsolePath = ui->edtEnigmaVbPath->text().trimmed();
            // 以界面填写的目标EXE为准确定主程序与Single命名（目录内存在其它exe时不再取错）
            QString targetExe         = QDir(outputDir).filePath(QFileInfo(ui->edtExePath->text().trimmed()).fileName());
            runEnigmaVbPack(outputDir, enigmaConsolePath, targetExe);
        }
    }
    else
    {
        appendLog(QString("❌ windeployqt失败 exitCode=%1").arg(exitCode));
        // 常见失败原因提示：目标exe与所选Qt Bin版本/编译器不匹配
        if(deployOutputBuffer.contains("does not seem to be a Qt executable", Qt::CaseInsensitive))
        {
            appendLog("⚠️ 提示：目标exe与所选“Qt Bin目录”版本/编译器不匹配（例如用Qt6的windeployqt部署Qt5程序，或MinGW版windeployqt部署MSVC程序）。");
            appendLog("   请将Qt Bin目录切换为目标exe实际使用的Qt版本与编译器对应的bin目录（含windeployqt.exe），并重新打包。");
        }
    }
}

void MainWindow::onDeployReadyReadStdout()
{
    QString out = deployProcess->readAllStandardOutput();
    deployOutputBuffer.append(out);
    appendLog(out);
}

void MainWindow::onDeployReadyReadStderr()
{
    QString err = deployProcess->readAllStandardError();
    deployOutputBuffer.append(err);
    appendLog(err);
}

void MainWindow::on_btnSaveConfig_clicked()
{
    saveSettings();
    appendLog("✅ 配置已保存到 config.ini");
}

void MainWindow::on_btnLoadConfig_clicked()
{
    loadSettings();
    appendLog("✅ 已加载 config.ini");
}

void MainWindow::on_btnClearLog_clicked()
{
    ui->txtLog->clear();
}

void MainWindow::on_btnSaveLog_clicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, "保存日志", "deploy_log.txt", "Text(*.txt)");
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
        appendLog(QString("✅ 日志保存至 %1").arg(filePath));
    }
    else
    {
        appendLog("❌ 日志保存失败");
    }
}

bool MainWindow::detectQtCompiler(const QString &qtBinDir, QString &compilerType)
{
    compilerType      = "Unknown";
    QString qmakePath = QDir(qtBinDir).filePath("qmake.exe");
    if(!QFile::exists(qmakePath))
    {
        appendLog("⚠️ qmake.exe不存在，无法识别编译器");
        return false;
    }
    QProcess proc;
    proc.setProgram(qmakePath);
    proc.setArguments({"-v"});
    proc.start();
    if(!proc.waitForFinished(3000))
    {
        appendLog("⚠️ qmake -v 执行超时");
        proc.kill();
        return false;
    }
    QString output = proc.readAllStandardOutput();
    appendLog("qmake -v 输出:\n" + output);

    if(output.contains("MSVC", Qt::CaseInsensitive) || output.contains("Microsoft Visual Studio", Qt::CaseInsensitive))
    {
        compilerType = "MSVC";
    }
    else if(output.contains("MinGW", Qt::CaseInsensitive) || output.contains("g++", Qt::CaseInsensitive))
    {
        compilerType = "MinGW";
    }
    appendLog(QString("✅ 识别编译器类型：%1").arg(compilerType));
    return true;
}

bool MainWindow::runEnigmaVbPack(const QString &sourceDir, const QString &enigmaConsolePath, const QString &targetExe)
{
    appendLog("\n===== Enigma Virtual Box Console 开始封包单文件 =====");
    QDir srcDir(sourceDir);

    // 主程序必须以界面选择的目标EXE为准：Single文件名 = 目标EXE基名 + "_Single.exe"
    // 目录里若存在其它exe（如旧版本exe、附带工具），枚举第一个exe会取错主程序与命名，故不能再用exeList.first()
    QString mainExe;
    QString exeBaseName;
    if(!targetExe.isEmpty() && QFile::exists(targetExe))
    {
        mainExe     = targetExe;
        exeBaseName = QFileInfo(mainExe).completeBaseName();
        appendLog(QString("✅ 主程序(目标EXE)：%1").arg(mainExe));
    }
    else
    {
        // 兼容旧行为：未传入目标exe时回退取目录内第一个exe（含多个exe时可能取错，仅作兜底）
        QFileInfoList exeList = srcDir.entryInfoList(QStringList() << "*.exe", QDir::Files);
        if(exeList.isEmpty())
        {
            appendLog("❌ 源目录找不到exe主程序，终止Enigma打包");
            return false;
        }
        mainExe     = exeList.first().absoluteFilePath();
        exeBaseName = exeList.first().completeBaseName();
        appendLog(QString("⚠️ 未找到目标EXE，回退使用目录内第一个exe：%1").arg(mainExe));
    }
    QString singleExePath = srcDir.filePath(exeBaseName + "_Single.exe");
    QString evbPath       = srcDir.filePath("enigma_temp.evb");

    QFile::remove(evbPath);
    QFile evbFile(evbPath);
    if(!evbFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        appendLog(QString("❌ 无法创建evb工程文件：%1").arg(evbPath));
        appendLog(QString("打开失败原因：%1").arg(evbFile.errorString()));
        return false;
    }

    // ==================================================================
    // Enigma Virtual Box 11.30 的 .evb 工程文件是 XML 格式（非 INI）。
    // 旧版 INI 模板已被官方控制台拒绝（"Project file is invalid!"），
    // 这里保留注释以便追溯：
    //
    //   QString evbTemplate = R"([Enigma Project]
    // MainExe=%1
    // OutputFile=%2
    // CompressFiles=1
    // DeleteExtractedOnExit=0
    // VirtualizeFiles=1
    // [Files]
    // AddFolderRecursive=%3
    // )";
    //   QString evbContent  = evbTemplate.arg(
    //       QDir::toNativeSeparators(mainExe),
    //       QDir::toNativeSeparators(singleExePath),
    //       QDir::toNativeSeparators(sourceDir));
    //   evbFile.write(evbContent.toUtf8());
    // ==================================================================

    QTextStream evbStream(&evbFile);
    evbStream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    evbStream << "<Project>\n";
    evbStream << "  <InputFile>" << escapeXml(QDir::toNativeSeparators(mainExe)) << "</InputFile>\n";
    evbStream << "  <OutputFile>" << escapeXml(QDir::toNativeSeparators(singleExePath)) << "</OutputFile>\n";
    evbStream << "  <Files>\n";
    evbStream << "    <Enabled>True</Enabled>\n";
    evbStream << "    <DeleteExtractedOnExit>False</DeleteExtractedOnExit>\n";
    evbStream << "    <CompressFiles>True</CompressFiles>\n";
    evbStream << "    <Files>\n";
    appendEvbFolder(evbStream, srcDir, QStringLiteral("%DEFAULT FOLDER%"), 1);
    evbStream << "    </Files>\n";
    evbStream << "  </Files>\n";
    evbStream << "</Project>\n";
    evbStream.flush();
    evbFile.close();
    appendLog(QString("✅ 生成临时evb：%1").arg(evbPath));

    QProcess    enigmaProc;
    QStringList args;
    args << evbPath;
    enigmaProc.setProgram(enigmaConsolePath);
    enigmaProc.setArguments(args);
    enigmaProc.setProcessChannelMode(QProcess::MergedChannels);

    appendLog(QString("执行EnigmaVB Console: %1 %2").arg(enigmaConsolePath, args.join(" ")));
    enigmaProc.start();
    if(!enigmaProc.waitForFinished(-1))
    {
        appendLog("❌ EnigmaVB执行超时");
        enigmaProc.kill();
        QFile::remove(evbPath);
        return false;
    }
    QString out = enigmaProc.readAllStandardOutput();
    if(!out.isEmpty())
    {
        appendLog("Enigma Output:\n" + out);
    }

    bool ok = false;
    if(enigmaProc.exitCode() == 0 && QFile::exists(singleExePath))
    {
        appendLog(QString("✅ Enigma打包成功！单文件输出：%1").arg(singleExePath));
        ok = true;
    }
    else
    {
        appendLog(QString("❌ EnigmaVB失败，exitCode=%1").arg(enigmaProc.exitCode()));
    }

    QFile::remove(evbPath);
    appendLog("已删除临时evb工程文件");
    return ok;
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
    stream << pad << "<File>\n";
    stream << pad << "  <Type>3</Type>\n";
    stream << pad << "  <Name>" << escapeXml(virtualName) << "</Name>\n";
    stream << pad << "  <Files>\n";

    // 当前目录下的普通文件（跳过临时evb工程文件自身）
    const QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    for(const QFileInfo &fileInfo : fileList)
    {
        if(fileInfo.fileName() == "enigma_temp.evb" || fileInfo.fileName().endsWith("_Single.exe"))
        {
            continue;
        }
        const QString filePad = pad + "    ";
        stream << filePad << "<File>\n";
        stream << filePad << "  <Type>2</Type>\n";
        stream << filePad << "  <Name>" << escapeXml(fileInfo.fileName()) << "</Name>\n";
        stream << filePad << "  <File>" << escapeXml(QDir::toNativeSeparators(fileInfo.absoluteFilePath())) << "</File>\n";
        stream << filePad << "</File>\n";
    }

    // 递归子目录，保持部署目录的相对结构
    const QFileInfoList subDirList = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for(const QFileInfo &subDirInfo : subDirList)
    {
        appendEvbFolder(stream, QDir(subDirInfo.absoluteFilePath()), subDirInfo.fileName(), depth + 1);
    }

    stream << pad << "  </Files>\n";
    stream << pad << "</File>\n";
}
