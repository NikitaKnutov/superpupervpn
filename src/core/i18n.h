#pragma once

#include <QString>

namespace rl {
// Switches the process to "en" (the source language) or "ru". Returns false for an unknown language.
bool setLanguage(const QString &language);
}
