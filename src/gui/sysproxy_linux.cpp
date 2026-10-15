#include "sysproxy.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace rl::sysproxy {
namespace {
const QList<std::pair<QString, QString>> kGnomeKeys = {
    {u"org.gnome.system.proxy"_s, u"mode"_s},           {u"org.gnome.system.proxy"_s, u"ignore-hosts"_s},
    {u"org.gnome.system.proxy.http"_s, u"host"_s},      {u"org.gnome.system.proxy.http"_s, u"port"_s},
    {u"org.gnome.system.proxy.https"_s, u"host"_s},     {u"org.gnome.system.proxy.https"_s, u"port"_s},
    {u"org.gnome.system.proxy.socks"_s, u"host"_s},     {u"org.gnome.system.proxy.socks"_s, u"port"_s},
};

QString run(const QString &program, const QStringList &args, bool *ok = nullptr)
{
    QProcess p;
    p.start(program, args);
    const bool done = p.waitForFinished(5000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    if (ok)
        *ok = done;
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

bool isKde()
{
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(u"KDE"_s, Qt::CaseInsensitive);
}

QString kioslaverc()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/kioslaverc"_s;
}

QString kwriteconfig()
{
    return QStandardPaths::findExecutable(u"kwriteconfig6"_s).isEmpty() ? u"kwriteconfig5"_s : u"kwriteconfig6"_s;
}

void kdeReparse()
{
    run(u"dbus-send"_s, {u"--type=signal"_s, u"/KIO/Scheduler"_s,
                         u"org.kde.KIO.Scheduler.reparseSlaveConfiguration"_s, u"string:"_s});
}

bool hasGsettings()
{
    return !QStandardPaths::findExecutable(u"gsettings"_s).isEmpty();
}

}

QJsonObject capture()
{
    QJsonObject gnome;
    if (hasGsettings())
        for (const auto &[schema, key] : kGnomeKeys)
            gnome[schema + u' ' + key] = run(u"gsettings"_s, {u"get"_s, schema, key});
    QJsonObject snap{{u"gnome"_s, gnome}};
    if (isKde()) {
        QFile f(kioslaverc());
        snap[u"kioslaverc"_s] = f.open(QIODevice::ReadOnly) ? QString::fromLatin1(f.readAll().toBase64()) : QString();
        snap[u"kioslavercExists"_s] = f.exists();
    }
    return snap;
}

bool set(int port, QString *error)
{
    const QString p = QString::number(port);
    if (hasGsettings()) {
        bool ok = true;
        for (const QString &schema : {u"org.gnome.system.proxy.http"_s, u"org.gnome.system.proxy.https"_s,
                                     u"org.gnome.system.proxy.socks"_s}) {
            bool a = false, b = false;
            run(u"gsettings"_s, {u"set"_s, schema, u"host"_s, u"127.0.0.1"_s}, &a);
            run(u"gsettings"_s, {u"set"_s, schema, u"port"_s, p}, &b);
            ok = ok && a && b;
        }
        run(u"gsettings"_s, {u"set"_s, u"org.gnome.system.proxy"_s, u"ignore-hosts"_s,
                             u"['localhost', '127.0.0.0/8', '::1', '10.0.0.0/8', '172.16.0.0/12', '192.168.0.0/16', '169.254.0.0/16']"_s});
        bool m = false;
        run(u"gsettings"_s, {u"set"_s, u"org.gnome.system.proxy"_s, u"mode"_s, u"manual"_s}, &m);
        if (!ok || !m) {
            *error = u"gsettings не принял настройки прокси"_s;
            return false;
        }
    }
    if (isKde()) {
        const QString kw = kwriteconfig();
        const QStringList base = {u"--file"_s, u"kioslaverc"_s, u"--group"_s, u"Proxy Settings"_s, u"--key"_s};
        run(kw, base + QStringList{u"httpProxy"_s, u"http://127.0.0.1 "_s + p});
        run(kw, base + QStringList{u"httpsProxy"_s, u"http://127.0.0.1 "_s + p});
        run(kw, base + QStringList{u"socksProxy"_s, u"socks://127.0.0.1 "_s + p});
        run(kw, base + QStringList{u"NoProxyFor"_s, u"localhost,127.0.0.1,::1"_s});
        run(kw, base + QStringList{u"ProxyType"_s, u"1"_s});
        kdeReparse();
    }
    if (!hasGsettings() && !isKde()) {
        *error = u"не найдено ни gsettings, ни KDE: настройте прокси вручную"_s;
        return false;
    }
    return true;
}

bool restore(const QJsonObject &snapshot, QString *error)
{
    bool ok = true;
    const QJsonObject gnome = snapshot.value(u"gnome"_s).toObject();
    for (auto it = kGnomeKeys.crbegin(); it != kGnomeKeys.crend(); ++it) {
        const QString v = gnome.value(it->first + u' ' + it->second).toString();
        if (v.isEmpty())
            continue;
        bool done = false;
        run(u"gsettings"_s, {u"set"_s, it->first, it->second, v}, &done);
        ok = ok && done;
    }
    if (snapshot.contains(u"kioslavercExists"_s)) {
        if (snapshot.value(u"kioslavercExists"_s).toBool()) {
            QSaveFile f(kioslaverc());
            ok = ok && f.open(QIODevice::WriteOnly)
                && f.write(QByteArray::fromBase64(snapshot.value(u"kioslaverc"_s).toString().toLatin1())) >= 0 && f.commit();
        } else {
            QFile::remove(kioslaverc());
        }
        kdeReparse();
    }
    if (!ok)
        *error = u"не удалось вернуть прежние настройки прокси"_s;
    return ok;
}

bool pointsTo(int port)
{
    if (hasGsettings())
        return run(u"gsettings"_s, {u"get"_s, u"org.gnome.system.proxy"_s, u"mode"_s}) == u"'manual'"
            && run(u"gsettings"_s, {u"get"_s, u"org.gnome.system.proxy.http"_s, u"host"_s}) == u"'127.0.0.1'"
            && run(u"gsettings"_s, {u"get"_s, u"org.gnome.system.proxy.http"_s, u"port"_s}) == QString::number(port);
    QFile f(kioslaverc());
    return f.open(QIODevice::ReadOnly) && f.readAll().contains("httpProxy=http://127.0.0.1 " + QByteArray::number(port));
}

}
