#pragma once

#include <QJsonObject>
#include <QString>

namespace rl {
class SystemProxy {
public:
    explicit SystemProxy(const QString &dir);
    bool apply(int port, QString *error);
    bool restore(QString *error);
    void forget();
    int markerPort() const;
    static bool pointsTo(int port);

private:
    QString m_dir;
};

namespace sysproxy {
QJsonObject capture();
bool set(int port, QString *error);
bool restore(const QJsonObject &snapshot, QString *error);
bool pointsTo(int port);
}

}
