#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <functional>

class QLocalSocket;
class QTimer;

namespace rl {
class HelperClient : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(const QJsonObject &result, const QJsonObject &error)>;

    explicit HelperClient(QObject *parent = nullptr);
    void start();
    void call(const QString &method, const QJsonObject &params = {}, Callback cb = {});
    bool isReady() const { return m_ready; }
    QJsonObject hello() const { return m_hello; }
    void setLanguage(const QString &language);

signals:
    void readyChanged(bool ready);
    void subscribed(const QJsonObject &result);
    void notification(const QString &method, const QJsonObject &params);
    void fatal(const QString &message);

private:
    void onConnected();
    void onReadyRead();
    void setReady(bool ready);

    QLocalSocket *m_socket;
    QTimer *m_retry;
    QByteArray m_buf;
    QHash<int, Callback> m_pending;
    int m_nextId = 1;
    bool m_ready = false;
    QJsonObject m_hello;
    QString m_language;
};

}
