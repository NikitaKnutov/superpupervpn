#include "helper.h"

#include "coreprocess.h"
#include "platform.h"
#include "protocol.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QNetworkInformation>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTimer>
#include <QUrlQuery>
#include <QVersionNumber>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace rl {
namespace {
enum ErrorCode {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    Busy = -32000,
    VersionMismatch = -32001,
    Failed = -32002,
};

const QString kDefaultDelayUrl = u"https://www.gstatic.com/generate_204"_s;

bool validProfileId(const QString &id)
{
    static const QRegularExpression re(u"^[A-Za-z0-9{}_-]{1,64}$"_s);
    return re.match(id).hasMatch();
}

bool validTestUrl(const QString &s)
{
    const QUrl u(s, QUrl::StrictMode);
    return s.size() <= 256 && u.isValid() && u.scheme() == u"https" && !u.host().isEmpty() && u.userInfo().isEmpty();
}

int pickProxyPort(int preferred)
{
    for (int i = 0; i < 20; ++i) {
        const int port = i == 0 && preferred ? preferred : QRandomGenerator::system()->bounded(20000, 60001);
        QTcpServer probe;
        if (probe.listen(QHostAddress::LocalHost, quint16(port)))
            return port;
    }
    return 0;
}

class DelayJob : public QObject {
public:
    QPointer<QLocalSocket> socket;
    QJsonValue id;
    QStringList queue;
    QString url;
    int timeoutMs = 5000;
    int concurrency = 8;
    int inflight = 0;
    int done = 0;
    int total = 0;
    using QObject::QObject;
};

}

Helper::Helper(QObject *parent)
    : QObject(parent),
      m_probeNam(new QNetworkAccessManager(this)),
      m_server(new HelperServer(this)),
      m_core(new CoreProcess(u"core"_s, this)),
      m_test(new CoreProcess(u"test"_s, this)),
      m_retry(new QTimer(this)),
      m_stable(new QTimer(this)),
      m_netDebounce(new QTimer(this))
{
    m_retry->setSingleShot(true);
    connect(m_retry, &QTimer::timeout, this, &Helper::startCore);
    m_stable->setSingleShot(true);
    m_stable->setInterval(60000);
    connect(m_stable, &QTimer::timeout, this, [this] { m_attempts = 0; });
    m_netDebounce->setSingleShot(true);
    m_netDebounce->setInterval(3000);
    connect(m_netDebounce, &QTimer::timeout, this, &Helper::onNetworkChanged);

    connect(m_server, &HelperServer::clientReady, this, &Helper::onClient);
    connect(m_core, &CoreProcess::ready, this, &Helper::onCoreReady);
    connect(m_core, &CoreProcess::exited, this, &Helper::onCoreFailed);
    connect(m_core, &CoreProcess::logLine, this, [this](const QString &l, const QString &m) { log(u"core"_s, l, m); });
    connect(m_core, &CoreProcess::traffic, this, [this](const QJsonObject &t) { broadcast(u"traffic"_s, t); });
    connect(m_test, &CoreProcess::logLine, this, [this](const QString &l, const QString &m) {
        if (l != u"info" && l != u"debug")
            log(u"core"_s, l, u"[проверка] "_s + m);
    });
}

Helper::~Helper() = default;

bool Helper::start(QString *error)
{
    platform::cleanupStaleNetwork();
    if (!platform::ensurePrivateDir(platform::dataDir(), error))
        return false;
    QString why;
    if (verifyCore(&why))
        m_coreVersion = coreVersion();
    else
        log(u"helper"_s, u"error"_s, why);
    if (!m_server->listen(error))
        return false;

    if (QNetworkInformation::loadDefaultBackend()) {
        auto *ni = QNetworkInformation::instance();
        connect(ni, &QNetworkInformation::transportMediumChanged, m_netDebounce, qOverload<>(&QTimer::start));
        connect(ni, &QNetworkInformation::reachabilityChanged, this, [this](QNetworkInformation::Reachability r) {
            if (r == QNetworkInformation::Reachability::Online)
                m_netDebounce->start();
        });
    }
    qInfo("superpupervpn-helper %s listening, core %s", SUPERPUPERVPN_VERSION, qPrintable(m_coreVersion));
    return true;
}

