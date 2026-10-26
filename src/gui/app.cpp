#include "app.h"

#include "configgen.h"
#include "helperclient.h"
#include "i18n.h"
#include "linkparser.h"
#include "platformgui.h"
#include "subscription.h"
#include "theme_tokens.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QHostInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QSaveFile>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>
#include <QVersionNumber>

using namespace Qt::StringLiterals;

namespace rl {
namespace {
App *s_instance = nullptr;

QIcon stateIcon(const QString &state)
{
    const QColor c = state == u"connected" ? QColor(0x16, 0xA3, 0x4A)
                   : state == u"connecting" ? QColor(0xD9, 0x77, 0x06) : QColor(0x6B, 0x72, 0x80);
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(4, 4, 56, 56);
    p.setPen(QPen(Qt::white, 6, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawArc(18, 18, 28, 28, 120 * 16, 300 * 16);
    p.drawLine(32, 14, 32, 30);
    return QIcon(pm);
}

QVariantMap previewOf(const QString &source, const QString &domain, const ImportResult &r, const QString &title = {})
{
    return {{u"source"_s, source}, {u"domain"_s, domain}, {u"found"_s, r.profiles.size()},
            {u"skipped"_s, r.skipped.size()}, {u"reasons"_s, r.skipped}, {u"title"_s, title}};
}

}

App::App(Storage *storage, bool startHidden, QObject *parent)
    : QObject(parent),
      m_storage(storage),
      m_helper(new HelperClient(this)),
      m_sysproxy(storage->dir()),
      m_nam(new QNetworkAccessManager(this)),
      m_profiles(new ProfileModel(this)),
      m_appLogs(new LogModel(this)),
      m_coreLogs(new LogModel(this)),
      m_proxyGuard(new QTimer(this)),
      m_hourly(new QTimer(this)),
      m_startHidden(startHidden)
{
    if (QFile geo(u":/geo/country.bin"_s); geo.open(QIODevice::ReadOnly))
        m_geo = GeoIp(geo.readAll());
    s_instance = this;
    m_reduceMotion = gui::reduceMotion();
    QString language = m_storage->settings.value(u"language"_s).toString();
    if (!setLanguage(language))
        setLanguage(language = u"en"_s);
    m_helper->setLanguage(language);
    connect(m_helper, &HelperClient::readyChanged, this, [this](bool ready) {
        emit helperChanged();
        if (ready) {
            m_helperError.clear();
            runDiagnostics();
            if (std::exchange(m_connectWhenReady, false))
                connectNow();
        }
    });
    connect(m_helper, &HelperClient::subscribed, this, &App::onSubscribed);
    connect(m_helper, &HelperClient::fatal, this, [this](const QString &msg) {
        m_helperError = msg;
        appLog(u"error"_s, msg);
        emit helperChanged();
    });
    connect(m_helper, &HelperClient::notification, this, [this](const QString &method, const QJsonObject &p) {
        if (method == u"state") {
            onHelperState(p);
        } else if (method == u"traffic") {
            m_traffic = p.toVariantMap();
            emit trafficChanged();
        } else if (method == u"log") {
            const QString msg = m_masker.mask(p.value(u"msg"_s).toString());
            (p.value(u"source"_s) == u"core"_s ? m_coreLogs : m_appLogs)
                ->append(p.value(u"time"_s).toInteger(QDateTime::currentMSecsSinceEpoch()), p.value(u"level"_s).toString(),
                         p.value(u"source"_s) == u"core"_s ? msg : tr("service: ") + msg);
        } else if (method == u"delay") {
            m_delays.insert(p.value(u"profileId"_s).toString(), p.contains(u"ms"_s) ? p.value(u"ms"_s).toInt() : -2);
            delaysChanged();
        }
    });

    m_proxyGuard->setInterval(5000);
    connect(m_proxyGuard, &QTimer::timeout, this, [this] {
        if (m_state == u"connected" && m_proxyApplied && !SystemProxy::pointsTo(m_proxyPort)) {
            emit toast(tr("Another app changed the proxy settings; they have been restored"));
            appLog(u"warning"_s, tr("the system proxy was overwritten by another app"));
            applySystemProxy();
        }
    });
    m_hourly->setInterval(10 * 60 * 1000);
    connect(m_hourly, &QTimer::timeout, this, [this] {
        autoUpdateSubscriptions();
        checkForUpdates();
    });
}

App::~App()
{
    s_instance = nullptr;
}

App *App::create(QQmlEngine *, QJSEngine *engine)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    Q_UNUSED(engine);
    return s_instance;
}

void App::start(bool autostarted)
{
    Q_UNUSED(autostarted);
    setupTray();
    gui::registerUrlSchemes();
    gui::setAutostart(m_storage->settings.value(u"autostart"_s).toBool());
    rebuildMasker();
    m_profiles->rebuild();
    resolveCountries();
    m_connectWhenReady = m_storage->settings.value(u"connectOnStart"_s).toBool();
    m_helper->start();
    QTimer::singleShot(3000, this, [this] { recoverSystemProxy({}); });
    QTimer::singleShot(5000, this, [this] {
        autoUpdateSubscriptions();
        checkForUpdates();
    });
    m_hourly->start();
}

void App::handleArgs(const QStringList &args)
{
    for (const QString &a : args) {
        if (a.size() > kMaxLinkBytes)
            continue;
        const QString lower = a.left(12).toLower();
        if (lower.startsWith(u"vless://")) {
            const ImportResult r = parseImport(a.toUtf8());
            m_pendingProfiles = r.profiles;
            m_pendingSubscription = {};
            emit showWindowRequested();
            emit importPreviewReady(previewOf(tr("Link"), r.profiles.isEmpty() ? QString() : r.profiles.first().host, r));
        } else if (lower.startsWith(u"superpupervpn://")) {
            const QUrl u(a);
            const QString url = QUrlQuery(u).queryItemValue(u"url"_s, QUrl::FullyDecoded);
            emit showWindowRequested();
            if (u.host() != u"import" || url.isEmpty()) {
                emit importPreviewReady({{u"error"_s, tr("Invalid superpupervpn:// link")}});
                continue;
            }
            previewSubscription(url, false, u.fragment(QUrl::FullyDecoded).left(64));
        }
    }
}

void App::appLog(const QString &level, const QString &message)
{
    m_appLogs->append(QDateTime::currentMSecsSinceEpoch(), level, m_masker.mask(message));
}

QVariantMap App::current() const
{
    const Profile *p = find(m_storage->settings.value(u"selectedProfileId"_s).toString());
    if (!p)
        return {};
    return {{u"id"_s, p->id}, {u"name"_s, ProfileModel::splitFlag(p->name).second}, {u"flag"_s, flagOf(*p)}, {u"badge"_s, p->badge()},
            {u"delay"_s, delayOf(p->id)}, {u"insecure"_s, p->insecure}};
}

void App::setSearch(const QString &s)
{
    m_search = s;
    m_profiles->rebuild();
    emit listChanged();
}

QString App::sortMode() const
{
    return m_storage->settings.value(u"sortMode"_s).toString();
}

void App::setSortMode(const QString &m)
{
    setSetting(u"sortMode"_s, m);
    emit listChanged();
}

bool App::helperReady() const
{
    return m_helper->isReady();
}

bool App::darkTheme() const
{
    return m_storage->settings.value(u"theme"_s).toString() != u"light";
}

QVariantMap App::colors() const
{
    QVariantMap m;
    const bool dark = darkTheme();
    for (const theme::Token &t : theme::kTokens)
        m.insert(QString::fromLatin1(t.name), QString::fromLatin1(dark ? t.dark : t.light));
    return m;
}

bool App::trayAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

QString App::version() const
{
    return QStringLiteral(SUPERPUPERVPN_VERSION);
}

QJsonObject App::subscription(const QString &id) const
{
    for (const QJsonValue v : m_storage->subscriptions)
        if (v.toObject().value(u"id"_s).toString() == id)
            return v.toObject();
    return {};
}

void App::setSetting(const QString &key, const QVariant &value)
{
    QJsonObject &s = m_storage->settings;
    if (!s.contains(key) || key == u"version")
        return;
    const QJsonValue v = QJsonValue::fromVariant(value);
    if (s.value(key) == v)
        return;
    static const QStringList coreKeys = {u"mode"_s, u"dohUrl"_s, u"mtu"_s, u"logLevel"_s, u"maskHosts"_s};
    if (coreKeys.contains(key)) {
        QJsonObject probe = s;
        probe[key] = v;
        CoreSettings cs;
        cs.mode = probe.value(u"mode"_s).toString();
        cs.dohUrl = probe.value(u"dohUrl"_s).toString();
        cs.tunMtu = probe.value(u"mtu"_s).toInt();
        cs.logLevel = probe.value(u"logLevel"_s).toString();
        if (const auto e = validate(cs)) {
            emit toast(e->text());
            emit settingsChanged();
            return;
        }
    }
    if (key == u"language" && !setLanguage(v.toString()))
        return;
    s[key] = v;
    m_storage->saveSettings();
    if (key == u"language") {
        m_helper->setLanguage(v.toString());
        m_profiles->rebuild();
    }
    if (key == u"autostart")
        gui::setAutostart(v.toBool());
    if (key == u"maskHosts")
        rebuildMasker();
    if (key == u"sortMode" || key == u"collapsedGroups")
        m_profiles->rebuild();
    emit settingsChanged();
    if (coreKeys.contains(key) && (m_state == u"connected" || m_state == u"connecting"))
        doConnect();
    updateTray();
}

void App::toggleConnection()
{
    if (m_state == u"connected" || m_state == u"connecting")
        disconnectNow();
    else
        connectNow();
}

void App::connectNow()
{
    if (!m_helper->isReady()) {
        emit toast(tr("The service is not running"));
        return;
    }
    QString id = m_storage->settings.value(u"selectedProfileId"_s).toString();
    if (!find(id) && !m_storage->profiles.isEmpty()) {
        id = m_storage->profiles.first().id;
        selectProfile(id);
    }
    const Profile *p = find(id);
    if (!p) {
        emit toast(tr("Add a server first"));
        return;
    }
    if (p->insecure && !p->insecureConsent) {
        emit confirmInsecure(p->id);
        return;
    }
    m_helper->call(u"diagnostics"_s, {}, [this](const QJsonObject &d, const QJsonObject &) {
        QStringList issues;
        for (const QJsonValue a : d.value(u"conflictingAdapters"_s).toArray())
            issues << tr("another VPN or TUN: %1").arg(a.toString());
        if (d.value(u"hotspot"_s).toBool())
            issues << tr("Windows Mobile Hotspot is on");
        if (d.value(u"ipForwarding"_s).toBool())
            issues << tr("IP forwarding is on");
        m_diag = d.toVariantMap();
        emit diagnosticsChanged();
        const QString signature = issues.join(u'|');
        if (!issues.isEmpty() && m_storage->settings.value(u"mode"_s) == u"tun"_s
            && signature != m_storage->settings.value(u"ackConflicts"_s).toString()) {
            emit confirmConflicts(u"• "_s + issues.join(u"\n• "_s));
            return;
        }
        doConnect();
    });
}

void App::acknowledgeConflicts()
{
    QStringList issues;
    for (const QVariant &a : m_diag.value(u"conflictingAdapters"_s).toList())
        issues << tr("another VPN or TUN: %1").arg(a.toString());
    if (m_diag.value(u"hotspot"_s).toBool())
        issues << tr("Windows Mobile Hotspot is on");
    if (m_diag.value(u"ipForwarding"_s).toBool())
        issues << tr("IP forwarding is on");
    setSetting(u"ackConflicts"_s, issues.join(u'|'));
    doConnect();
}

void App::stopOtherVpns(bool connectAfter)
{
    m_helper->call(u"stopOtherVpns"_s, {}, [this, connectAfter](const QJsonObject &r, const QJsonObject &e) {
        const QStringList stopped = r.value(u"stopped"_s).toVariant().toStringList();
        if (!e.isEmpty() || stopped.isEmpty()) {
            emit toast(e.isEmpty() ? tr("Could not turn off other VPNs") : e.value(u"message"_s).toString());
            runDiagnostics();
            return;
        }
        emit toast(tr("Turned off: %1").arg(stopped.join(u", "_s)));
        if (connectAfter)
            connectNow();
        else
            runDiagnostics();
    });
}

void App::doConnect()
{
    const Profile *p = find(m_storage->settings.value(u"selectedProfileId"_s).toString());
    if (!p)
        return;
    const QJsonObject &s = m_storage->settings;
    m_error.clear();
    m_helper->call(u"connect"_s,
                   {{u"profile"_s, p->toJson()}, {u"mode"_s, s.value(u"mode"_s)},
                    {u"dns"_s, s.value(u"dohUrl"_s)}, {u"mtu"_s, s.value(u"mtu"_s)}, {u"logLevel"_s, s.value(u"logLevel"_s)},
                    {u"maskHosts"_s, s.value(u"maskHosts"_s)}},
                   [this](const QJsonObject &, const QJsonObject &e) {
                       if (e.isEmpty())
                           return;
                       m_state = u"error"_s;
                       m_error = e.value(u"message"_s).toString();
                       emit stateChanged();
                       updateTray();
                   });
}

void App::disconnectNow()
{
    m_helper->call(u"disconnect"_s);
}

void App::selectProfile(const QString &id)
{
    if (!find(id))
        return;
    m_storage->settings[u"selectedProfileId"_s] = id;
    m_storage->saveSettings();
    m_profiles->refresh({ProfileModel::SelectedRole});
    emit currentChanged();
    emit settingsChanged();
    if (m_state == u"connected" || m_state == u"connecting")
        connectNow();
}

void App::setInsecureConsent(const QString &id, bool consent)
{
    if (Profile *p = find(id)) {
        p->insecureConsent = consent && p->insecure;
        saveProfiles();
    }
}

void App::onSubscribed(const QJsonObject &r)
{
    for (const QJsonValue v : r.value(u"logs"_s).toArray()) {
        const QJsonObject l = v.toObject();
        (l.value(u"source"_s) == u"core"_s ? m_coreLogs : m_appLogs)
            ->append(l.value(u"time"_s).toInteger(), l.value(u"level"_s).toString(), m_masker.mask(l.value(u"msg"_s).toString()));
    }
    const QJsonObject state = r.value(u"state"_s).toObject();
    recoverSystemProxy(state);
    onHelperState(state);
}

void App::onHelperState(const QJsonObject &s)
{
    const QString prev = m_state;
    m_state = s.value(u"state"_s).toString(u"disconnected"_s);
    m_error = s.value(u"message"_s).toString();
    m_since = m_state == u"connected" ? s.value(u"since"_s).toInteger() : 0;
    m_proxyPort = s.value(u"proxyPort"_s).toInt();
    const bool proxyMode = s.value(u"mode"_s) == u"proxy"_s;

    if (m_state == u"connected") {
        if (s.contains(u"delay"_s))
            m_delays.insert(s.value(u"profileId"_s).toString(), s.value(u"delay"_s).toInt());
        if (proxyMode && (!m_proxyApplied || !SystemProxy::pointsTo(m_proxyPort)))
            applySystemProxy();
        else if (!proxyMode && m_proxyApplied)
            restoreSystemProxy();
        QJsonArray recent = m_storage->settings.value(u"recentProfileIds"_s).toArray();
        const QString id = s.value(u"profileId"_s).toString();
        for (qsizetype i = recent.size() - 1; i >= 0; --i)
            if (recent[i].toString() == id)
                recent.removeAt(i);
        recent.prepend(id);
        while (recent.size() > 5)
            recent.removeLast();
        m_storage->settings[u"recentProfileIds"_s] = recent;
        m_storage->saveSettings();
    } else if (m_state == u"disconnected" || m_state == u"error") {
        restoreSystemProxy();
        m_traffic.clear();
        emit trafficChanged();
        if (m_state == u"error" && prev != u"error")
            appLog(u"error"_s, m_error);
    }
    emit stateChanged();
    emit currentChanged();
    updateTray();
    if (m_quitting && m_state != u"connected" && m_state != u"connecting")
        QCoreApplication::quit();
}

void App::applySystemProxy()
{
    QString error;
    if (m_sysproxy.apply(m_proxyPort, &error)) {
        m_proxyApplied = true;
        m_proxyGuard->start();
    } else {
        appLog(u"error"_s, error);
        emit toast(error);
    }
}

void App::restoreSystemProxy()
{
    m_proxyGuard->stop();
    if (!m_proxyApplied && !m_sysproxy.markerPort())
        return;
    QString error;
    if (!m_sysproxy.restore(&error)) {
        appLog(u"error"_s, error);
        emit toast(error);
    }
    m_proxyApplied = false;
}

void App::recoverSystemProxy(const QJsonObject &helperState)
{
    if (std::exchange(m_recovered, true))
        return;
    const int marker = m_sysproxy.markerPort();
    if (!marker)
        return;
    if (helperState.value(u"state"_s) == u"connected"_s && helperState.value(u"mode"_s) == u"proxy"_s
        && helperState.value(u"proxyPort"_s).toInt() == marker) {
        m_proxyApplied = true;
        m_proxyPort = marker;
        m_proxyGuard->start();
        return;
    }
    if (SystemProxy::pointsTo(marker)) {
        QString error;
        if (m_sysproxy.restore(&error))
            appLog(u"warning"_s, tr("the system proxy was restored after a crash"));
        else
            appLog(u"error"_s, error);
    } else {
        m_sysproxy.forget();
    }
}

QVariantMap App::importFromClipboard()
{
    const QString text = QGuiApplication::clipboard()->text();
    const ImportResult r = parseImport(text.toUtf8());
    for (const QString &k : r.unknownKeys)
        appLog(u"info"_s, tr("unknown link parameter: %1").arg(k));
    m_pendingProfiles = r.profiles;
    m_pendingSubscription = {};
    QStringList hosts;
    for (const Profile &p : r.profiles)
        if (!hosts.contains(p.host) && hosts.size() < 3)
            hosts << p.host;
    return previewOf(tr("Clipboard"), hosts.join(u", "_s), r);
}

void App::previewSubscription(const QString &url, bool allowHttp, const QString &name)
{
    const QUrl u(url.trimmed(), QUrl::StrictMode);
    if (url.size() > kMaxLinkBytes || !u.isValid() || u.host().isEmpty() || (u.scheme() != u"https" && u.scheme() != u"http")) {
        emit importPreviewReady({{u"error"_s, tr("An https:// link is required")}});
        return;
    }
    if (u.scheme() == u"http" && !allowHttp) {
        emit importPreviewReady({{u"needHttpConfirm"_s, true}, {u"url"_s, url}, {u"name"_s, name}});
        return;
    }
    emit importPreviewReady({{u"loading"_s, true}, {u"domain"_s, u.host()}});
    fetchSubscription(u.toString(), allowHttp, [=, this](QByteArray body, SubscriptionInfo info, QString error) {
        if (!error.isEmpty()) {
            emit importPreviewReady({{u"error"_s, error}, {u"domain"_s, u.host()}});
            return;
        }
        const ImportResult r = parseImport(body);
        m_pendingProfiles = r.profiles;
        const QString title = !name.isEmpty() ? name : !info.title.isEmpty() ? info.title : u.host();
        m_pendingSubscription = {{u"id"_s, newId()}, {u"url"_s, u.toString()}, {u"name"_s, title}, {u"allowHttp"_s, allowHttp},
                                 {u"info"_s, info.toJson()}, {u"updatedAt"_s, QDateTime::currentSecsSinceEpoch()},
                                 {u"error"_s, QString()}, {u"skipped"_s, r.skipped.size()}};
        emit importPreviewReady(previewOf(tr("Subscription"), u.host(), r, title));
    });
}

void App::confirmImport()
{
    const QString subId = m_pendingSubscription.value(u"id"_s).toString();
    QString firstId;
    for (Profile p : std::as_const(m_pendingProfiles)) {
        p.id = newId();
        p.subscriptionId = subId;
        if (firstId.isEmpty())
            firstId = p.id;
        m_storage->profiles << p;
    }
    if (!subId.isEmpty())
        m_storage->subscriptions << m_pendingSubscription;
    const int added = int(m_pendingProfiles.size());
    m_pendingProfiles.clear();
    m_pendingSubscription = {};
    saveProfiles();
    if (!find(m_storage->settings.value(u"selectedProfileId"_s).toString()) && !firstId.isEmpty())
        selectProfile(firstId);
    emit toast(tr("Servers added: %1").arg(added));
}

void App::cancelImport()
{
    m_pendingProfiles.clear();
    m_pendingSubscription = {};
}

void App::fetchSubscription(const QString &url, bool allowHttp,
                            std::function<void(QByteArray, SubscriptionInfo, QString)> done)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, u"SuperPuperVPN/%1 (%2)"_s.arg(version(), gui::osName()));
    req.setTransferTimeout(30000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     allowHttp ? QNetworkRequest::SameOriginRedirectPolicy : QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *r = m_nam->get(req);
    auto body = std::make_shared<QByteArray>();
    connect(r, &QNetworkReply::readyRead, this, [r, body] {
        *body += r->readAll();
        if (body->size() > kMaxBodyBytes)
            r->abort();
    });
    connect(r, &QNetworkReply::finished, this, [this, r, body, url, done] {
        r->deleteLater();
        *body += r->readAll();
        if (body->size() > kMaxBodyBytes) {
            done({}, {}, tr("The subscription is larger than 5 MB"));
            return;
        }
        if (r->error() != QNetworkReply::NoError) {
            appLog(u"warning"_s, tr("subscription %1: %2").arg(maskUrl(url), r->errorString()));
            done({}, {}, tr("Could not load the subscription: %1").arg(r->errorString()));
            return;
        }
        done(*body, parseSubscriptionHeaders(r->rawHeader("subscription-userinfo"), r->rawHeader("profile-update-interval"),
                                             r->rawHeader("profile-title"), r->rawHeader("support-url"),
                                             r->rawHeader("profile-web-page-url")),
             {});
    });
}

void App::refreshSubscription(const QString &id)
{
    const QJsonObject sub = subscription(id);
    if (sub.isEmpty() || m_updating.contains(id))
        return;
    m_updating.insert(id);
    m_profiles->rebuild();
    fetchSubscription(sub.value(u"url"_s).toString(), sub.value(u"allowHttp"_s).toBool(),
                      [this, id](QByteArray body, SubscriptionInfo info, QString error) {
        m_updating.remove(id);
        QJsonArray &subs = m_storage->subscriptions;
        qsizetype idx = -1;
        for (qsizetype i = 0; i < subs.size(); ++i)
            if (subs[i].toObject().value(u"id"_s).toString() == id)
                idx = i;
        if (idx < 0)
            return;
        QJsonObject sub = subs[idx].toObject();
        if (error.isEmpty()) {
            const ImportResult r = parseImport(body);
            QList<Profile> existing, others;
            qsizetype insertAt = -1;
            for (const Profile &p : std::as_const(m_storage->profiles)) {
                if (p.subscriptionId == id) {
                    if (insertAt < 0)
                        insertAt = others.size();
                    existing << p;
                } else {
                    others << p;
                }
            }
            const QList<Profile> merged = mergeSubscription(existing, r.profiles, id, [this] { return newId(); });
            if (insertAt < 0)
                insertAt = others.size();
            for (qsizetype i = 0; i < merged.size(); ++i)
                others.insert(insertAt + i, merged[i]);
            m_storage->profiles = others;
            sub[u"info"_s] = info.toJson();
            sub[u"updatedAt"_s] = QDateTime::currentSecsSinceEpoch();
            sub[u"error"_s] = QString();
            appLog(u"info"_s, tr("subscription %1 updated: %2 servers, %3 skipped")
                                  .arg(sub.value(u"name"_s).toString()).arg(merged.size()).arg(r.skipped.size()));
        } else {
            sub[u"error"_s] = error;
            sub[u"updatedAt"_s] = QDateTime::currentSecsSinceEpoch();
        }
        subs[idx] = sub;
        saveProfiles();
    });
}

void App::autoUpdateSubscriptions()
{
    const int userHours = m_storage->settings.value(u"subIntervalHours"_s).toInt();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const QJsonValue v : std::as_const(m_storage->subscriptions)) {
        const QJsonObject s = v.toObject();
        const int hours = userHours > 0 ? userHours : SubscriptionInfo::fromJson(s.value(u"info"_s).toObject()).updateIntervalHours;
        if (now - s.value(u"updatedAt"_s).toInteger() >= qint64(hours) * 3600)
            refreshSubscription(s.value(u"id"_s).toString());
    }
}

