#include "subscription.h"

#include <QMultiHash>
#include <QUrl>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace rl {
namespace {
QString plain(const QString &s, int maxLen)
{
    QString out;
    for (QChar c : s)
        if (c.category() != QChar::Other_Control)
            out += c;
    return out.trimmed().left(maxLen);
}

}

SubscriptionInfo parseSubscriptionHeaders(const QByteArray &userinfo, const QByteArray &interval,
                                          const QByteArray &title, const QByteArray &supportUrl,
                                          const QByteArray &webPageUrl)
{
    SubscriptionInfo info;
    for (const QByteArray &part : userinfo.split(';')) {
        const qsizetype eq = part.indexOf('=');
        if (eq < 0)
            continue;
        const QByteArray key = part.left(eq).trimmed().toLower();
        bool ok = false;
        const qint64 v = part.mid(eq + 1).trimmed().toLongLong(&ok);
        if (!ok || v < 0)
            continue;
        if (key == "upload") info.upload = v;
        else if (key == "download") info.download = v;
        else if (key == "total") info.total = v;
        else if (key == "expire") info.expire = v > 32'000'000'000LL ? v / 1000 : v;
    }

    bool ok = false;
    const int hours = interval.trimmed().toInt(&ok);
    if (ok)
        info.updateIntervalHours = std::clamp(hours, 1, 168);

    QByteArray t = title.trimmed();
    if (t.startsWith("base64:"))
        t = QByteArray::fromBase64(t.mid(7));
    info.title = plain(QString::fromUtf8(t), 64);
    info.supportUrl = plain(QString::fromUtf8(supportUrl), 256);
    info.webPageUrl = plain(QString::fromUtf8(webPageUrl), 256);
    return info;
}

QJsonObject SubscriptionInfo::toJson() const
{
    return {{u"upload"_s, upload}, {u"download"_s, download}, {u"total"_s, total}, {u"expire"_s, expire},
            {u"updateIntervalHours"_s, updateIntervalHours}, {u"title"_s, title},
            {u"supportUrl"_s, supportUrl}, {u"webPageUrl"_s, webPageUrl}};
}

SubscriptionInfo SubscriptionInfo::fromJson(const QJsonObject &o)
{
    SubscriptionInfo i;
    i.upload = o.value(u"upload"_s).toInteger(-1);
    i.download = o.value(u"download"_s).toInteger(-1);
    i.total = o.value(u"total"_s).toInteger(-1);
    i.expire = o.value(u"expire"_s).toInteger(-1);
    i.updateIntervalHours = o.value(u"updateIntervalHours"_s).toInt(12);
    i.title = o.value(u"title"_s).toString();
    i.supportUrl = o.value(u"supportUrl"_s).toString();
    i.webPageUrl = o.value(u"webPageUrl"_s).toString();
    return i;
}

QList<Profile> mergeSubscription(const QList<Profile> &existing, const QList<Profile> &incoming,
                                 const QString &subscriptionId, const std::function<QString()> &newId)
{
    QMultiHash<QString, Profile> byKey;
    for (const Profile &p : existing)
        byKey.insert(p.identityKey(), p);
    QList<Profile> out;
    for (Profile p : incoming) {
        const QString key = p.identityKey();
        if (auto it = byKey.find(key); it != byKey.end()) {
            p.id = it->id;
            p.pinned = it->pinned;
            p.insecureConsent = it->insecureConsent && p.insecure;
            byKey.erase(it);
        } else {
            p.id = newId();
        }
        p.subscriptionId = subscriptionId;
        out << p;
    }
    return out;
}

QString maskUrl(const QString &url)
{
    const QUrl u(url);
    if (!u.isValid() || u.host().isEmpty())
        return u"***"_s;
    return u.scheme() + u"://"_s + u.host() + u"/***"_s;
}

}
