#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <optional>

namespace rl {
struct Profile {
    QString id;
    QString subscriptionId;
    QString name;
    QString host;
    int port = 0;
    QString uuid;
    QString network = QStringLiteral("tcp");
    QString encryption = QStringLiteral("none");
    QString flow;
    QString security = QStringLiteral("none");
    QString sni, fp, pbk, sid, spx;
    QString alpn;
    QString path, hostHeader, serviceName, mode;
    QString extra;
    QString ech, pcs, vcn;
    bool insecure = false;
    bool insecureConsent = false;
    bool pinned = false;
    QJsonObject unknownParams;

    bool nonStandardId() const;
    QString badge() const;
    QString identityKey() const;
    QStringList secrets() const;

    QJsonObject toJson() const;
    static Profile fromJson(const QJsonObject &o);
};

struct FieldError {
    enum Kind { Format, UnsupportedProtocol, UnsupportedParam };
    Kind kind = Format;
    QString field;
    QString reason;
    QString text() const;
};

std::optional<FieldError> validate(const Profile &p);
std::optional<FieldError> rejectUnknownKeys(const QJsonObject &profileJson);

bool isRfc4122Uuid(const QString &s);

}
