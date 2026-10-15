#include "configgen.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace rl {
namespace {
const QString kProxyName = u"proxy"_s;

QJsonArray localNetRules()
{
    return {u"IP-CIDR,10.0.0.0/8,DIRECT,no-resolve"_s,  u"IP-CIDR,172.16.0.0/12,DIRECT,no-resolve"_s,
            u"IP-CIDR,192.168.0.0/16,DIRECT,no-resolve"_s, u"IP-CIDR,169.254.0.0/16,DIRECT,no-resolve"_s,
            u"IP-CIDR,127.0.0.0/8,DIRECT,no-resolve"_s,  u"IP-CIDR6,::1/128,DIRECT,no-resolve"_s,
            u"IP-CIDR6,fc00::/7,DIRECT,no-resolve"_s,    u"IP-CIDR6,fe80::/10,DIRECT,no-resolve"_s};
}

QJsonArray rules()
{
    QJsonArray r = localNetRules();
    r << u"MATCH,"_s + kProxyName;
    return r;
}

void setController(QJsonObject &c, const CoreRuntime &rt)
{
    if (!rt.controllerUnix.isEmpty())
        c[u"external-controller-unix"_s] = rt.controllerUnix;
    if (!rt.controllerPipe.isEmpty())
        c[u"external-controller-pipe"_s] = rt.controllerPipe;
    c[u"secret"_s] = rt.secret;
}

QJsonObject baseConfig(const QString &logLevel)
{
    return {
        {u"mode"_s, u"rule"_s},
        {u"log-level"_s, logLevel},
        {u"ipv6"_s, true},
        {u"allow-lan"_s, false},
        {u"bind-address"_s, u"127.0.0.1"_s},
        {u"skip-auth-prefixes"_s, QJsonArray()},
        {u"profile"_s, QJsonObject{{u"store-selected"_s, false}}},
    };
}

}

QJsonObject CoreSettings::toJson() const
{
    return {{u"mode"_s, mode}, {u"dohUrl"_s, dohUrl}, {u"tunMtu"_s, tunMtu},
            {u"logLevel"_s, logLevel}};
}

CoreSettings CoreSettings::fromJson(const QJsonObject &o)
{
    CoreSettings s;
    s.mode = o.value(u"mode"_s).toString(s.mode);
    s.dohUrl = o.value(u"dohUrl"_s).toString(s.dohUrl);
    s.tunMtu = o.value(u"tunMtu"_s).toInt(s.tunMtu);
    s.logLevel = o.value(u"logLevel"_s).toString(s.logLevel);
    return s;
}

std::optional<FieldError> validate(const CoreSettings &s)
{
    if (s.mode != u"tun" && s.mode != u"proxy")
        return FieldError{FieldError::Format, u"mode"_s, u"допустимо tun или proxy"_s};
    const QUrl doh(s.dohUrl, QUrl::StrictMode);
    if (s.dohUrl.size() > 256 || !doh.isValid() || doh.scheme() != u"https" || doh.host().isEmpty()
        || !doh.userInfo().isEmpty() || s.dohUrl.contains(u' '))
        return FieldError{FieldError::Format, u"dns"_s, u"ожидается https://-адрес DoH"_s};
    if (s.tunMtu != 0 && (s.tunMtu < 1280 || s.tunMtu > 9000))
        return FieldError{FieldError::Format, u"mtu"_s, u"от 1280 до 9000"_s};
    if (!QSet<QString>{u"silent"_s, u"error"_s, u"warning"_s, u"info"_s, u"debug"_s}.contains(s.logLevel))
        return FieldError{FieldError::Format, u"logLevel"_s, u"неизвестный уровень"_s};
    return std::nullopt;
}