void App::removeSubscription(const QString &id)
{
    QJsonArray &subs = m_storage->subscriptions;
    for (qsizetype i = subs.size() - 1; i >= 0; --i)
        if (subs[i].toObject().value(u"id"_s).toString() == id)
            subs.removeAt(i);
    m_storage->profiles.removeIf([&](const Profile &p) { return p.subscriptionId == id; });
    saveProfiles();
}

void App::toggleGroup(const QString &id)
{
    QJsonArray c = m_storage->settings.value(u"collapsedGroups"_s).toArray();
    bool removed = false;
    for (qsizetype i = c.size() - 1; i >= 0; --i)
        if (c[i].toString() == id) {
            c.removeAt(i);
            removed = true;
        }
    if (!removed)
        c << id;
    setSetting(u"collapsedGroups"_s, c.toVariantList());
}

Profile *App::find(const QString &id)
{
    for (Profile &p : m_storage->profiles)
        if (p.id == id)
            return &p;
    return nullptr;
}

const Profile *App::find(const QString &id) const
{
    for (const Profile &p : m_storage->profiles)
        if (p.id == id)
            return &p;
    return nullptr;
}

QString App::newId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void App::saveProfiles()
{
    if (!m_storage->saveProfiles() && !m_storage->readOnly)
        emit toast(tr("Could not save the profiles"));
    profilesChanged();
}

