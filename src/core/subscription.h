#pragma once

#include "profile.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <functional>

namespace rl {
struct SubscriptionInfo {
    qint64 upload = -1, download = -1, total = -1;
    qint64 expire = -1;
    int updateIntervalHours = 12;
    QString title, supportUrl, webPageUrl;

    QJsonObject toJson() const;
    static SubscriptionInfo fromJson(const QJsonObject &o);
};

SubscriptionInfo parseSubscriptionHeaders(const QByteArray &userinfo, const QByteArray &interval,
                                          const QByteArray &title, const QByteArray &supportUrl,
                                          const QByteArray &webPageUrl);

QString maskUrl(const QString &url);

QList<Profile> mergeSubscription(const QList<Profile> &existing, const QList<Profile> &incoming,
                                 const QString &subscriptionId, const std::function<QString()> &newId);

}