QJsonObject proxyEntry(const Profile &p, const QString &name)
{
    QJsonObject o{{u"name"_s, name}, {u"type"_s, u"vless"_s}, {u"server"_s, p.host}, {u"port"_s, p.port},
                  {u"uuid"_s, p.uuid}, {u"udp"_s, true}};

    static const QHash<QString, QString> networks = {{u"tcp"_s, u"tcp"_s}, {u"ws"_s, u"ws"_s},
                                                     {u"httpupgrade"_s, u"ws"_s}, {u"grpc"_s, u"grpc"_s},
                                                     {u"xhttp"_s, u"xhttp"_s}, {u"http"_s, u"h2"_s}};
    o[u"network"_s] = networks.value(p.network, u"tcp"_s);
    if (p.encryption != u"none")
        o[u"encryption"_s] = p.encryption;
    if (!p.flow.isEmpty())
        o[u"flow"_s] = p.flow;

    if (p.security != u"none") {
        o[u"tls"_s] = true;
        o[u"servername"_s] = p.sni;
        o[u"client-fingerprint"_s] = p.fp;
        if (!p.alpn.isEmpty())
            o[u"alpn"_s] = QJsonArray::fromStringList(p.alpn.split(u','));
        if (p.insecure && p.insecureConsent)
            o[u"skip-cert-verify"_s] = true;
        if (!p.pcs.isEmpty())
            o[u"fingerprint"_s] = p.pcs.toLower();
        if (!p.ech.isEmpty())
            o[u"ech-opts"_s] = QJsonObject{{u"enable"_s, true}, {u"config"_s, p.ech}};
    }
    if (p.security == u"reality")
        o[u"reality-opts"_s] = QJsonObject{{u"public-key"_s, p.pbk}, {u"short-id"_s, p.sid},
                                           {u"support-x25519mlkem768"_s, true}};

    if (p.network == u"ws" || p.network == u"httpupgrade") {
        QJsonObject ws{{u"path"_s, p.path}};
        if (!p.hostHeader.isEmpty())
            ws[u"headers"_s] = QJsonObject{{u"Host"_s, p.hostHeader}};
        if (p.network == u"httpupgrade")
            ws[u"v2ray-http-upgrade"_s] = true;
        o[u"ws-opts"_s] = ws;
    } else if (p.network == u"grpc") {
        o[u"grpc-opts"_s] = QJsonObject{{u"grpc-service-name"_s, p.serviceName}};
    } else if (p.network == u"http") {
        QJsonObject h2{{u"path"_s, p.path.isEmpty() ? u"/"_s : p.path}};
        if (!p.hostHeader.isEmpty())
            h2[u"host"_s] = QJsonArray{p.hostHeader};
        o[u"h2-opts"_s] = h2;
    } else if (p.network == u"xhttp") {
        QJsonObject x{{u"path"_s, p.path}, {u"mode"_s, p.mode}};
        if (!p.hostHeader.isEmpty())
            x[u"host"_s] = p.hostHeader;
        const QJsonObject extra = QJsonDocument::fromJson(p.extra.toUtf8()).object();
        static const QHash<QString, QString> extraKeys = {
            {u"headers"_s, u"headers"_s}, {u"xPaddingBytes"_s, u"x-padding-bytes"_s},
            {u"noGRPCHeader"_s, u"no-grpc-header"_s}, {u"scMaxEachPostBytes"_s, u"sc-max-each-post-bytes"_s},
            {u"scMinPostsIntervalMs"_s, u"sc-min-posts-interval-ms"_s}};
        for (auto it = extra.begin(); it != extra.end(); ++it)
            if (extraKeys.contains(it.key()))
                x[extraKeys.value(it.key())] = it.value();
        o[u"xhttp-opts"_s] = x;
    }
    return o;
}

QJsonObject buildCoreConfig(const Profile &p, const CoreSettings &s, const CoreRuntime &rt)
{
    QJsonObject c = baseConfig(s.logLevel);
    setController(c, rt);

    const bool tun = s.mode == u"tun";
    if (tun) {
        QJsonObject t{
            {u"enable"_s, true},
            {u"device"_s, u"superpupervpn"_s},
            {u"auto-route"_s, true},
            {u"auto-detect-interface"_s, true},
            {u"strict-route"_s, true},
            {u"dns-hijack"_s, QJsonArray{u"any:53"_s}},
            {u"iproute2-table-index"_s, 8700},
            {u"iproute2-rule-index"_s, 8700},
        };
        if (rt.autoRedirect)
            t[u"auto-redirect"_s] = true;
        if (s.tunMtu)
            t[u"mtu"_s] = s.tunMtu;
        c[u"tun"_s] = t;
    } else {
        c[u"listeners"_s] = QJsonArray{QJsonObject{{u"name"_s, u"sysproxy"_s}, {u"type"_s, u"mixed"_s},
                                                   {u"listen"_s, u"127.0.0.1"_s}, {u"port"_s, rt.proxyPort},
                                                   {u"udp"_s, false}}};
        if (!rt.interfaceName.isEmpty())
            c[u"interface-name"_s] = rt.interfaceName;
    }

    const QString direct = QString::fromUtf16(kDirectDns);
    QJsonObject dns{
        {u"enable"_s, true},
        {u"ipv6"_s, true},
        {u"respect-rules"_s, true},
        {u"nameserver"_s, QJsonArray{s.dohUrl}},
        {u"proxy-server-nameserver"_s, QJsonArray{direct}},
    };
    if (tun) {
        dns[u"enhanced-mode"_s] = u"fake-ip"_s;
        dns[u"fake-ip-range"_s] = u"198.18.0.1/16"_s;
    }
    c[u"dns"_s] = dns;

    c[u"proxies"_s] = QJsonArray{proxyEntry(p, kProxyName)};
    c[u"rules"_s] = rules();
    return c;
}

QJsonObject buildTestConfig(const QList<Profile> &profiles, const CoreRuntime &rt, const QString &interfaceName)
{
    QJsonObject c = baseConfig(u"warning"_s);
    setController(c, rt);
    if (!interfaceName.isEmpty())
        c[u"interface-name"_s] = interfaceName;
    const QString direct = QString::fromUtf16(kDirectDns);
    c[u"dns"_s] = QJsonObject{{u"enable"_s, true}, {u"nameserver"_s, QJsonArray{direct}},
                              {u"proxy-server-nameserver"_s, QJsonArray{direct}}};
    QJsonArray proxies;
    for (const Profile &p : profiles)
        proxies << proxyEntry(p, p.id);
    c[u"proxies"_s] = proxies;
    c[u"rules"_s] = QJsonArray{u"MATCH,DIRECT"_s};
    return c;
}

}
