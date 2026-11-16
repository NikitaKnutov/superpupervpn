#pragma once

#include <QObject>
#include <QString>
#include <functional>

namespace rl::gui {
enum class UpdateInstallResult { Started, Installed, Failed };
using UpdateInstallCallback = std::function<void(UpdateInstallResult, const QString &)>;

void setAutostart(bool on);
void registerUrlSchemes();
bool startService(bool restart);
QString updateAssetName(const QString &version);
void installUpdate(const QString &path, QObject *context, UpdateInstallCallback done);
bool reduceMotion();
QString osName();

}
