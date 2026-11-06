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
    // __DATE__ 形如 "Nov  6 2026"（日小于 10 时日期前有两个空格），先 simplified() 压缩；
    // 月名为英文，必须用 C locale 解析，系统中文 locale 会把 "Nov" 解析失败。
    static const QDateTime buildDateTime = []() {
        const QString raw = QString::fromLatin1(__DATE__ " " __TIME__).simplified();
        const QLocale cLocale(QLocale::C);
        return cLocale.toDateTime(raw, QStringLiteral("MMM d yyyy HH:mm:ss"));
    }();
    return buildDateTime;
}

QString Version::getDisplayVersion()
{
    const QString buildStamp = getBuildDateTime().toString(QStringLiteral("yyyyMMdd-HHmm"));
    return QStringLiteral("%1.%2").arg(getSemanticVersion(), buildStamp);
}