void Helper::shutdown()
{
    m_test->stop();
    disconnectCore();
}

void Helper::onClient(QLocalSocket *s, const QString &peer)
{
    m_clients.insert(s, Client{QByteArray(), peer});
    connect(s, &QLocalSocket::readyRead, this, [this, s] { onReadyRead(s); });
    connect(s, &QObject::destroyed, this, [this, s] { m_clients.remove(s); });
    connect(s, &QLocalSocket::disconnected, this, [this, s] { m_clients.remove(s); });
    onReadyRead(s);
}

void Helper::onReadyRead(QLocalSocket *s)
{
    auto it = m_clients.find(s);
    if (it == m_clients.end())
        return;
    it->buf += s->readAll();
    qsizetype nl;
    while ((nl = it->buf.indexOf('\n')) >= 0) {
        const QByteArray line = it->buf.left(nl);
        it->buf.remove(0, nl + 1);
        if (line.trimmed().isEmpty())
            continue;
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
        if (!doc.isObject())
            replyError(s, QJsonValue::Null, ParseError, u"неверный JSON"_s);
        else
            dispatch(s, doc.object());
        it = m_clients.find(s);
        if (it == m_clients.end())
            return;
    }
    if (it->buf.size() > ipc::kMaxLineBytes) {
        qWarning("dropping %s: line too long", qPrintable(it->peer));
        m_clients.erase(it);
        s->abort();
    }
}

void Helper::send(QLocalSocket *s, const QJsonObject &msg)
{
    if (s && s->state() == QLocalSocket::ConnectedState)
        s->write(ipc::encode(msg));
}

void Helper::reply(QLocalSocket *s, const QJsonValue &id, const QJsonObject &result)
{
    if (!id.isUndefined())
        send(s, {{u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, id}, {u"result"_s, result}});
}

void Helper::replyError(QLocalSocket *s, const QJsonValue &id, int code, const QString &message, const QString &field)
{
    QJsonObject err{{u"code"_s, code}, {u"message"_s, message}};
    if (!field.isEmpty())
        err[u"data"_s] = QJsonObject{{u"field"_s, field}};
    send(s, {{u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, id.isUndefined() ? QJsonValue::Null : id}, {u"error"_s, err}});
}

void Helper::notify(QLocalSocket *s, const QString &method, const QJsonObject &params)
{
    send(s, {{u"jsonrpc"_s, u"2.0"_s}, {u"method"_s, method}, {u"params"_s, params}});
}

void Helper::broadcast(const QString &method, const QJsonObject &params)
{
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
        if (it->subscribed)
            notify(it.key(), method, params);
}

void Helper::log(const QString &source, const QString &level, const QString &message)
{
    const QJsonObject line{{u"source"_s, source}, {u"level"_s, level}, {u"msg"_s, message},
                           {u"time"_s, QDateTime::currentMSecsSinceEpoch()}};
    m_backlog << line;
    if (m_backlog.size() > 500)
        m_backlog.removeFirst();
    if (source == u"helper")
        qInfo("%s: %s", qPrintable(level), qPrintable(message));
    broadcast(u"log"_s, line);
}

