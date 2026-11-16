#include "update.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QRegularExpression>
#include <QVersionNumber>

using namespace Qt::StringLiterals;

namespace rl {
UpdatePackage parseUpdatePackage(const QJsonObject &release, const QString &currentVersion, const QString &assetName)
{
    QString version = release.value(u"tag_name"_s).toString();
    if (version.startsWith(u'v'))
        version.remove(0, 1);
    if (QVersionNumber::fromString(version) <= QVersionNumber::fromString(currentVersion))
        return {};

    QJsonObject asset;
    for (const QJsonValue value : release.value(u"assets"_s).toArray()) {
        const QJsonObject candidate = value.toObject();
        if (candidate.value(u"name"_s).toString() == assetName) {
            asset = candidate;
            break;
        }
    }
    const qint64 size = asset.value(u"size"_s).toInteger(0);
    const QUrl url(asset.value(u"browser_download_url"_s).toString(), QUrl::StrictMode);
    const QString digest = asset.value(u"digest"_s).toString();
    static const QRegularExpression digestPattern(u"^sha256:([0-9a-f]{64})$"_s);
    const auto match = digestPattern.match(digest);
    if (size <= 0 || size > 256 * 1024 * 1024 || !url.isValid() || url.scheme() != u"https" || url.host() != u"github.com"_s
        || !match.hasMatch())
        return {};
    return {version, assetName, url, match.captured(1), size};
}

bool verifyUpdatePackage(const QString &path, const UpdatePackage &package)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() != package.size)
        return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) && hash.result().toHex() == package.sha256.toLatin1();
}

}
