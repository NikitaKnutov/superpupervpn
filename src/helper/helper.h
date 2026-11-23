#pragma once

#include "configgen.h"
#include "policy.h"
#include "profile.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>

class QLocalSocket;
class QNetworkAccessManager;
class QTimer;

namespace rl {
class CoreProcess;
class HelperServer;

class Helper : public QObject {
    Q_OBJECT
public:
    explicit Helper(QObject *parent = nullptr);
    ~Helper() override;
    bool start(QString *error);
    void shutdown();

private:
    struct Client {
        QByteArray buf;
        QString peer;
        QString owner;
        bool subscribed = false;
    };

    void onClient(QLocalSocket *s, const QString &peer, const QString &owner);
    void onClientGone(QLocalSocket *s);
    void onReadyRead(QLocalSocket *s);
    void dispatch(QLocalSocket *s, const QJsonObject &req);
    void send(QLocalSocket *s, const QJsonObject &msg);
    void reply(QLocalSocket *s, const QJsonValue &id, const QJsonObject &result);
    void replyError(QLocalSocket *s, const QJsonValue &id, int code, const QString &message,
                    const QString &field = QString());
    void notify(QLocalSocket *s, const QString &method, const QJsonObject &params);
    void broadcast(const QString &method, const QJsonObject &params, bool ownerOnly = false);
    void log(const QString &source, const QString &level, const QString &message);

    QJsonObject helloResult();
    AdminPolicy policy();
    bool usedByOther(const QString &owner) const;
    QJsonObject stateFor(const QString &owner) const;
    void broadcastState();
    std::optional<FieldError> connectCore(const QJsonObject &params, const QString &owner);
    void disconnectCore();
    void startCore();
    void onCoreReady();
    void checkConnection(bool initial);
    void onCoreFailed(bool beforeReady, const QString &error);
    void setState(const QString &state, QJsonObject extra = {});
    void testDelay(QLocalSocket *s, const QJsonValue &id, const QJsonObject &params);
    void diagnostics(QLocalSocket *s, const QJsonValue &id);
    void onNetworkChanged();

    QNetworkAccessManager *m_probeNam;
    HelperServer *m_server;
    CoreProcess *m_core;
    CoreProcess *m_test;
    QHash<QLocalSocket *, Client> m_clients;
    QList<QJsonObject> m_backlog;
    QJsonObject m_state{{QStringLiteral("state"), QStringLiteral("disconnected")}};
    QString m_coreVersion;

    // Who started the core (see HelperServer::clientReady). The helper runs one core for the whole machine, so on
    // a terminal server only that user may change or stop it while one of their clients is still connected.
    QString m_owner;
    QString m_policyError;
    bool m_wanted = false;
    bool m_wasConnected = false;
    Profile m_profile;
    CoreSettings m_settings;
    bool m_maskHosts = true;
    int m_attempts = 0;
    int m_proxyPort = 0;
    QString m_iface;
    QTimer *m_retry;
    QTimer *m_stable;
    QTimer *m_netDebounce;
    QTimer *m_health;
    int m_healthFailures = 0;
    QObject *m_delayJob = nullptr;
};

}