void Helper::dispatch(QLocalSocket *s, const QJsonObject &req)
{
    const QJsonValue id = req.value(u"id"_s);
    const QString method = req.value(u"method"_s).toString();
    const QJsonObject params = req.value(u"params"_s).toObject();
    if (req.value(u"jsonrpc"_s) != u"2.0"_s || method.isEmpty()) {
        replyError(s, id, InvalidRequest, u"неверный запрос"_s);
        return;
    }

    if (method == u"hello") {
        const QVersionNumber client = QVersionNumber::fromString(params.value(u"clientVersion"_s).toString());
        const QVersionNumber self = QVersionNumber::fromString(QStringLiteral(SUPERPUPERVPN_VERSION));
        if (params.value(u"protocol"_s).toInt(ipc::kProtocolMajor) != ipc::kProtocolMajor
            || client.majorVersion() != self.majorVersion()) {
            replyError(s, id, VersionMismatch, u"Версии приложения и службы несовместимы — переустановите SuperPuperVPN"_s);
            return;
        }
        reply(s, id, helloResult());
    } else if (method == u"connect") {
        if (const auto e = connectCore(params))
            replyError(s, id, InvalidParams, e->text(), e->field);
        else
            reply(s, id, {{u"ok"_s, true}});
    } else if (method == u"disconnect") {
        disconnectCore();
        reply(s, id, {{u"ok"_s, true}});
    } else if (method == u"status") {
        reply(s, id, m_state);
    } else if (method == u"subscribe") {
        m_clients[s].subscribed = true;
        QJsonArray logs;
        for (const QJsonObject &o : std::as_const(m_backlog))
            logs << o;
        reply(s, id, {{u"state"_s, m_state}, {u"logs"_s, logs}});
    } else if (method == u"testDelay") {
        testDelay(s, id, params);
    } else if (method == u"diagnostics") {
        diagnostics(s, id);
    } else if (method == u"stopOtherVpns") {
        const QStringList stopped = platform::stopOtherVpns();
        if (!stopped.isEmpty())
            log(u"helper"_s, u"warning"_s, u"остановлены другие VPN: %1"_s.arg(stopped.join(u", "_s)));
        reply(s, id, {{u"stopped"_s, QJsonArray::fromStringList(stopped)}});
    } else {
        replyError(s, id, MethodNotFound, u"неизвестный метод"_s);
    }
}

QJsonObject Helper::helloResult() const
{
    return {{u"helperVersion"_s, QStringLiteral(SUPERPUPERVPN_VERSION)}, {u"coreVersion"_s, m_coreVersion},
            {u"protocol"_s, ipc::kProtocolMajor},
            {u"capabilities"_s, QJsonArray{u"tun"_s, u"proxy"_s, u"testDelay"_s, u"diagnostics"_s, u"stopOtherVpns"_s}}};
}

std::optional<FieldError> Helper::connectCore(const QJsonObject &params)
{
    const QJsonObject pj = params.value(u"profile"_s).toObject();
    if (const auto e = rejectUnknownKeys(pj))
        return e;
    if (pj.value(u"protocol"_s) != u"vless"_s)
        return FieldError{FieldError::UnsupportedProtocol, u"protocol"_s, {}};
    Profile p = Profile::fromJson(pj);
    if (!validProfileId(p.id))
        return FieldError{FieldError::Format, u"id"_s, u"неверный идентификатор профиля"_s};
    if (const auto e = validate(p))
        return e;
    if (p.insecure && !p.insecureConsent)
        return FieldError{FieldError::Format, u"allowInsecure"_s, u"небезопасный профиль требует явного согласия"_s};

    CoreSettings s;
    s.mode = params.value(u"mode"_s).toString();
    s.dohUrl = params.value(u"dns"_s).toString(s.dohUrl);
    s.tunMtu = params.value(u"mtu"_s).toInt(0);
    s.logLevel = params.value(u"logLevel"_s).toString(s.logLevel);
    if (const auto e = validate(s))
        return e;

    m_profile = p;
    m_settings = s;
    m_maskHosts = params.value(u"maskHosts"_s).toBool(true);
    m_wanted = true;
    m_attempts = 0;
    QTimer::singleShot(0, this, &Helper::startCore);
    return std::nullopt;
}

