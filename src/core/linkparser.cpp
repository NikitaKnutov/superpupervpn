#include "linkparser.h"

#include <QHostAddress>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace rl {
namespace {
const QSet<QString> kOtherProtocols = {u"vmess"_s, u"trojan"_s, u"ss"_s, u"hysteria2"_s, u"hy2"_s,
                                       u"tuic"_s, u"wireguard"_s, u"anytls"_s, u"ssr"_s, u"socks"_s,
                                       u"http"_s, u"https"_s, u"hysteria"_s};

const QSet<QString> kKnownParams = {u"type"_s, u"encryption"_s, u"flow"_s, u"security"_s, u"sni"_s,
                                    u"fp"_s, u"pbk"_s, u"sid"_s, u"spx"_s, u"pqv"_s, u"alpn"_s, u"path"_s,
                                    u"host"_s, u"serviceName"_s, u"mode"_s, u"extra"_s, u"ech"_s,
                                    u"pcs"_s, u"vcn"_s, u"fm"_s, u"allowInsecure"_s, u"insecure"_s,
                                    u"headerType"_s};

std::optional<QString> pct(const QString &s)
{
    QStringDecoder dec(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString out = dec(QByteArray::fromPercentEncoding(s.toUtf8()));
    if (dec.hasError())
        return std::nullopt;
    return out;
}

ParseResult fail(FieldError::Kind kind, const QString &field, const QString &reason)
{
    ParseResult r;
    r.error = FieldError{kind, field, reason};
    return r;
}

ParseResult fail(const QString &field, const QString &reason)
{
    return fail(FieldError::Format, field, reason);
}

}

ParseResult parseLink(const QString &input)
{
    const QString link = input.trimmed();
    if (link.toUtf8().size() > kMaxLinkBytes)
        return fail(QString(), u"ссылка длиннее 8 КБ"_s);

    const qsizetype colon = link.indexOf(u':');
    if (colon <= 0)
        return fail(QString(), u"не ссылка"_s);
    const QString scheme = link.left(colon).toLower();
    if (kOtherProtocols.contains(scheme))
        return fail(FieldError::UnsupportedProtocol, u"scheme"_s, QString());
    if (scheme != u"vless" || !link.mid(colon).startsWith(u"://"))
        return fail(u"scheme"_s, u"неизвестная схема"_s);

    QString rest = link.mid(colon + 3);
    QString fragment, query;
    if (const qsizetype h = rest.indexOf(u'#'); h >= 0) {
        fragment = rest.mid(h + 1);
        rest.truncate(h);
    }
    if (const qsizetype q = rest.indexOf(u'?'); q >= 0) {
        query = rest.mid(q + 1);
        rest.truncate(q);
    }
    if (const qsizetype s = rest.indexOf(u'/'); s >= 0) {
        if (rest.mid(s) != u"/")
            return fail(u"path"_s, u"лишний путь после адреса"_s);
        rest.truncate(s);
    }

    Profile p;
    const qsizetype at = rest.lastIndexOf(u'@');
    if (at < 0)
        return fail(u"id"_s, u"пустой идентификатор"_s);
    const auto uuid = pct(rest.left(at));
    if (!uuid)
        return fail(u"id"_s, u"неверная кодировка"_s);
    p.uuid = *uuid;

    const QString hostport = rest.mid(at + 1);
    QString host, portStr;
    if (hostport.startsWith(u'[')) {
        const qsizetype close = hostport.indexOf(u']');
        if (close < 0 || QHostAddress(hostport.mid(1, close - 1)).protocol() != QAbstractSocket::IPv6Protocol)
            return fail(u"host"_s, u"неверный IPv6-адрес"_s);
        if (hostport.mid(close + 1, 1) != u":")
            return fail(u"port"_s, u"не указан"_s);
        host = hostport.mid(1, close - 1);
        portStr = hostport.mid(close + 2);
    } else {
        const qsizetype c = hostport.lastIndexOf(u':');
        if (c < 0)
            return fail(u"port"_s, u"не указан"_s);
        host = hostport.left(c);
        portStr = hostport.mid(c + 1);
        if (host.contains(u':'))
            return fail(u"host"_s, u"IPv6-адрес должен быть в []"_s);
        const auto decoded = pct(host);
        if (!decoded)
            return fail(u"host"_s, u"неверная кодировка"_s);
        host = QHostAddress(*decoded).isNull() ? QString::fromLatin1(QUrl::toAce(*decoded)) : *decoded;
        if (host.isEmpty())
            return fail(u"host"_s, u"не домен и не IP-адрес"_s);
    }
    static const QRegularExpression digits(u"^[0-9]{1,5}$"_s);
    if (!digits.match(portStr).hasMatch())
        return fail(u"port"_s, u"не число"_s);
    p.host = host;
    p.port = portStr.toInt();

    QHash<QString, QString> params;
    ParseResult r;
    for (const QString &part : query.split(u'&', Qt::SkipEmptyParts)) {
        const qsizetype eq = part.indexOf(u'=');
        const auto key = pct(eq < 0 ? part : part.left(eq));
        const auto value = pct(eq < 0 ? QString() : part.mid(eq + 1));
        if (!key || !value)
            return fail(key.value_or(QString()), u"неверная кодировка"_s);
        if (params.contains(*key))
            return fail(*key, u"параметр повторяется"_s);
        params.insert(*key, *value);
        if (!kKnownParams.contains(*key)) {
            r.unknownKeys << *key;
            p.unknownParams.insert(*key, *value);
        }
    }

    if (!params.value(u"pqv"_s).isEmpty())
        return fail(FieldError::UnsupportedParam, u"pqv"_s, u"Параметр pqv не поддерживается ядром"_s);
    if (params.contains(u"fm"_s))
        return fail(FieldError::UnsupportedParam, u"fm"_s, u"Параметр fm не поддерживается ядром"_s);
    if (const QString ht = params.value(u"headerType"_s); !ht.isEmpty() && ht != u"none")
        return fail(FieldError::UnsupportedParam, u"headerType"_s,
                    u"Параметр headerType=%1 не поддерживается ядром"_s.arg(ht.left(16)));

    p.network = params.value(u"type"_s);
    if (p.network.isEmpty())
        p.network = u"tcp"_s;
    p.encryption = params.contains(u"encryption"_s) ? params.value(u"encryption"_s) : u"none"_s;
    p.flow = params.value(u"flow"_s);
    p.security = params.value(u"security"_s, u"none"_s);
    if (p.security.isEmpty())
        p.security = u"none"_s;
    if (p.security != u"none") {
        p.sni = params.value(u"sni"_s);
        if (p.sni.isEmpty())
            p.sni = p.host;
        p.fp = params.value(u"fp"_s);
        if (p.fp.isEmpty())
            p.fp = u"chrome"_s;
    }
    p.pbk = params.value(u"pbk"_s);
    p.sid = params.value(u"sid"_s).toLower();
    p.spx = params.value(u"spx"_s);
    p.alpn = params.value(u"alpn"_s);
    p.path = params.value(u"path"_s);
    if (p.path.isEmpty() && (p.network == u"ws" || p.network == u"httpupgrade" || p.network == u"xhttp"))
        p.path = u"/"_s;
    p.hostHeader = params.value(u"host"_s);
    p.serviceName = params.value(u"serviceName"_s);
    p.mode = params.value(u"mode"_s);
    if (p.mode.isEmpty() && p.network == u"grpc")
        p.mode = u"gun"_s;
    if (p.mode.isEmpty() && p.network == u"xhttp")
        p.mode = u"auto"_s;
    p.extra = params.value(u"extra"_s);
    p.ech = params.value(u"ech"_s);
    p.pcs = params.value(u"pcs"_s);
    p.vcn = params.value(u"vcn"_s);
    for (const QString &k : {u"allowInsecure"_s, u"insecure"_s})
        if (params.value(k) == u"1" || params.value(k).toLower() == u"true")
            p.insecure = true;

    const auto name = pct(fragment);
    if (!name)
        return fail(u"name"_s, u"неверная кодировка"_s);
    p.name = name->trimmed();
    if (p.name.isEmpty())
        p.name = p.host;

    r.error = validate(p);
    r.profile = p;
    return r;
}

QString decodeBody(const QByteArray &body)
{
    QByteArray text = body;
    if (text.startsWith("\xEF\xBB\xBF"))
        text.remove(0, 3);
    text = text.trimmed();

    QByteArray compact = text;
    compact.removeIf([](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; });
    QByteArray padded = compact;
    while (padded.size() % 4)
        padded.append('=');
    for (auto [data, opt] : {std::pair{compact, QByteArray::Base64Encoding},
                             std::pair{padded, QByteArray::Base64UrlEncoding}}) {
        const auto res = QByteArray::fromBase64Encoding(data, opt | QByteArray::AbortOnBase64DecodingErrors);
        if (!res || res.decoded.isEmpty())
            continue;
        QStringDecoder dec(QStringDecoder::Utf8);
        const QString decoded = dec(res.decoded);
        if (!dec.hasError() && decoded.contains(u"://"))
            return decoded;
    }
    return QString::fromUtf8(text);
}

ImportResult parseImport(const QByteArray &body)
{
    ImportResult out;
    if (body.size() > kMaxBodyBytes) {
        out.skipped << u"данные больше 5 МБ"_s;
        return out;
    }
    static const QRegularExpression lineBreak(u"[\r\n]+"_s);
    const QStringList lines = decodeBody(body).split(lineBreak, Qt::SkipEmptyParts);
    int lineNo = 0;
    for (const QString &raw : lines) {
        ++lineNo;
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        if (out.profiles.size() >= kMaxServers) {
            out.skipped << u"строка %1: превышен лимит %2 серверов"_s.arg(lineNo).arg(kMaxServers);
            continue;
        }
        ParseResult r = parseLink(line);
        out.unknownKeys << r.unknownKeys;
        if (r.error)
            out.skipped << u"строка %1: %2"_s.arg(lineNo).arg(r.error->text());
        else
            out.profiles << r.profile;
    }
    out.unknownKeys.removeDuplicates();
    return out;
}

}
