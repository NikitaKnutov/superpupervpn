#pragma once

#include <QObject>
#include <QProcessEnvironment>
#include <QStringList>

class QLocalSocket;
class QWinEventNotifier;
class QLocalServer;

namespace rl::platform {
QString dataDir();
QString coreBinary();
QString coreController(const QString &instance);
QProcessEnvironment coreEnvironment();

bool ensurePrivateDir(const QString &path, QString *error);
bool coreBinaryTrusted(const QString &path, QString *error);

void cleanupStaleNetwork();

QStringList conflictingAdapters();
QStringList stopOtherVpns();
bool ipForwarding();
bool hotspotActive();
bool tunPresent();
QString physicalInterface();

}

namespace rl {
class HelperServer : public QObject {
    Q_OBJECT
public:
    explicit HelperServer(QObject *parent = nullptr);
    ~HelperServer() override;
    bool listen(QString *error);

signals:
    void clientReady(QLocalSocket *socket, const QString &peer);

private:
#ifdef Q_OS_WIN
    void listenNext();
    void onPipeConnected();
    void *m_pipe = nullptr;
    void *m_event = nullptr;
    void *m_overlapped = nullptr;
    bool m_first = true;
    QWinEventNotifier *m_notifier = nullptr;
#else
    void authorize(QLocalSocket *socket);
    QLocalServer *m_server = nullptr;
#endif
};

}
