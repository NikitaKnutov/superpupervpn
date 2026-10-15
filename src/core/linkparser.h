#pragma once

#include "profile.h"

#include <QByteArray>
#include <QList>

namespace rl {
constexpr int kMaxLinkBytes = 8 * 1024;
constexpr int kMaxBodyBytes = 5 * 1024 * 1024;
constexpr int kMaxServers = 5000;

struct ParseResult {
    Profile profile;
    std::optional<FieldError> error;
    QStringList unknownKeys;
};

ParseResult parseLink(const QString &link);

struct ImportResult {
    QList<Profile> profiles;
    QStringList skipped;
    QStringList unknownKeys;
};

QString decodeBody(const QByteArray &body);
ImportResult parseImport(const QByteArray &body);

}
