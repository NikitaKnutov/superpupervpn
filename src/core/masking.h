#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QStringList>

namespace rl {
class Masker {
public:
    void addSecret(const QString &s);
    void addHost(const QString &host);
    void setMaskHosts(bool on) { m_maskHosts = on; }
    void clear();
    QString mask(QString text) const;

private:
    QStringList m_secrets, m_hosts;
    bool m_maskHosts = true;
};

QByteArray makeZip(const QList<QPair<QString, QByteArray>> &files);

}
