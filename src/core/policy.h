#pragma once

#include "profile.h"

#include <QJsonArray>
#include <optional>

namespace rl {
// Machine-wide administrator policy. Only the helper (the SYSTEM / root service) reads and enforces it;
// the GUI learns the outcome from hello.capabilities.
//   Windows: HKLM\SOFTWARE\Policies\SuperPuperVPN, AllowTun (DWORD, 0 = forbidden; missing = allowed)
//   Linux:   /etc/superpupervpn/policy.json, {"allowTun": false}
struct AdminPolicy {
    bool allowTun = true;
};

// A missing key keeps the default. A file that exists but cannot be understood forbids TUN, so a typo never
// silently lifts a restriction the administrator meant to set.
AdminPolicy parsePolicyJson(const QByteArray &json, QString *error = nullptr);

QJsonArray helperCapabilities(const AdminPolicy &policy);
std::optional<FieldError> checkPolicy(const AdminPolicy &policy, const QString &mode);

}
