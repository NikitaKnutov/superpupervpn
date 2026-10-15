#include "platformgui.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace rl::gui {
namespace {
QString autostartFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/autostart/superpupervpn.desktop"_s;
}

QString output(const QString &program, const QStringList &args)
{
    QProcess p;
    p.start(program, args);
    p.waitForFinished(3000);
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

}

void setAutostart(bool on)
{
    if (!on) {
        QFile::remove(autostartFile());
        return;
    }
    QDir().mkpath(QFileInfo(autostartFile()).path());
    QSaveFile f(autostartFile());
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write("[Desktop Entry]\nType=Application\nName=SuperPuperVPN\nExec=superpupervpn --autostart\nIcon=superpupervpn\n"
            "X-GNOME-Autostart-enabled=true\nNoDisplay=true\n");
    f.commit();
}

void registerUrlSchemes()
{
    for (const QString &scheme : {u"x-scheme-handler/vless"_s, u"x-scheme-handler/superpupervpn"_s})
        if (output(u"xdg-mime"_s, {u"query"_s, u"default"_s, scheme}).isEmpty())
            QProcess::startDetached(u"xdg-mime"_s, {u"default"_s, u"superpupervpn.desktop"_s, scheme});
}

bool startService(bool restart)
{
    return QProcess::startDetached(u"pkexec"_s, {u"systemctl"_s, restart ? u"restart"_s : u"start"_s, u"superpupervpn-helper.service"_s});
}

bool reduceMotion()
{
    if (output(u"gsettings"_s, {u"get"_s, u"org.gnome.desktop.interface"_s, u"enable-animations"_s}) == u"false")
        return true;
    QSettings kde(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/kdeglobals"_s, QSettings::IniFormat);
    return kde.value(u"KDE/AnimationDurationFactor"_s, 1.0).toDouble() == 0.0;
}

QString osName()
{
    return u"Linux"_s;
}

}