void Helper::disconnectCore()
{
    const bool wasRunning = m_core->isRunning();
    m_wanted = false;
    m_retry->stop();
    m_stable->stop();
    m_core->stop();
    if (wasRunning)
        platform::cleanupStaleNetwork();
    setState(u"disconnected"_s);
}

void Helper::startCore()
{
    if (!m_wanted)
        return;
    m_retry->stop();
    if (m_core->isRunning())
        m_core->stop();
    platform::cleanupStaleNetwork();

    QString error;
    if (!platform::ensurePrivateDir(platform::dataDir(), &error) || !verifyCore(&error)) {
        m_wanted = false;
        log(u"helper"_s, u"error"_s, error);
        setState(u"error"_s, {{u"message"_s, error}});
        return;
    }
    CoreRuntime rt = m_core->prepare();
    if (m_settings.mode == u"proxy") {
        rt.proxyPort = pickProxyPort(m_attempts > 0 ? m_proxyPort : 0);
        if (!rt.proxyPort) {
            m_wanted = false;
            setState(u"error"_s, {{u"message"_s, u"нет свободного локального порта"_s}});
            return;
        }
    }
#ifdef Q_OS_LINUX
    rt.autoRedirect = m_settings.mode == u"tun";
#endif
    m_proxyPort = rt.proxyPort;
    for (const QString &secret : m_profile.secrets())
        m_core->masker.addSecret(secret);
    m_core->masker.addHost(m_profile.host);
    m_core->masker.setMaskHosts(m_maskHosts);
    for (const QString &k : m_profile.unknownParams.keys())
        log(u"helper"_s, u"info"_s, u"неизвестный параметр ссылки: %1"_s.arg(k));

    setState(u"connecting"_s, {{u"profileId"_s, m_profile.id}, {u"mode"_s, m_settings.mode}, {u"attempt"_s, m_attempts}});
    m_core->start(buildCoreConfig(m_profile, m_settings, rt));
}

void Helper::onCoreReady()
{
    QUrlQuery q{{u"url"_s, kDefaultDelayUrl}, {u"timeout"_s, u"5000"_s}};
    QNetworkReply *r = m_core->get(u"/proxies/proxy/delay?"_s + q.toString(QUrl::FullyEncoded), 8000);
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        if (!m_wanted || !m_core->isReady())
            return;
        const QJsonObject o = QJsonDocument::fromJson(r->readAll()).object();
        if (r->error() != QNetworkReply::NoError || !o.contains(u"delay"_s)) {
            QString why = u"сервер не отвечает (%1)"_s.arg(o.value(u"message"_s).toString(r->errorString()));
            if (const QStringList other = platform::conflictingAdapters(); !other.isEmpty())
                why += u". Включён другой VPN (%1) — выключите его"_s.arg(other.join(u", "_s));
            log(u"helper"_s, u"warning"_s, why);
            m_core->stop();
            onCoreFailed(m_attempts == 0, why);
            return;
        }
        if (const QStringList other = platform::conflictingAdapters(); !other.isEmpty())
            log(u"helper"_s, u"warning"_s, u"включён другой VPN (%1): трафик может идти мимо SuperPuperVPN"_s.arg(other.join(u", "_s)));
        m_stable->start();
        m_core->watchTraffic();
        setState(u"connected"_s, {{u"profileId"_s, m_profile.id}, {u"mode"_s, m_settings.mode},
                                  {u"proxyPort"_s, m_proxyPort}, {u"delay"_s, o.value(u"delay"_s)},
                                  {u"since"_s, QDateTime::currentMSecsSinceEpoch()}});
    });
}

