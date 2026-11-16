#include "platformgui.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <memory>

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

QString updateAssetName(const QString &version)
{
    if (QFile::exists(u"/usr/bin/dnf"_s))
        return u"superpupervpn-%1-1.fc43.x86_64.rpm"_s.arg(version);
    return u"superpupervpn_%1-1_amd64.deb"_s.arg(version);
}

void installUpdate(const QString &path, QObject *context, UpdateInstallCallback done)
{
    auto *process = new QProcess(context);
    auto completed = std::make_shared<bool>(false);
    auto finish = [completed, done](UpdateInstallResult result, const QString &error) {
        if (*completed)
            return;
        *completed = true;
        done(result, error);
    };
    QObject::connect(process, &QProcess::errorOccurred, context, [process, finish](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        finish(UpdateInstallResult::Failed, process->errorString());
        process->deleteLater();
    });
    QObject::connect(process, &QProcess::finished, context, [process, finish](int code, QProcess::ExitStatus status) {
        const QString details = QString::fromUtf8(process->readAllStandardError()).trimmed().left(300);
        if (code == 0 && status == QProcess::NormalExit) {
            finish(UpdateInstallResult::Installed, {});
        } else {
            finish(UpdateInstallResult::Failed,
                   details.isEmpty() ? QObject::tr("The package installer failed") : details);
        }
        process->deleteLater();
    });
    process->setProgram(u"pkexec"_s);
    process->setArguments(path.endsWith(u".rpm"_s) ? QStringList{u"dnf"_s, u"-y"_s, u"install"_s, path}
                                                   : QStringList{u"apt-get"_s, u"-y"_s, u"install"_s, path});
    process->start();
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
