#include "version.h"

#include <QLocale>

QString Version::getSemanticVersion()
{
    // 版本号唯一手工维护点：升级主版本只改这一行，日期段由编译时间自动生成
    static const QString SEMANTIC_VERSION = QStringLiteral("1.0.0");
    return SEMANTIC_VERSION;
}

QDateTime Version::getBuildDateTime()
{
    // __DATE__ / __TIME__ 是编译器处理本文件时的时刻；version.o 在每次构建后会被删除，
    // 下次构建必然重编本文件，所以该时间戳就是本次编译时间。
    QString dateTime = QStringLiteral("%1 %2").arg(__DATE__, __TIME__);

    // replace() 的作用是把日期补成两位：__DATE__ 在日小于 10 时形如 "Nov  6 2026"，
    // 把其中的双空格替换为“空格+0”即得到 "Nov 06 2026"；日已是两位时没有双空格，不受影响
    dateTime.replace("  ", " 0");
    QDateTime dt = QLocale(QLocale::English).toDateTime(dateTime, "MMM dd yyyy HH:mm:ss");

    return dt;
}

QString Version::getDisplayVersion()
{
    const QString buildStamp = getBuildDateTime().toString(QStringLiteral("yyyyMMdd-HHmm"));
    return QStringLiteral("%1.%2").arg(getSemanticVersion(), buildStamp);
}
