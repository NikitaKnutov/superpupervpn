#include "profile.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

using namespace Qt::StringLiterals;

namespace rl {
namespace {
const QSet<QString> kNetworks = {u"tcp"_s, u"ws"_s, u"grpc"_s, u"httpupgrade"_s, u"xhttp"_s, u"http"_s};
const QSet<QString> kFingerprints = {u"chrome"_s, u"firefox"_s, u"safari"_s, u"ios"_s, u"android"_s,
                                     u"edge"_s, u"360"_s, u"qq"_s, u"random"_s, u"randomized"_s};
const QSet<QString> kFlows = {QString(), u"xtls-rprx-vision"_s, u"xtls-rprx-vision-udp443"_s};
const QSet<QString> kGrpcModes = {u"gun"_s, u"multi"_s, u"guna"_s};
const QSet<QString> kXhttpModes = {u"auto"_s, u"packet-up"_s, u"stream-up"_s, u"stream-one"_s};
const QSet<QString> kXhttpExtraKeys = {u"headers"_s, u"xPaddingBytes"_s, u"noGRPCHeader"_s,
                                       u"scMaxEachPostBytes"_s, u"scMinPostsIntervalMs"_s};

bool hasControlChars(const QString &s)
{
    for (QChar c : s)
        if (c.category() == QChar::Other_Control)
            return true;
    return false;
}

bool isDomain(const QString &s)
{
    static const QRegularExpression re(u"^(?=.{1,253}$)([A-Za-z0-9_]([A-Za-z0-9_-]{0,61}[A-Za-z0-9_])?\\.)*"
                                       u"[A-Za-z0-9_]([A-Za-z0-9_-]{0,61}[A-Za-z0-9_])?\\.?$"_s);
    return re.match(s).hasMatch();
}

bool isHostOrIp(const QString &s)
{
    return !QHostAddress(s).isNull() || isDomain(s);
}

FieldError err(const QString &field, const QString &reason)
{
    return {FieldError::Format, field, reason};
}

FieldError unsupported(const QString &field, const QString &reason)
{
    return {FieldError::UnsupportedParam, field, reason};
}

}

bool isRfc4122Uuid(const QString &s)
{
    static const QRegularExpression re(
        u"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"_s);
    return re.match(s).hasMatch();
}

bool Profile::nonStandardId() const
{
    return !isRfc4122Uuid(uuid);
}

QString Profile::badge() const
{
    return security == u"none" ? u"VLESS"_s : u"VLESS · "_s + security.toUpper();
}

QString Profile::identityKey() const
{
    const QStringList parts = {host, QString::number(port), uuid, network, encryption, flow, security,
                               sni, pbk, sid, path, hostHeader, serviceName, mode};
    return QString::fromLatin1(QCryptographicHash::hash(parts.join(QChar(0x1f)).toUtf8(),
                                                        QCryptographicHash::Sha256).toHex());
}

QStringList Profile::secrets() const
{
    return {uuid, pbk, sid};
}

QJsonObject Profile::toJson() const
{
    return {
        {u"id"_s, id}, {u"subscriptionId"_s, subscriptionId}, {u"protocol"_s, u"vless"_s},
        {u"name"_s, name}, {u"host"_s, host}, {u"port"_s, port}, {u"uuid"_s, uuid},
        {u"network"_s, network}, {u"encryption"_s, encryption}, {u"flow"_s, flow},
        {u"security"_s, security}, {u"sni"_s, sni}, {u"fp"_s, fp}, {u"pbk"_s, pbk},
        {u"sid"_s, sid}, {u"spx"_s, spx}, {u"alpn"_s, alpn}, {u"path"_s, path},
        {u"hostHeader"_s, hostHeader}, {u"serviceName"_s, serviceName}, {u"mode"_s, mode},
        {u"extra"_s, extra}, {u"ech"_s, ech}, {u"pcs"_s, pcs}, {u"vcn"_s, vcn},
        {u"insecure"_s, insecure}, {u"insecureConsent"_s, insecureConsent}, {u"pinned"_s, pinned},
        {u"unknownParams"_s, unknownParams},
    };
}

Profile Profile::fromJson(const QJsonObject &o)
{
    Profile p;
    auto s = [&](const char16_t *k) { return o.value(QString::fromUtf16(k)).toString(); };
    p.id = s(u"id");
    p.subscriptionId = s(u"subscriptionId");
    p.name = s(u"name");
    p.host = s(u"host");
    p.port = o.value(u"port"_s).toInt();
    p.uuid = s(u"uuid");
    p.network = s(u"network");
    p.encryption = s(u"encryption");
    p.flow = s(u"flow");
    p.security = s(u"security");
    p.sni = s(u"sni");
    p.fp = s(u"fp");
    p.pbk = s(u"pbk");
    p.sid = s(u"sid");
    p.spx = s(u"spx");
    p.alpn = s(u"alpn");
    p.path = s(u"path");
    p.hostHeader = s(u"hostHeader");
    p.serviceName = s(u"serviceName");
    p.mode = s(u"mode");
    p.extra = s(u"extra");
    p.ech = s(u"ech");
    p.pcs = s(u"pcs");
    p.vcn = s(u"vcn");
    p.insecure = o.value(u"insecure"_s).toBool();
    p.insecureConsent = o.value(u"insecureConsent"_s).toBool();
    p.pinned = o.value(u"pinned"_s).toBool();
    p.unknownParams = o.value(u"unknownParams"_s).toObject();
    return p;
}

std::optional<FieldError> rejectUnknownKeys(const QJsonObject &profileJson)
{
    static const QStringList known = Profile().toJson().keys();
    for (const QString &k : profileJson.keys())
        if (!known.contains(k))
            return err(k.left(32), QCoreApplication::translate("Profile", "unknown field"));
    return std::nullopt;
}

QString FieldError::text() const
{
    switch (kind) {
    case UnsupportedProtocol: return QCoreApplication::translate("Profile", "Protocol is not supported");
    case UnsupportedParam: return reason;
    case Format: break;
    }
    return field.isEmpty() ? reason : field + u": "_s + reason;
}

std::optional<FieldError> validate(const Profile &p)
{
    if (p.name.toUcs4().size() > 64)
        return err(u"name"_s, QCoreApplication::translate("Profile", "longer than 64 characters"));
    if (hasControlChars(p.name))
        return err(u"name"_s, QCoreApplication::translate("Profile", "contains control characters"));

    if (p.host.isEmpty())
        return err(u"host"_s, QCoreApplication::translate("Profile", "missing"));
    if (!isHostOrIp(p.host))
        return err(u"host"_s, QCoreApplication::translate("Profile", "not a domain or an IP address"));
    if (p.port < 1 || p.port > 65535)
        return err(u"port"_s, QCoreApplication::translate("Profile", "must be from 1 to 65535"));

    if (p.uuid.isEmpty())
        return err(u"id"_s, QCoreApplication::translate("Profile", "empty ID"));
    if (!isRfc4122Uuid(p.uuid)) {
        if (p.uuid.toUtf8().size() > 30)
            return err(u"id"_s, QCoreApplication::translate("Profile", "not a UUID and longer than 30 bytes"));
        if (hasControlChars(p.uuid))
            return err(u"id"_s, QCoreApplication::translate("Profile", "contains control characters"));
    }

    if (p.network == u"kcp")
        return unsupported(u"type"_s, QCoreApplication::translate("Profile", "The type=kcp parameter is not supported by the mihomo core"));
    if (!kNetworks.contains(p.network))
        return err(u"type"_s, QCoreApplication::translate("Profile", "unknown transport"));

    if (p.encryption.isEmpty())
        return err(u"encryption"_s, QCoreApplication::translate("Profile", "empty value"));
    if (p.encryption != u"none" && !p.encryption.startsWith(u"mlkem768x25519plus."))
        return err(u"encryption"_s, QCoreApplication::translate("Profile", "must be none or mlkem768x25519plus.*"));

    if (!kFlows.contains(p.flow))
        return unsupported(u"flow"_s, QCoreApplication::translate("Profile", "This flow value is not supported by the mihomo core"));

    if (p.security != u"none" && p.security != u"tls" && p.security != u"reality")
        return err(u"security"_s, QCoreApplication::translate("Profile", "must be none, tls or reality"));
    if (!p.flow.isEmpty() && (p.security == u"none" || p.network != u"tcp"))
        return err(u"flow"_s, QCoreApplication::translate("Profile", "Vision works only with tls/reality and type=tcp"));

    if (p.security != u"none") {
        if (!isHostOrIp(p.sni))
            return err(u"sni"_s, QCoreApplication::translate("Profile", "not a domain or an IP address"));
        if (!kFingerprints.contains(p.fp))
            return err(u"fp"_s, QCoreApplication::translate("Profile", "invalid fingerprint"));
    }
    if (p.security == u"reality") {
        static const QRegularExpression pbkRe(u"^[A-Za-z0-9_-]{43}$"_s);
        if (p.pbk.isEmpty())
            return err(u"pbk"_s, QCoreApplication::translate("Profile", "required for reality"));
        if (!pbkRe.match(p.pbk).hasMatch())
            return err(u"pbk"_s, QCoreApplication::translate("Profile", "not an x25519 key in base64url"));
        static const QRegularExpression sidRe(u"^([0-9a-f]{2}){0,8}$"_s);
        if (!sidRe.match(p.sid).hasMatch())
            return err(u"sid"_s, QCoreApplication::translate("Profile", "expected hex of even length, up to 16 characters"));
    }

    // spx only shapes what Xray does after a failed REALITY check; mihomo has no such step, so it is kept, not used.
    if (hasControlChars(p.spx))
        return err(u"spx"_s, QCoreApplication::translate("Profile", "contains control characters"));

    if (!p.alpn.isEmpty()) {
        for (const QString &a : p.alpn.split(u','))
            if (a.isEmpty() || a.contains(QRegularExpression(u"\\s"_s)))
                return err(u"alpn"_s, QCoreApplication::translate("Profile", "a comma-separated list without spaces"));
    }

    const bool pathTransport = p.network == u"ws" || p.network == u"httpupgrade" || p.network == u"xhttp";
    if (pathTransport && !p.path.startsWith(u'/'))
        return err(u"path"_s, QCoreApplication::translate("Profile", "must start with /"));
    if (hasControlChars(p.path) || hasControlChars(p.hostHeader) || hasControlChars(p.serviceName))
        return err(u"path"_s, QCoreApplication::translate("Profile", "contains control characters"));
    if (!p.hostHeader.isEmpty() && !isHostOrIp(p.hostHeader))
        return err(u"host"_s, QCoreApplication::translate("Profile", "not a domain or an IP address"));

    if (p.network == u"grpc") {
        if (p.serviceName.isEmpty())
            return err(u"serviceName"_s, QCoreApplication::translate("Profile", "required for grpc"));
        if (!kGrpcModes.contains(p.mode))
            return err(u"mode"_s, QCoreApplication::translate("Profile", "for grpc must be gun, multi or guna"));
    }
    if (p.network == u"xhttp" && !kXhttpModes.contains(p.mode))
        return err(u"mode"_s, QCoreApplication::translate("Profile", "for xhttp must be auto, packet-up, stream-up or stream-one"));

    if (!p.extra.isEmpty()) {
        if (p.network != u"xhttp")
            return err(u"extra"_s, QCoreApplication::translate("Profile", "allowed only for xhttp"));
        if (p.extra.toUtf8().size() > 4096)
            return err(u"extra"_s, QCoreApplication::translate("Profile", "larger than 4 KB"));
        const QJsonDocument doc = QJsonDocument::fromJson(p.extra.toUtf8());
        if (!doc.isObject())
            return err(u"extra"_s, QCoreApplication::translate("Profile", "not a JSON object"));
        const QJsonObject o = doc.object();
        for (auto it = o.begin(); it != o.end(); ++it)
            if (!kXhttpExtraKeys.contains(it.key()))
                return unsupported(u"extra"_s, QCoreApplication::translate("Profile", "The extra.%1 key is not supported").arg(it.key().left(32)));
        const QJsonValue headers = o.value(u"headers"_s);
        if (!headers.isUndefined()) {
            if (!headers.isObject())
                return err(u"extra"_s, QCoreApplication::translate("Profile", "headers must be an object"));
            for (const QJsonValue v : headers.toObject())
                if (!v.isString() || hasControlChars(v.toString()))
                    return err(u"extra"_s, QCoreApplication::translate("Profile", "headers values must be strings"));
        }
    }

    if (!p.ech.isEmpty()) {
        static const QRegularExpression b64(u"^[A-Za-z0-9+/]+={0,2}$"_s);
        if (p.ech.size() > 4096 || !b64.match(p.ech).hasMatch())
            return unsupported(u"ech"_s, QCoreApplication::translate("Profile", "This ech parameter format is not supported"));
    }
    if (!p.pcs.isEmpty() && !QRegularExpression(u"^[0-9a-fA-F]{64}$"_s).match(p.pcs).hasMatch())
        return err(u"pcs"_s, QCoreApplication::translate("Profile", "expected a hex SHA-256"));
    if (!p.vcn.isEmpty())
        return unsupported(u"vcn"_s, QCoreApplication::translate("Profile", "The vcn parameter is not supported"));

    return std::nullopt;
}

}