void App::profilesChanged()
{
    rebuildMasker();
    m_profiles->rebuild();
    resolveCountries();
    emit currentChanged();
    updateTray();
}

void App::rebuildMasker()
{
    m_masker.clear();
    for (const Profile &p : std::as_const(m_storage->profiles)) {
        for (const QString &s : p.secrets())
            m_masker.addSecret(s);
        m_masker.addHost(p.host);
    }
    m_masker.setMaskHosts(m_storage->settings.value(u"maskHosts"_s).toBool());
}

QVariantMap App::profileDetails(const QString &id, bool reveal) const
{
    const Profile *p = find(id);
    if (!p)
        return {};
    const QString hidden = u"••••••••"_s;
    const QJsonObject sub = subscription(p->subscriptionId);
    return {{u"name"_s, p->name}, {u"host"_s, p->host}, {u"port"_s, p->port}, {u"network"_s, p->network},
            {u"security"_s, p->security}, {u"sni"_s, p->sni}, {u"fp"_s, p->fp}, {u"flow"_s, p->flow},
            {u"encryption"_s, p->encryption == u"none" ? p->encryption : (reveal ? p->encryption : hidden)},
            {u"path"_s, p->path}, {u"hostHeader"_s, p->hostHeader}, {u"serviceName"_s, p->serviceName},
            {u"mode"_s, p->mode}, {u"alpn"_s, p->alpn}, {u"spx"_s, p->spx},
            {u"uuid"_s, reveal ? p->uuid : hidden}, {u"pbk"_s, p->pbk.isEmpty() ? QString() : reveal ? p->pbk : hidden},
            {u"sid"_s, p->sid.isEmpty() ? QString() : reveal ? p->sid : hidden},
            {u"nonStandardId"_s, p->nonStandardId()}, {u"insecure"_s, p->insecure}, {u"insecureConsent"_s, p->insecureConsent},
            {u"subscription"_s, sub.value(u"name"_s).toString()}, {u"unknownKeys"_s, QStringList(p->unknownParams.keys())},
            {u"badge"_s, p->badge()}};
}

