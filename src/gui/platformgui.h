#pragma once

#include <QString>

namespace rl::gui {
void setAutostart(bool on);
void registerUrlSchemes();
bool startService(bool restart);
bool reduceMotion();
QString osName();

}
