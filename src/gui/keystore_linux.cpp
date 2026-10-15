#include "storage.h"

#include <QFile>
#include <QSaveFile>

#include <openssl/rand.h>

#undef signals
#include <libsecret/secret.h>

using namespace Qt::StringLiterals;

namespace rl {
namespace {
const SecretSchema *schema()
{
    static const SecretSchema s = {"org.superpupervpn.ProfilesKey", SECRET_SCHEMA_NONE,
                                   {{"app", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, SecretSchemaAttributeType(0)}},
                                   0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    return &s;
}

QByteArray randomKey()
{
    QByteArray k(32, Qt::Uninitialized);
    RAND_bytes(reinterpret_cast<unsigned char *>(k.data()), 32);
    return k;
}

}

QByteArray loadOrCreateKey(const QString &dir, QString *warning)
{
    const QString fallback = dir + u"/key"_s;
    const QString fallbackWarning = u"Связка ключей недоступна: ключ шифрования хранится в файле с правами 0600"_s;
    if (QFile f(fallback); f.open(QIODevice::ReadOnly)) {
        *warning = fallbackWarning;
        return QByteArray::fromBase64(f.readAll());
    }

    GError *err = nullptr;
    gchar *stored = secret_password_lookup_sync(schema(), nullptr, &err, "app", "superpupervpn", nullptr);
    if (!err && stored) {
        const QByteArray key = QByteArray::fromBase64(stored);
        secret_password_free(stored);
        if (key.size() == 32)
            return key;
    }
    if (!err) {
        const QByteArray key = randomKey();
        if (secret_password_store_sync(schema(), SECRET_COLLECTION_DEFAULT, "SuperPuperVPN", key.toBase64().constData(),
                                       nullptr, &err, "app", "superpupervpn", nullptr))
            return key;
    }
    if (err)
        g_error_free(err);

    const QByteArray key = randomKey();
    QSaveFile f(fallback);
    if (!f.open(QIODevice::WriteOnly))
        return {};
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    f.write(key.toBase64());
    if (!f.commit())
        return {};
    *warning = fallbackWarning;
    return key;
}

}
