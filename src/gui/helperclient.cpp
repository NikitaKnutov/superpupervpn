#include "helperclient.h"

#include "protocol.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace rl {
HelperClient::HelperClient(QObject *parent) : QObject(parent), m_socket(new QLocalSocket(this)), m_retry(new QTimer(this))
{
    m_retry->setInterval(2000);
    m_retry->setSingleShot(true);
    connect(m_retry, &QTimer::timeout, this, [this] { m_socket->connectToServer(ipc::helperSocketName()); });
    connect(m_socket, &QLocalSocket::connected, this, &HelperClient::onConnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &HelperClient::onReadyRead);
    connect(m_socket, &QLocalSocket::disconnected, this, [this] {
        setReady(false);
        m_retry->start();
    });
    connect(m_socket, &QLocalSocket::errorOccurred, this, [this] {
        if (m_socket->state() == QLocalSocket::UnconnectedState) {
            setReady(false);
            m_retry->start();
        }
    });
}

void HelperClient::start()
{
    m_socket->connectToServer(ipc::helperSocketName());
}

void HelperClient::call(const QString &method, const QJsonObject &params, Callback cb)
{
    const int id = m_nextId++;
    if (m_socket->state() != QLocalSocket::ConnectedState) {
        if (cb)
            cb({}, {{u"code"_s, -1}, {u"message"_s, QCoreApplication::translate("HelperClient", "The service is unavailable")}});
        return;
    }
    if (cb)
        m_pending.insert(id, std::move(cb));
    m_socket->write(ipc::encode({{u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, id}, {u"method"_s, method}, {u"params"_s, params}}));
}

void HelperClient::setLanguage(const QString &language)
{
    m_language = language;
    if (m_ready)
        call(u"setLanguage"_s, {{u"language"_s, language}});
}

void HelperClient::onConnected()
{
    m_buf.clear();
    call(u"hello"_s, {{u"clientVersion"_s, QStringLiteral(SUPERPUPERVPN_VERSION)}, {u"protocol"_s, ipc::kProtocolMajor}},
         [this](const QJsonObject &r, const QJsonObject &e) {
             if (e.value(u"code"_s).toInt() == -1)
                 return;
             if (!e.isEmpty()) {
                 emit fatal(e.value(u"message"_s).toString());
                 return;
             }
             m_hello = r;
             call(u"setLanguage"_s, {{u"language"_s, m_language}});
             call(u"subscribe"_s, {}, [this](const QJsonObject &r, const QJsonObject &e) {
                 if (!e.isEmpty())
                     return;
                 setReady(true);
                 emit subscribed(r);
             });
         });
}

void HelperClient::onReadyRead()
{
    m_buf += m_socket->readAll();
    qsizetype nl;
    while ((nl = m_buf.indexOf('\n')) >= 0) {
        const QJsonObject m = QJsonDocument::fromJson(m_buf.left(nl)).object();
        m_buf.remove(0, nl + 1);
        if (m.contains(u"method"_s)) {
            emit notification(m.value(u"method"_s).toString(), m.value(u"params"_s).toObject());
            continue;
        }
        const QJsonValue idv = m.value(u"id"_s);
        const QJsonObject err = m.value(u"error"_s).toObject();
        if (idv.isNull() && !err.isEmpty()) {
            emit fatal(err.value(u"message"_s).toString());
            continue;
        }
        if (Callback cb = m_pending.take(idv.toInt()))
            cb(m.value(u"result"_s).toObject(), err);
    }
}

void HelperClient::setReady(bool ready)
{
    if (!ready) {
        const auto pending = std::exchange(m_pending, {});
        for (const Callback &cb : pending)
            cb({}, {{u"code"_s, -1}, {u"message"_s, QCoreApplication::translate("HelperClient", "The service is unavailable")}});
    }
    if (m_ready == ready)
        return;
    m_ready = ready;
    emit readyChanged(ready);
}

}
