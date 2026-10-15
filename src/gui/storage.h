#pragma once

#include "profile.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>

namespace rl {
class Storage {
public:
    static constexpr int kSchema = 1;

    bool load(QString *error);
    bool saveSettings();
    bool saveProfiles();
    QString dir() const;

    QJsonObject settings;
    QList<Profile> profiles;
    QJsonArray subscriptions;
    QString warning;
    bool readOnly = false;

    static QJsonObject defaults();

private:
    QByteArray m_key;
};

QByteArray loadOrCreateKey(const QString &dir, QString *warning);

}