void App::renameProfile(const QString &id, const QString &name)
{
    Profile *p = find(id);
    if (!p)
        return;
    Profile probe = *p;
    probe.name = name.trimmed();
    if (const auto e = validate(probe); e || probe.name.isEmpty()) {
        emit toast(e ? e->text() : tr("The name cannot be empty"));
        return;
    }
    p->name = probe.name;
    saveProfiles();
}

void App::duplicateProfile(const QString &id)
{
    const Profile *p = find(id);
    if (!p)
        return;
    Profile copy = *p;
    copy.id = newId();
    copy.subscriptionId.clear();
    copy.name = (p->name + tr(" (copy)")).left(64);
    copy.pinned = false;
    m_storage->profiles << copy;
    saveProfiles();
}

void App::deleteProfile(const QString &id)
{
    m_storage->profiles.removeIf([&](const Profile &p) { return p.id == id; });
    m_delays.remove(id);
    saveProfiles();
}

void App::togglePin(const QString &id)
{
    if (Profile *p = find(id)) {
        p->pinned = !p->pinned;
        saveProfiles();
    }
}

void App::testAll()
{
    testProfiles(m_storage->profiles);
}

void App::testProfiles(const QList<Profile> &profiles)
{
    if (profiles.isEmpty())
        return;
    QJsonArray arr;
    QStringList ids;
    for (const Profile &p : profiles) {
        arr << p.toJson();
        ids << p.id;
        m_delays.insert(p.id, -3);
    }
    delaysChanged();
    if (m_testing) {
        for (const QString &id : std::as_const(ids))
            if (!m_testQueue.contains(id))
                m_testQueue << id;
        return;
    }
    m_testing = true;
    emit testingChanged();
    m_helper->call(u"testDelay"_s, {{u"profiles"_s, arr}, {u"timeoutMs"_s, 5000}, {u"concurrency"_s, 8}},
                   [this, ids](const QJsonObject &, const QJsonObject &e) {
                       m_testing = false;
                       emit testingChanged();
                       for (const QString &id : ids)
                           if (m_delays.value(id) == -3)
                               m_delays.remove(id);
                       delaysChanged();
                       if (!e.isEmpty())
                           emit toast(e.value(u"message"_s).toString());
                       QList<Profile> next;
                       for (const QString &id : std::exchange(m_testQueue, {}))
                           if (const Profile *p = find(id))
                               next << *p;
                       testProfiles(next);
                   });
}

