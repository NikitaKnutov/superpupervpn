#include "masking.h"

#include "profile.h"
#include "subscription.h"

#include <QDateTime>
#include <QRegularExpression>

#include <array>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace rl {
void Masker::addSecret(const QString &s)
{
    if (s.size() >= 6 && !m_secrets.contains(s))
        m_secrets << s;
}

void Masker::addHost(const QString &host)
{
    if (!host.isEmpty() && !m_hosts.contains(host))
        m_hosts << host;
}

void Masker::clear()
{
    m_secrets.clear();
    m_hosts.clear();
}

QString Masker::mask(QString text) const
{
    QStringList secrets = m_secrets;
    std::sort(secrets.begin(), secrets.end(), [](const QString &a, const QString &b) { return a.size() > b.size(); });
    for (const QString &s : secrets) {
        if (!text.contains(s))
            continue;
        text.replace(s, isRfc4122Uuid(s) ? u"xxxxxxxx-…-"_s + s.right(4) : u"***"_s);
    }

    static const QRegularExpression urlRe(u"https?://[^\\s\"'<>]+"_s);
    QString out;
    qsizetype last = 0;
    for (auto it = urlRe.globalMatch(text); it.hasNext();) {
        const auto m = it.next();
        out += QStringView(text).mid(last, m.capturedStart() - last);
        out += maskUrl(m.captured());
        last = m.capturedEnd();
    }
    out += QStringView(text).mid(last);
    text = out;

    static const QRegularExpression uuidRe(
        u"\\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{8}([0-9a-fA-F]{4})\\b"_s);
    text.replace(uuidRe, u"xxxxxxxx-…-\\1"_s);

    if (m_maskHosts)
        for (const QString &h : m_hosts)
            text.replace(h, u"***"_s);
    return text;
}

namespace {
quint32 crc32(const QByteArray &data)
{
    static const auto table = [] {
        std::array<quint32, 256> t{};
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    quint32 c = 0xFFFFFFFFu;
    for (char ch : data)
        c = table[(c ^ quint8(ch)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void put16(QByteArray &b, quint16 v) { b.append(char(v & 0xFF)).append(char(v >> 8)); }
void put32(QByteArray &b, quint32 v) { put16(b, quint16(v & 0xFFFF)); put16(b, quint16(v >> 16)); }

}

QByteArray makeZip(const QList<QPair<QString, QByteArray>> &files)
{
    const QDateTime now = QDateTime::currentDateTime();
    const quint16 dosTime = quint16((now.time().hour() << 11) | (now.time().minute() << 5) | (now.time().second() / 2));
    const quint16 dosDate = quint16(((now.date().year() - 1980) << 9) | (now.date().month() << 5) | now.date().day());

    QByteArray out, central;
    for (const auto &[name, data] : files) {
        const QByteArray n = name.toUtf8();
        const quint32 crc = crc32(data);
        const quint32 offset = quint32(out.size());
        auto header = [&](QByteArray &b, bool isCentral) {
            put32(b, isCentral ? 0x02014b50 : 0x04034b50);
            if (isCentral)
                put16(b, 20);
            put16(b, 20);
            put16(b, 0x0800);
            put16(b, 0);
            put16(b, dosTime);
            put16(b, dosDate);
            put32(b, crc);
            put32(b, quint32(data.size()));
            put32(b, quint32(data.size()));
            put16(b, quint16(n.size()));
            put16(b, 0);
            if (isCentral) {
                put16(b, 0);
                put16(b, 0);
                put16(b, 0);
                put32(b, 0);
                put32(b, offset);
            }
            b.append(n);
        };
        header(out, false);
        out.append(data);
        header(central, true);
    }
    const quint32 cdOffset = quint32(out.size());
    out.append(central);
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, quint16(files.size()));
    put16(out, quint16(files.size()));
    put32(out, quint32(central.size()));
    put32(out, cdOffset);
    put16(out, 0);
    return out;
}

}
