#include "sysproxy.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <windows.h>
#include <wininet.h>
#include <ras.h>
#include <raserror.h>

#include <vector>

using namespace Qt::StringLiterals;

namespace rl::sysproxy {
namespace {
QStringList connections()
{
    QStringList out{QString()};
    DWORD size = 0, count = 0;
    if (RasEnumEntriesW(nullptr, nullptr, nullptr, &size, &count) == ERROR_BUFFER_TOO_SMALL && count) {
        std::vector<RASENTRYNAMEW> entries(count);
        entries[0].dwSize = sizeof(RASENTRYNAMEW);
        if (RasEnumEntriesW(nullptr, nullptr, entries.data(), &size, &count) == ERROR_SUCCESS)
            for (DWORD i = 0; i < count; ++i)
                out << QString::fromWCharArray(entries[i].szEntryName);
    }
    return out;
}

QString take(LPWSTR s)
{
    const QString v = s ? QString::fromWCharArray(s) : QString();
    if (s)
        GlobalFree(s);
    return v;
}

QJsonObject query(const QString &name)
{
    INTERNET_PER_CONN_OPTIONW opts[4] = {};
    opts[0].dwOption = INTERNET_PER_CONN_FLAGS;
    opts[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    opts[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
    opts[3].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;
    std::wstring conn = name.toStdWString();
    INTERNET_PER_CONN_OPTION_LISTW list{sizeof(list), name.isEmpty() ? nullptr : conn.data(), 4, 0, opts};
    DWORD size = sizeof(list);
    if (!InternetQueryOptionW(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, &size))
        return {};
    return {{u"name"_s, name}, {u"flags"_s, qint64(opts[0].Value.dwValue)}, {u"server"_s, take(opts[1].Value.pszValue)},
            {u"bypass"_s, take(opts[2].Value.pszValue)}, {u"autoconfig"_s, take(opts[3].Value.pszValue)}};
}

bool setOne(const QString &name, DWORD flags, const QString &server, const QString &bypass, const QString &autoconfig)
{
    std::wstring conn = name.toStdWString(), s = server.toStdWString(), b = bypass.toStdWString(), a = autoconfig.toStdWString();
    INTERNET_PER_CONN_OPTIONW opts[4] = {};
    opts[0].dwOption = INTERNET_PER_CONN_FLAGS;
    opts[0].Value.dwValue = flags;
    opts[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    opts[1].Value.pszValue = server.isEmpty() ? nullptr : s.data();
    opts[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
    opts[2].Value.pszValue = bypass.isEmpty() ? nullptr : b.data();
    opts[3].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;
    opts[3].Value.pszValue = autoconfig.isEmpty() ? nullptr : a.data();
    INTERNET_PER_CONN_OPTION_LISTW list{sizeof(list), name.isEmpty() ? nullptr : conn.data(), 4, 0, opts};
    return InternetSetOptionW(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, sizeof(list));
}

void notifyChanged()
{
    InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
    InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
}

QString bypassList()
{
    QStringList b = {u"localhost"_s, u"127.*"_s, u"10.*"_s, u"192.168.*"_s, u"169.254.*"_s};
    for (int i = 16; i <= 31; ++i)
        b << u"172.%1.*"_s.arg(i);
    b << u"<local>"_s;
    return b.join(u';');
}

}

QJsonObject capture()
{
    QJsonArray conns;
    for (const QString &c : connections())
        if (const QJsonObject o = query(c); !o.isEmpty())
            conns << o;
    return {{u"connections"_s, conns}};
}

bool set(int port, QString *error)
{
    bool ok = true;
    for (const QString &c : connections())
        ok = setOne(c, PROXY_TYPE_DIRECT | PROXY_TYPE_PROXY, u"127.0.0.1:%1"_s.arg(port), bypassList(), QString()) && ok;
    notifyChanged();
    if (!ok)
        *error = QCoreApplication::translate("SysProxy", "WinINet rejected the proxy settings");
    return ok;
}

bool restore(const QJsonObject &snapshot, QString *error)
{
    bool ok = true;
    for (const QJsonValue v : snapshot.value(u"connections"_s).toArray()) {
        const QJsonObject o = v.toObject();
        ok = setOne(o.value(u"name"_s).toString(), DWORD(o.value(u"flags"_s).toInteger()), o.value(u"server"_s).toString(),
                    o.value(u"bypass"_s).toString(), o.value(u"autoconfig"_s).toString()) && ok;
    }
    notifyChanged();
    if (!ok)
        *error = QCoreApplication::translate("SysProxy", "failed to restore the previous proxy settings");
    return ok;
}

bool pointsTo(int port)
{
    const QJsonObject lan = query(QString());
    return (DWORD(lan.value(u"flags"_s).toInteger()) & PROXY_TYPE_PROXY)
        && lan.value(u"server"_s).toString() == u"127.0.0.1:%1"_s.arg(port);
}

}
