#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QString>

namespace rl {
class GeoIp {
public:
    explicit GeoIp(const QByteArray &table = {});
    QString country(const QHostAddress &address) const;

private:
    QByteArray m_data;
    const uchar *m_v4 = nullptr;
    const uchar *m_v6 = nullptr;
    quint32 m_n4 = 0;
    quint32 m_n6 = 0;
};

QString flagEmoji(const QString &countryCode);

}
