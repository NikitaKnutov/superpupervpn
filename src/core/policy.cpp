#include "policy.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace rl {
AdminPolicy parsePolicyJson(const QByteArray &json, QString *error)
{
    AdminPolicy p;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    const QJsonValue allowTun = doc.object().value(u"allowTun"_s);
    if (!doc.isObject() || !(allowTun.isUndefined() || allowTun.isBool())) {
        p.allowTun = false;
        if (error)
            *error = QCoreApplication::translate("Policy", "the policy file is invalid, TUN is disabled until it is fixed");
        return p;
    }
    p.allowTun = allowTun.toBool(true);
    return p;
}

QJsonArray helperCapabilities(const AdminPolicy &policy)
{
    QJsonArray caps;
    if (policy.allowTun)
        caps << u"tun"_s;
    caps << u"proxy"_s << u"testDelay"_s << u"diagnostics"_s << u"stopOtherVpns"_s;
    return caps;
}

std::optional<FieldError> checkPolicy(const AdminPolicy &policy, const QString &mode)
{
    if (mode == u"tun" && !policy.allowTun)
        return FieldError{FieldError::UnsupportedParam, u"mode"_s,
                          QCoreApplication::translate("Policy", "TUN is disabled by the administrator. Use System proxy mode")};
    return std::nullopt;
}

}