void App::delaysChanged()
{
    if (sortMode() == u"delay")
        m_profiles->rebuild();
    else
        m_profiles->refresh({ProfileModel::DelayRole});
    emit currentChanged();
}

QString App::flagOf(const Profile &p) const
{
    const QString named = ProfileModel::splitFlag(p.name).first;
    if (!named.isEmpty())
        return named;
    const QHostAddress address(p.host);
    return flagEmoji(address.isNull() ? m_hostCountry.value(p.host) : m_geo.country(address));
}

void App::resolveCountries()
{
    for (const Profile &p : std::as_const(m_storage->profiles)) {
        if (!ProfileModel::splitFlag(p.name).first.isEmpty() || !QHostAddress(p.host).isNull()
            || m_hostCountry.contains(p.host) || m_resolving.contains(p.host))
            continue;
        m_resolving.insert(p.host);
        QHostInfo::lookupHost(p.host, this, [this, host = p.host](const QHostInfo &info) {
            m_resolving.remove(host);
            for (const QHostAddress &a : info.addresses()) {
                if (a.isLoopback() || a.isInSubnet(QHostAddress(u"198.18.0.0"_s), 15))
                    continue;
                m_hostCountry.insert(host, m_geo.country(a));
                m_profiles->refresh({ProfileModel::FlagRole});
                emit currentChanged();
                return;
            }
        });
    }
}

