#pragma once

#include "configgen.h"
#include "masking.h"

#include <QJsonObject>
#include <QObject>
#include <QProcess>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace rl {
bool verifyCore(QString *error);
QString coreVersion();

class CoreProcess : public QObject {
    Q_OBJECT
public:
    CoreProcess(const QString &instance, QObject *parent = nullptr);
    ~CoreProcess() override;

    CoreRuntime prepare();
    void start(const QJsonObject &config);
    void stop();
    bool isRunning() const;
    bool isReady() const { return m_ready; }
    QNetworkReply *get(const QString &pathAndQuery, int timeoutMs);
    QNetworkReply *del(const QString &path);
    void watchTraffic();
    Masker masker;

signals:
    void ready();
    void exited(bool beforeReady, const QString &lastError);
    void logLine(const QString &level, const QString &message);
    void traffic(const QJsonObject &sample);

private:
    void readOutput();
    void probe();

    QString m_instance;
    QNetworkAccessManager *m_nam;
    QProcess *m_proc = nullptr;
    QTimer *m_probe;
    QNetworkReply *m_traffic = nullptr;
    CoreRuntime m_rt;
    QByteArray m_buf;
    QString m_lastError;
    QStringList m_expected;
    bool m_ready = false;
    bool m_stopping = false;
    int m_probes = 0;
};

}
