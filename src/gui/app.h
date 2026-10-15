#pragma once

#include "geoip.h"
#include "masking.h"
#include "models.h"
#include "storage.h"
#include "subscription.h"
#include "sysproxy.h"

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

class QMenu;
class QNetworkAccessManager;
class QSystemTrayIcon;
class QTimer;

namespace rl {
class HelperClient;

class App : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool startHidden READ startHidden CONSTANT)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(qint64 connectedSince READ connectedSince NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap traffic READ traffic NOTIFY trafficChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(ProfileModel *profiles READ profiles CONSTANT)
    Q_PROPERTY(LogModel *appLogs READ appLogs CONSTANT)
    Q_PROPERTY(LogModel *coreLogs READ coreLogs CONSTANT)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY listChanged)
    Q_PROPERTY(QString sortMode READ sortMode WRITE setSortMode NOTIFY listChanged)
    Q_PROPERTY(bool testing READ testing NOTIFY testingChanged)
    Q_PROPERTY(bool helperReady READ helperReady NOTIFY helperChanged)
    Q_PROPERTY(QString helperError READ helperError NOTIFY helperChanged)
    Q_PROPERTY(QVariantMap diagnostics READ diagnostics NOTIFY diagnosticsChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY settingsChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)
    Q_PROPERTY(bool reduceMotion READ reduceMotion CONSTANT)
    Q_PROPERTY(QString warning READ warning CONSTANT)
    Q_PROPERTY(int proxyPort READ proxyPort NOTIFY stateChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QVariantMap settings READ settingsMap NOTIFY settingsChanged)

public:
    App(Storage *storage, bool startHidden, QObject *parent = nullptr);
    ~App() override;
    static App *create(QQmlEngine *, QJSEngine *engine);
    bool startHidden() const { return m_startHidden; }
    void start(bool autostarted);
    void handleArgs(const QStringList &args);
    void appLog(const QString &level, const QString &message);

    QString state() const { return m_state; }
    QString errorText() const { return m_error; }
    qint64 connectedSince() const { return m_since; }
    QVariantMap traffic() const { return m_traffic; }
    QVariantMap current() const;
    ProfileModel *profiles() const { return m_profiles; }
    LogModel *appLogs() const { return m_appLogs; }
    LogModel *coreLogs() const { return m_coreLogs; }
    QString search() const { return m_search; }
    void setSearch(const QString &s);
    QString sortMode() const;
    void setSortMode(const QString &m);
    bool testing() const { return m_testing; }
    bool helperReady() const;
    QString helperError() const { return m_helperError; }
    QVariantMap diagnostics() const { return m_diag; }
    bool darkTheme() const;
    QVariantMap colors() const;
    bool trayAvailable() const;
    bool reduceMotion() const { return m_reduceMotion; }
    QString warning() const { return m_storage->warning; }
    int proxyPort() const { return m_proxyPort; }
    QString version() const;
    QVariantMap settingsMap() const { return m_storage->settings.toVariantMap(); }

    const Storage &storage() const { return *m_storage; }
    QVariant delayOf(const QString &id) const { return m_delays.value(id, -1); }
    QString flagOf(const Profile &p) const;
    QJsonObject subscription(const QString &id) const;
    bool subscriptionUpdating(const QString &id) const { return m_updating.contains(id); }

    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    Q_INVOKABLE void toggleConnection();
    Q_INVOKABLE void connectNow();
    Q_INVOKABLE void disconnectNow();
    Q_INVOKABLE void selectProfile(const QString &id);
    Q_INVOKABLE void acknowledgeConflicts();
    Q_INVOKABLE void stopOtherVpns(bool connectAfter);
    Q_INVOKABLE void setInsecureConsent(const QString &id, bool consent);

    Q_INVOKABLE QVariantMap importFromClipboard();
    Q_INVOKABLE void previewSubscription(const QString &url, bool allowHttp, const QString &name = {});
    Q_INVOKABLE void confirmImport();
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE void refreshSubscription(const QString &id);
    Q_INVOKABLE void removeSubscription(const QString &id);
    Q_INVOKABLE void toggleGroup(const QString &id);

    Q_INVOKABLE QVariantMap profileDetails(const QString &id, bool reveal) const;
    Q_INVOKABLE void renameProfile(const QString &id, const QString &name);
    Q_INVOKABLE void duplicateProfile(const QString &id);
    Q_INVOKABLE void deleteProfile(const QString &id);
    Q_INVOKABLE void togglePin(const QString &id);
    Q_INVOKABLE void testAll();
    Q_INVOKABLE void testProfile(const QString &id);

    Q_INVOKABLE void runDiagnostics();
    Q_INVOKABLE void startService(bool restart);
    Q_INVOKABLE bool exportLogs(const QUrl &file);
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE void openUrl(const QString &url);
    Q_INVOKABLE void quit();

signals:
    void stateChanged();
    void trafficChanged();
    void currentChanged();
    void listChanged();
    void testingChanged();
    void helperChanged();
    void diagnosticsChanged();
    void settingsChanged();
    void importPreviewReady(const QVariantMap &preview);
    void confirmConflicts(const QString &text);
    void confirmInsecure(const QString &profileId);
    void toast(const QString &text);
    void showWindowRequested();

private:
    Profile *find(const QString &id);
    const Profile *find(const QString &id) const;
    void saveProfiles();
    void profilesChanged();
    void rebuildMasker();
    void doConnect();
    void onHelperState(const QJsonObject &s);
    void onSubscribed(const QJsonObject &r);
    void applySystemProxy();
    void restoreSystemProxy();
    void recoverSystemProxy(const QJsonObject &helperState);
    void fetchSubscription(const QString &url, bool allowHttp,
                           std::function<void(QByteArray body, SubscriptionInfo info, QString error)> done);
    void autoUpdateSubscriptions();
    void checkForUpdates();
    void testProfiles(const QList<Profile> &profiles);
    void resolveCountries();
    void delaysChanged();
    void setupTray();
    void updateTray();
    QString newId() const;

    Storage *m_storage;
    HelperClient *m_helper;
    SystemProxy m_sysproxy;
    QNetworkAccessManager *m_nam;
    ProfileModel *m_profiles;
    LogModel *m_appLogs;
    LogModel *m_coreLogs;
    Masker m_masker;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QTimer *m_proxyGuard;
    QTimer *m_hourly;

    QString m_state = QStringLiteral("disconnected");
    QString m_error, m_helperError, m_search;
    qint64 m_since = 0;
    int m_proxyPort = 0;
    bool m_proxyApplied = false;
    bool m_testing = false;
    bool m_reduceMotion = false;
    bool m_connectWhenReady = false;
    bool m_quitting = false;
    bool m_recovered = false;
    bool m_startHidden = false;
    QVariantMap m_traffic, m_diag;
    QHash<QString, int> m_delays;
    QSet<QString> m_updating;
    GeoIp m_geo;
    QHash<QString, QString> m_hostCountry;
    QSet<QString> m_resolving;
    QStringList m_testQueue;
    QList<Profile> m_pendingProfiles;
    QJsonObject m_pendingSubscription;
};

}