void App::runDiagnostics()
{
    m_helper->call(u"diagnostics"_s, {}, [this](const QJsonObject &d, const QJsonObject &) {
        m_diag = d.toVariantMap();
        emit diagnosticsChanged();
    });
}

void App::startService(bool restart)
{
    if (!gui::startService(restart))
        emit toast(tr("Could not request the service start"));
}

bool App::exportLogs(const QUrl &file)
{
    const QByteArray zip = makeZip({{u"app.log"_s, m_masker.mask(m_appLogs->text()).toUtf8()},
                                    {u"core.log"_s, m_masker.mask(m_coreLogs->text()).toUtf8()}});
    QSaveFile f(file.toLocalFile());
    const bool ok = f.open(QIODevice::WriteOnly) && f.write(zip) == zip.size() && f.commit();
    emit toast(ok ? tr("Logs saved") : tr("Could not save the logs"));
    return ok;
}

void App::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void App::openUrl(const QString &url)
{
    const QUrl u(url);
    if (u.scheme() == u"https" || u.scheme() == u"http")
        QDesktopServices::openUrl(u);
}

void App::quit()
{
    if (m_state == u"connected" || m_state == u"connecting") {
        m_quitting = true;
        disconnectNow();
        QTimer::singleShot(3000, qApp, [this] {
            restoreSystemProxy();
            QCoreApplication::quit();
        });
        return;
    }
    restoreSystemProxy();
    QCoreApplication::quit();
}

