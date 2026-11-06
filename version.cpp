#include "version.h"

#include <QLocale>

namespace
{
    // 主版本号常量：发版时只改这一处，日期段由编译时间自动生成
    const QString SEMANTIC_VERSION = QStringLiteral("1.0.0");
}

QString Version::getSemanticVersion()
{
    return SEMANTIC_VERSION;
}

QDateTime Version::getBuildDateTime()
{
    // __DATE__ / __TIME__ 是编译器处理本文件时的时刻；version.o 在每次构建后会被删除，
    // 下次构建必然重编本文件，所以该时间戳就是本次编译时间。
    // 两个宏是 const char*，工程启用了 QT_NO_CAST_FROM_ASCII，不能隐式转 QString，需显式 fromUtf8
    QString dateTime = QStringLiteral("%1 %2").arg(QString::fromUtf8(__DATE__), QString::fromUtf8(__TIME__));

    // replace() 的作用是把日期补成两位：__DATE__ 在日小于 10 时形如 "Nov  6 2026"，
    // 把其中的双空格替换为“空格+0”即得到 "Nov 06 2026"；日已是两位时没有双空格，不受影响
    dateTime.replace(QStringLiteral("  "), QStringLiteral(" 0"));
    QDateTime dt = QLocale(QLocale::English).toDateTime(dateTime, QStringLiteral("MMM dd yyyy HH:mm:ss"));

    return dt;
}

QString Version::getDisplayVersion()
{
    const QString buildStamp = getBuildDateTime().toString(QStringLiteral("yyyyMMdd-HHmm"));
    return QStringLiteral("%1.%2").arg(SEMANTIC_VERSION, buildStamp);
}
