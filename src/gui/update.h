#pragma once

#include <QJsonObject>
#include <QString>
#include <QUrl>

namespace rl {
struct UpdatePackage {
    QString version;
    QString name;
    QUrl url;
    QString sha256;
    qint64 size = 0;
};

UpdatePackage parseUpdatePackage(const QJsonObject &release, const QString &currentVersion, const QString &assetName);
bool verifyUpdatePackage(const QString &path, const UpdatePackage &package);

}
