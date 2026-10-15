#include "platformgui.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include <windows.h>
#include <shellapi.h>

using namespace Qt::StringLiterals;

namespace rl::gui {
namespace {
QString exe()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

}

void setAutostart(bool on)
{
    QSettings run(u"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"_s, QSettings::NativeFormat);
    if (on)
        run.setValue(u"SuperPuperVPN"_s, u"\"%1\" --autostart"_s.arg(exe()));
    else
        run.remove(u"SuperPuperVPN"_s);
}

void registerUrlSchemes()
{
    for (const QString &scheme : {u"vless"_s, u"superpupervpn"_s}) {
        QSettings s(u"HKEY_CURRENT_USER\\Software\\Classes\\"_s + scheme, QSettings::NativeFormat);
        const QString command = u"\"%1\" \"%2\""_s.arg(exe(), u"%1"_s);
        const QString current = s.value(u"shell/open/command/Default"_s).toString();
        if (!current.isEmpty() && !current.contains(u"superpupervpn"_s, Qt::CaseInsensitive))
            continue;
        s.setValue(u"Default"_s, u"URL:%1"_s.arg(scheme));
        s.setValue(u"URL Protocol"_s, QString());
        s.setValue(u"shell/open/command/Default"_s, command);
    }
}

bool startService(bool restart)
{
    const QString cmd = restart ? u"/c net stop superpupervpn-helper & net start superpupervpn-helper"_s : u"/c net start superpupervpn-helper"_s;
    const std::wstring params = cmd.toStdWString();
    SHELLEXECUTEINFOW sei{sizeof(sei)};
    sei.lpVerb = L"runas";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = params.c_str();
    sei.nShow = SW_HIDE;
    return ShellExecuteExW(&sei);
}

bool reduceMotion()
{
    BOOL animations = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    return !animations;
}

QString osName()
{
    return u"Windows"_s;
}

}