void Helper::onCoreFailed(bool beforeReady, const QString &error)
{
    if (!m_wanted)
        return;
    m_stable->stop();
    if (beforeReady && m_attempts == 0) {
        m_wanted = false;
        platform::cleanupStaleNetwork();
        setState(u"error"_s, {{u"message"_s, error.isEmpty() ? u"ядро не запустилось"_s : error}});
        return;
    }
    if (m_attempts >= 5) {
        m_wanted = false;
        platform::cleanupStaleNetwork();
        setState(u"error"_s, {{u"message"_s, u"ядро завершилось: %1"_s.arg(error)}});
        return;
    }
    ++m_attempts;
    log(u"helper"_s, u"warning"_s, u"ядро завершилось, попытка %1 из 5: %2"_s.arg(m_attempts).arg(error));
    setState(u"connecting"_s, {{u"profileId"_s, m_profile.id}, {u"mode"_s, m_settings.mode}, {u"attempt"_s, m_attempts}});
    m_retry->start(1000 << (m_attempts - 1));
}

void Helper::setState(const QString &state, QJsonObject extra)
{
    extra[u"state"_s] = state;
    m_state = extra;
    broadcast(u"state"_s, m_state);
}

void Helper::onNetworkChanged()
{
    if (!m_wanted || !m_core->isReady())
        return;
    log(u"helper"_s, u"info"_s, u"сеть изменилась — сбрасываю соединения"_s);
    QNetworkReply *r = m_core->del(u"/connections"_s);
    connect(r, &QNetworkReply::finished, r, &QObject::deleteLater);
}

void Helper::testDelay(QLocalSocket *s, const QJsonValue &id, const QJsonObject &params)
{
    if (m_delayJob) {
        replyError(s, id, Busy, u"проверка задержки уже идёт"_s);
        return;
    }
    const QJsonArray arr = params.value(u"profiles"_s).toArray();
    if (arr.isEmpty() || arr.size() > 5000) {
        replyError(s, id, InvalidParams, u"нужно от 1 до 5000 профилей"_s, u"profiles"_s);
        return;
    }
    QList<Profile> profiles;
    for (qsizetype i = 0; i < arr.size(); ++i) {
        const QJsonObject pj = arr[i].toObject();
        auto e = rejectUnknownKeys(pj);
        Profile p = Profile::fromJson(pj);
        if (!e && !validProfileId(p.id))
            e = FieldError{FieldError::Format, u"id"_s, u"неверный идентификатор"_s};
        if (!e)
            e = validate(p);
        if (e) {
            replyError(s, id, InvalidParams, e->text(), u"profiles[%1].%2"_s.arg(i).arg(e->field));
            return;
        }
        p.insecureConsent = p.insecureConsent && p.insecure;
        profiles << p;
    }
    const QString url = params.value(u"url"_s).toString(kDefaultDelayUrl);
    if (!validTestUrl(url)) {
        replyError(s, id, InvalidParams, u"ожидается https://-адрес"_s, u"url"_s);
        return;
    }
    QString error;
    if (!verifyCore(&error)) {
        replyError(s, id, Failed, error);
        return;
    }

    auto *job = new DelayJob(this);
    m_delayJob = job;
    job->socket = s;
    job->id = id;
    job->url = url;
    job->timeoutMs = std::clamp(params.value(u"timeoutMs"_s).toInt(5000), 1000, 10000);
    job->concurrency = std::clamp(params.value(u"concurrency"_s).toInt(8), 1, 8);
    for (const Profile &p : profiles)
        job->queue << p.id;
    job->total = int(profiles.size());

    CoreRuntime rt = m_test->prepare();
    for (const Profile &p : profiles) {
        for (const QString &secret : p.secrets())
            m_test->masker.addSecret(secret);
        m_test->masker.addHost(p.host);
    }
    const QString iface = m_settings.mode == u"tun" && m_core->isRunning() ? platform::physicalInterface() : QString();

    auto finishJob = [this, job] {
        reply(job->socket, job->id, {{u"count"_s, job->done}});
        m_test->stop();
        m_delayJob = nullptr;
        job->deleteLater();
    };
    auto measure = std::make_shared<std::function<void(QString, int, QList<int>)>>();
    auto next = std::make_shared<std::function<void()>>();
    *next = [this, job, measure, finishJob] {
        while (job->inflight < job->concurrency && !job->queue.isEmpty()) {
            ++job->inflight;
            (*measure)(job->queue.takeFirst(), 0, {});
        }
        if (job->inflight == 0 && job->queue.isEmpty())
            finishJob();
    };
    *measure = [this, job, measure, next](QString pid, int round, QList<int> samples) {
        QUrlQuery q{{u"url"_s, job->url}, {u"timeout"_s, QString::number(job->timeoutMs)}};
        QNetworkReply *r = m_test->get(u"/proxies/%1/delay?%2"_s.arg(pid, q.toString(QUrl::FullyEncoded)), job->timeoutMs + 3000);
        connect(r, &QNetworkReply::finished, job, [=, this] {
            r->deleteLater();
            const QJsonObject o = QJsonDocument::fromJson(r->readAll()).object();
            QList<int> s = samples;
            if (round > 0 && o.contains(u"delay"_s))
                s << o.value(u"delay"_s).toInt();
            if (round < 3) {
                (*measure)(pid, round + 1, s);
                return;
            }
            QJsonObject result{{u"profileId"_s, pid}};
            if (s.isEmpty()) {
                result[u"error"_s] = o.value(u"message"_s).toString(u"нет ответа"_s);
            } else {
                std::sort(s.begin(), s.end());
                result[u"ms"_s] = s.at(s.size() / 2);
            }
            notify(job->socket, u"delay"_s, result);
            ++job->done;
            --job->inflight;
            (*next)();
        });
    };
    connect(m_test, &CoreProcess::ready, job, [next] { (*next)(); });
    connect(m_test, &CoreProcess::exited, job, [this, job, finishJob](bool, const QString &err) {
        for (const QString &pid : std::as_const(job->queue))
            notify(job->socket, u"delay"_s, {{u"profileId"_s, pid}, {u"error"_s, err.isEmpty() ? u"ядро проверки завершилось"_s : err}});
        job->queue.clear();
        job->inflight = 0;
        finishJob();
    });
    m_test->start(buildTestConfig(profiles, rt, iface));
}

