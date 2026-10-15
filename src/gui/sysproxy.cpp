#include "sysproxy.h"

#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

using namespace Qt::StringLiterals;

namespace rl {
SystemProxy::SystemProxy(const QString &dir) : m_dir(dir) {}

int SystemProxy::markerPort() const
{
    QFile f(m_dir + u"/proxy_applied"_s);
    return f.open(QIODevice::ReadOnly) ? f.readAll().trimmed().toInt() : 0;
}

bool SystemProxy::apply(int port, QString *error)
{
    if (!markerPort()) {
        QSaveFile snap(m_dir + u"/sysproxy-snapshot.json"_s);
        if (!snap.open(QIODevice::WriteOnly) || snap.write(QJsonDocument(sysproxy::capture()).toJson()) < 0 || !snap.commit()) {
            *error = u"не удалось сохранить снимок настроек прокси"_s;
            return false;
        }
    }
    QSaveFile marker(m_dir + u"/proxy_applied"_s);
    if (!marker.open(QIODevice::WriteOnly) || marker.write(QByteArray::number(port)) < 0 || !marker.commit()) {
        *error = u"не удалось записать маркер"_s;
        return false;
    }
    return sysproxy::set(port, error);
}

bool SystemProxy::restore(QString *error)
{
    QFile snap(m_dir + u"/sysproxy-snapshot.json"_s);
    if (!snap.open(QIODevice::ReadOnly)) {
        QFile::remove(m_dir + u"/proxy_applied"_s);
        return true;
    }
    const QJsonObject s = QJsonDocument::fromJson(snap.readAll()).object();
    snap.close();
    if (!sysproxy::restore(s, error))
        return false;
    QFile::remove(m_dir + u"/proxy_applied"_s);
    QFile::remove(m_dir + u"/sysproxy-snapshot.json"_s);
    return true;
}

void SystemProxy::forget()
{
    QFile::remove(m_dir + u"/proxy_applied"_s);
    QFile::remove(m_dir + u"/sysproxy-snapshot.json"_s);
}

bool SystemProxy::pointsTo(int port)
{
    return port > 0 && sysproxy::pointsTo(port);
}

}