void App::checkForUpdates()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - m_storage->settings.value(u"lastUpdateCheck"_s).toInteger() < 24 * 3600)
        return;
    QNetworkRequest req(QUrl(u"https://api.github.com/repos/" SUPERPUPERVPN_GITHUB_REPO "/releases/latest"_s));
    req.setHeader(QNetworkRequest::UserAgentHeader, u"SuperPuperVPN/%1 (%2)"_s.arg(version(), gui::osName()));
    req.setTransferTimeout(15000);
    QNetworkReply *r = m_nam->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r, now] {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            appLog(u"info"_s, tr("update check failed: %1").arg(r->errorString()));
            return;
        }
        setSetting(u"lastUpdateCheck"_s, now);
        const QJsonObject o = QJsonDocument::fromJson(r->readAll()).object();
        QString tag = o.value(u"tag_name"_s).toString();
        if (tag.startsWith(u'v'))
            tag.remove(0, 1);
        const QString url = o.value(u"html_url"_s).toString();
        if (QVersionNumber::fromString(tag) > QVersionNumber::fromString(version()) && url.startsWith(u"https://github.com/")) {
            emit toast(tr("Version %1 is available").arg(tag));
            if (m_tray)
                m_tray->showMessage(u"SuperPuperVPN"_s, tr("Version %1 is available").arg(tag));
        }
    });
}