void Helper::diagnostics(QLocalSocket *s, const QJsonValue &id)
{
    QJsonObject d{
        {u"tunPresent"_s, platform::tunPresent()},
        {u"conflictingAdapters"_s, QJsonArray::fromStringList(platform::conflictingAdapters())},
        {u"ipForwarding"_s, platform::ipForwarding()},
        {u"hotspot"_s, platform::hotspotActive()},
        {u"dnsMode"_s, m_core->isReady() ? (m_settings.mode == u"tun" ? u"fake-ip"_s : u"redir-host"_s) : u"system"_s},
        {u"helperVersion"_s, QStringLiteral(SUPERPUPERVPN_VERSION)},
        {u"coreVersion"_s, m_coreVersion},
    };
    if (!m_core->isReady()) {
        reply(s, id, d);
        return;
    }
    m_probeNam->setProxy(m_settings.mode == u"proxy"
                             ? QNetworkProxy(QNetworkProxy::HttpProxy, u"127.0.0.1"_s, quint16(m_proxyPort))
                             : QNetworkProxy(QNetworkProxy::NoProxy));
    QNetworkRequest req(QUrl(u"https://api.ipify.org"_s));
    req.setTransferTimeout(5000);
    QNetworkReply *r = m_probeNam->get(req);
    QPointer<QLocalSocket> sp(s);
    connect(r, &QNetworkReply::finished, this, [this, r, sp, id, d]() mutable {
        r->deleteLater();
        const QString ip = QString::fromLatin1(r->readAll().trimmed()).left(45);
        if (r->error() == QNetworkReply::NoError && !QHostAddress(ip).isNull())
            d[u"externalIp"_s] = ip;
        reply(sp, id, d);
    });
}

}
