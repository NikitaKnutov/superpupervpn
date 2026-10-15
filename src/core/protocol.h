#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace rl {
inline QString devOverride(const char *env, const QString &fallback)
{
#ifdef SUPERPUPERVPN_DEV
    const QString v = qEnvironmentVariable(env);
    return v.isEmpty() ? fallback : v;
#else
    Q_UNUSED(env);
    return fallback;
#endif
}

}

namespace rl::ipc {
constexpr int kProtocolMajor = 1;
constexpr qsizetype kMaxLineBytes = 8 * 1024 * 1024;

inline QString helperSocketName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("superpupervpn-helper");
#else
    return devOverride("SUPERPUPERVPN_SOCKET", QStringLiteral("/run/superpupervpn/helper.sock"));
#endif
}

inline QByteArray encode(const QJsonObject &o)
{
    return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n';
}

}
