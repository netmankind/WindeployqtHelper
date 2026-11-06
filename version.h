#pragma once

#include <QDateTime>
#include <QString>

/**
 * @brief 版本信息：手工维护的主版本号 + 自动刷新的编译时间戳
 *
 * 编译时间来自 __DATE__/__TIME__，因此只有在 version.cpp 被重新编译时才变化。
 * .pro 中启用了 config.pri 的 version_stamp 机制：每次构建后删除 version.o，
 * 下次构建必然重编本文件，从而实现“每次编译自动更新版本日期”。
 */
class Version
{
public:
    /**
     * @brief 手工维护的主版本号（如 1.0.0），发版时只需修改 version.cpp 中的常量
     */
    static QString getSemanticVersion();

    /**
     * @brief 本二进制的编译时间（yyyyMMdd-HHmm 即可表示到分钟）
     */
    static QDateTime getBuildDateTime();

    /**
     * @brief 展示用完整版本号：主版本号 + 编译时间，形如 1.0.0.20261106-0928
     */
    static QString getDisplayVersion();
};