void App::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    m_tray = new QSystemTrayIcon(this);
    m_trayMenu = new QMenu;
    m_tray->setContextMenu(m_trayMenu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
            emit showWindowRequested();
    });
    updateTray();
    m_tray->show();
}

void App::updateTray()
{
    if (!m_tray)
        return;
    m_tray->setIcon(stateIcon(m_state));
    const QVariantMap cur = current();
    m_tray->setToolTip(u"SuperPuperVPN — %1"_s.arg(m_state == u"connected" ? tr("connected · %1").arg(cur.value(u"name"_s).toString())
                                                : m_state == u"connecting" ? tr("connecting…") : tr("disconnected")));
    m_trayMenu->clear();
    const bool on = m_state == u"connected" || m_state == u"connecting";
    m_trayMenu->addAction(on ? tr("Disconnect") : tr("Connect"), this, &App::toggleConnection);

    QMenu *modes = m_trayMenu->addMenu(tr("Mode"));
    const QString mode = m_storage->settings.value(u"mode"_s).toString();
    for (const auto &[key, label] : {std::pair{u"proxy"_s, tr("System proxy")}, std::pair{u"tun"_s, tr("TUN")}}) {
        QAction *a = modes->addAction(label, this, [this, key] { setSetting(u"mode"_s, key); });
        a->setCheckable(true);
        a->setChecked(mode == key);
    }

    QMenu *servers = m_trayMenu->addMenu(tr("Server"));
    QStringList ids;
    for (const Profile &p : std::as_const(m_storage->profiles))
        if (p.pinned)
            ids << p.id;
    for (const QJsonValue v : m_storage->settings.value(u"recentProfileIds"_s).toArray())
        if (!ids.contains(v.toString()) && find(v.toString()))
            ids << v.toString();
    const QString selected = m_storage->settings.value(u"selectedProfileId"_s).toString();
    for (const QString &id : ids) {
        QAction *a = servers->addAction(find(id)->name, this, [this, id] { selectProfile(id); });
        a->setCheckable(true);
        a->setChecked(id == selected);
    }
    servers->setEnabled(!ids.isEmpty());

    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tr("Open"), this, &App::showWindowRequested);
    m_trayMenu->addAction(tr("Quit"), this, &App::quit);
}

}
