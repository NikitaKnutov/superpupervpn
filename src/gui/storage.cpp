#include "storage.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <openssl/evp.h>
#include <openssl/rand.h>

using namespace Qt::StringLiterals;

namespace rl {
namespace {
const QByteArray kMagic = "RLP1";

QByteArray seal(const QByteArray &key, const QByteArray &plain)
{
    QByteArray nonce(12, Qt::Uninitialized), out(plain.size(), Qt::Uninitialized), tag(16, Qt::Uninitialized);
    RAND_bytes(reinterpret_cast<unsigned char *>(nonce.data()), int(nonce.size()));
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, fin = 0;
    const bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, reinterpret_cast<const unsigned char *>(key.constData()),
                                       reinterpret_cast<const unsigned char *>(nonce.constData()))
        && EVP_EncryptUpdate(ctx, nullptr, &len, reinterpret_cast<const unsigned char *>(kMagic.constData()), int(kMagic.size()))
        && EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(out.data()), &len,
                             reinterpret_cast<const unsigned char *>(plain.constData()), int(plain.size()))
        && EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(out.data()) + len, &fin)
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data());
    EVP_CIPHER_CTX_free(ctx);
    return ok ? kMagic + nonce + out + tag : QByteArray();
}

std::optional<QByteArray> unseal(const QByteArray &key, const QByteArray &blob)
{
    if (!blob.startsWith(kMagic) || blob.size() < kMagic.size() + 12 + 16)
        return std::nullopt;
    const QByteArray nonce = blob.mid(4, 12), tag = blob.right(16), ct = blob.mid(16, blob.size() - 32);
    QByteArray out(ct.size(), Qt::Uninitialized);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, fin = 0;
    const bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, reinterpret_cast<const unsigned char *>(key.constData()),
                                       reinterpret_cast<const unsigned char *>(nonce.constData()))
        && EVP_DecryptUpdate(ctx, nullptr, &len, reinterpret_cast<const unsigned char *>(kMagic.constData()), int(kMagic.size()))
        && EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char *>(out.data()), &len,
                             reinterpret_cast<const unsigned char *>(ct.constData()), int(ct.size()))
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<char *>(tag.constData()))
        && EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(out.data()) + len, &fin);
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        return std::nullopt;
    return out;
}

bool writeAtomic(const QString &path, const QByteArray &data)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    return f.write(data) == data.size() && f.commit();
}

void backupBeforeMigration(const QString &path, int fromVersion)
{
    QFile::copy(path, path + u".v%1.bak"_s.arg(fromVersion));
}

}

QJsonObject Storage::defaults()
{
    return {
        {u"version"_s, kSchema},
        {u"theme"_s, u"dark"_s},
        {u"language"_s, u"en"_s},
        {u"autostart"_s, false},
        {u"connectOnStart"_s, false},
        {u"closeToTray"_s, true},
        {u"noTrayClose"_s, u"ask"_s},
        {u"mode"_s, u"tun"_s},
        {u"subIntervalHours"_s, 0},
        {u"logLevel"_s, u"warning"_s},
        {u"dohUrl"_s, u"https://1.1.1.1/dns-query"_s},
        {u"mtu"_s, 0},
        {u"maskHosts"_s, true},
        {u"selectedProfileId"_s, QString()},
        {u"recentProfileIds"_s, QJsonArray()},
        {u"onboardingDone"_s, false},
        {u"sortMode"_s, u"name"_s},
        {u"collapsedGroups"_s, QJsonArray()},
        {u"ackConflicts"_s, QString()},
        {u"lastUpdateCheck"_s, 0},
    };
}

QString Storage::dir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

bool Storage::load(QString *error)
{
    QDir().mkpath(dir());
    QFile::setPermissions(dir(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    settings = defaults();
    const QString settingsPath = dir() + u"/settings.json"_s;
    if (QFile f(settingsPath); f.open(QIODevice::ReadOnly)) {
        const QJsonObject stored = QJsonDocument::fromJson(f.readAll()).object();
        const int v = stored.value(u"version"_s).toInt(1);
        if (v > kSchema)
            readOnly = true;
        for (auto it = stored.begin(); it != stored.end(); ++it)
            if (settings.contains(it.key()))
                settings[it.key()] = it.value();
        if (v < kSchema && !readOnly) {
            backupBeforeMigration(settingsPath, v);
            settings[u"version"_s] = kSchema;
            saveSettings();
        }
    }

    m_key = loadOrCreateKey(dir(), &warning);
    if (m_key.size() != 32) {
        *error = QCoreApplication::translate("Storage", "failed to get the encryption key");
        return false;
    }
    const QString profilesPath = dir() + u"/profiles.enc"_s;
    QFile f(profilesPath);
    if (!f.exists())
        return true;
    if (!f.open(QIODevice::ReadOnly)) {
        *error = QCoreApplication::translate("Storage", "no access to %1").arg(profilesPath);
        return false;
    }
    const auto plain = unseal(m_key, f.readAll());
    f.close();
    if (!plain) {
        const QString aside = profilesPath + u".unreadable-"_s + QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss"_s);
        QFile::rename(profilesPath, aside);
        warning = QCoreApplication::translate("Storage", "Could not decrypt the profiles; the file was saved as %1").arg(QDir::toNativeSeparators(aside));
        return true;
    }
    const QJsonObject o = QJsonDocument::fromJson(*plain).object();
    const int v = o.value(u"version"_s).toInt(1);
    if (v > kSchema)
        readOnly = true;
    for (const QJsonValue p : o.value(u"profiles"_s).toArray())
        profiles << Profile::fromJson(p.toObject());
    subscriptions = o.value(u"subscriptions"_s).toArray();
    if (v < kSchema && !readOnly) {
        backupBeforeMigration(profilesPath, v);
        saveProfiles();
    }
    if (readOnly)
        warning = QCoreApplication::translate("Storage", "The data was created by a newer SuperPuperVPN version; changes will not be saved");
    return true;
}

bool Storage::saveSettings()
{
    if (readOnly)
        return false;
    return writeAtomic(dir() + u"/settings.json"_s, QJsonDocument(settings).toJson());
}

bool Storage::saveProfiles()
{
    if (readOnly)
        return false;
    QJsonArray arr;
    for (const Profile &p : std::as_const(profiles))
        arr << p.toJson();
    const QJsonObject o{{u"version"_s, kSchema}, {u"profiles"_s, arr}, {u"subscriptions"_s, subscriptions}};
    const QByteArray blob = seal(m_key, QJsonDocument(o).toJson(QJsonDocument::Compact));
    return !blob.isEmpty() && writeAtomic(dir() + u"/profiles.enc"_s, blob);
}

}
