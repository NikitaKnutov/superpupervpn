#include "geoip.h"

#include <QtEndian>

using namespace Qt::StringLiterals;

namespace rl {
namespace {
template<typename Key>
QString lookup(const uchar *base, quint32 n, Key key)
{
    constexpr int rec = sizeof(Key) + 2;
    quint32 lo = 0, hi = n;
    while (lo < hi) {
        const quint32 mid = lo + (hi - lo) / 2;
        if (qFromBigEndian<Key>(base + mid * rec) <= key)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == 0)
        return {};
    const char *cc = reinterpret_cast<const char *>(base + (lo - 1) * rec + sizeof(Key));
    const QString code = QString::fromLatin1(cc, 2);
    return code == u"ZZ" ? QString() : code;
}

}

GeoIp::GeoIp(const QByteArray &table) : m_data(table)
{
    const auto *p = reinterpret_cast<const uchar *>(m_data.constData());
    const qsizetype size = m_data.size();
    if (size < 12 || !m_data.startsWith("GEO1"))
        return;
    const quint32 n4 = qFromBigEndian<quint32>(p + 4);
    const qsizetype v6Header = 8 + qsizetype(n4) * 6;
    if (v6Header + 4 > size)
        return;
    const quint32 n6 = qFromBigEndian<quint32>(p + v6Header);
    if (v6Header + 4 + qsizetype(n6) * 10 != size)
        return;
    m_v4 = p + 8;
    m_n4 = n4;
    m_v6 = p + v6Header + 4;
    m_n6 = n6;
}

QString GeoIp::country(const QHostAddress &address) const
{
    bool isV4 = false;
    const quint32 v4 = address.toIPv4Address(&isV4);
    if (isV4)
        return lookup<quint32>(m_v4, m_n4, v4);
    if (address.protocol() != QAbstractSocket::IPv6Protocol)
        return {};
    const Q_IPV6ADDR a = address.toIPv6Address();
    return lookup<quint64>(m_v6, m_n6, qFromBigEndian<quint64>(a.c));
}

QString flagEmoji(const QString &countryCode)
{
    if (countryCode.size() != 2)
        return {};
    QString out;
    for (const QChar c : countryCode.toUpper()) {
        if (c < u'A' || c > u'Z')
            return {};
        const char32_t regional = 0x1F1E6 + (c.unicode() - u'A');
        out += QString::fromUcs4(&regional, 1);
    }
    return out;
}

}
