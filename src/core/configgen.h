#pragma once

#include "profile.h"

#include <QJsonObject>
#include <QList>

namespace rl {
struct CoreSettings {
    QString mode = QStringLiteral("tun");
    QString dohUrl = QStringLiteral("https://1.1.1.1/dns-query");
    int tunMtu = 0;
    QString logLevel = QStringLiteral("warning");

    QJsonObject toJson() const;
    static CoreSettings fromJson(const QJsonObject &o);
};
std::optional<FieldError> validate(const CoreSettings &s);

struct CoreRuntime {
    QString controllerUnix;
    QString controllerPipe;
    QString secret;
    int proxyPort = 0;
    bool autoRedirect = false;
};

inline constexpr char16_t kDirectDns[] = u"77.88.8.8";

QJsonObject buildCoreConfig(const Profile &p, const CoreSettings &s, const CoreRuntime &rt);
QJsonObject buildTestConfig(const QList<Profile> &profiles, const CoreRuntime &rt, const QString &interfaceName);
QJsonObject proxyEntry(const Profile &p, const QString &name);

}
