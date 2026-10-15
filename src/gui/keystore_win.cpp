#include "storage.h"

#include <QFile>
#include <QSaveFile>

#include <windows.h>
#include <dpapi.h>
#include <openssl/rand.h>

using namespace Qt::StringLiterals;

namespace rl {
QByteArray loadOrCreateKey(const QString &dir, QString *warning)
{
    Q_UNUSED(warning);
    const QString path = dir + u"/key.dpapi"_s;
    if (QFile f(path); f.open(QIODevice::ReadOnly)) {
        QByteArray blob = f.readAll();
        DATA_BLOB in{DWORD(blob.size()), reinterpret_cast<BYTE *>(blob.data())}, out{};
        if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
            const QByteArray key(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
            SecureZeroMemory(out.pbData, out.cbData);
            LocalFree(out.pbData);
            return key;
        }
        return {};
    }
    QByteArray key(32, Qt::Uninitialized);
    RAND_bytes(reinterpret_cast<unsigned char *>(key.data()), 32);
    DATA_BLOB in{DWORD(key.size()), reinterpret_cast<BYTE *>(key.data())}, out{};
    if (!CryptProtectData(&in, L"SuperPuperVPN", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return {};
    QSaveFile f(path);
    const bool ok = f.open(QIODevice::WriteOnly)
        && f.write(reinterpret_cast<const char *>(out.pbData), out.cbData) == qint64(out.cbData) && f.commit();
    LocalFree(out.pbData);
    return ok ? key : QByteArray();
}

}
