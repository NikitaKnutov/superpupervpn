#include "platform.h"

#include "protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>

#include <climits>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

namespace rl::platform {
namespace {
constexpr int kRouteIndex = 8700;

QByteArray run(const QString &program, const QStringList &args, int *exitCode = nullptr)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(5000)) {
        p.kill();
        p.waitForFinished();
    }
    if (exitCode)
        *exitCode = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    return p.readAllStandardOutput();
}

QString runDir()
{
    return devOverride("SUPERPUPERVPN_RUN_DIR", u"/run/superpupervpn/core"_s);
}

}

QString dataDir() { return devOverride("SUPERPUPERVPN_DATA_DIR", u"/var/lib/superpupervpn/core"_s); }
QString coreBinary() { return devOverride("SUPERPUPERVPN_CORE_PATH", u"/usr/libexec/superpupervpn/mihomo"_s); }

QString coreController(const QString &instance)
{
    return runDir() + u'/' + instance + u".sock"_s;
}

QProcessEnvironment coreEnvironment()
{
    return QProcessEnvironment::systemEnvironment();
}

bool ensurePrivateDir(const QString &path, QString *error)
{
    if (!QDir().mkpath(path)) {
        *error = u"не удалось создать %1"_s.arg(path);
        return false;
    }
    struct stat st{};
    if (lstat(QFile::encodeName(path).constData(), &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != geteuid()) {
        *error = u"%1 принадлежит другому пользователю"_s.arg(path);
        return false;
    }
    if (chmod(QFile::encodeName(path).constData(), 0700) != 0) {
        *error = u"не удалось выставить права на %1"_s.arg(path);
        return false;
    }
    return true;
}

bool coreBinaryTrusted(const QString &path, QString *error)
{
    const QString real = QFileInfo(path).canonicalFilePath();
    if (real.isEmpty()) {
        *error = u"ядро не найдено: %1"_s.arg(path);
        return false;
    }
    struct stat st{};
    if (stat(QFile::encodeName(real).constData(), &st) != 0 || !S_ISREG(st.st_mode)) {
        *error = u"ядро не является файлом"_s;
        return false;
    }
    auto trustedOwner = [](uid_t uid) { return uid == 0 || uid == geteuid(); };
    if (!trustedOwner(st.st_uid) || (st.st_mode & 022)) {
        *error = u"файл ядра доступен на запись другим пользователям"_s;
        return false;
    }
    for (QDir dir = QFileInfo(real).dir();; dir.cdUp()) {
        if (stat(QFile::encodeName(dir.absolutePath()).constData(), &st) != 0) {
            *error = u"нет доступа к %1"_s.arg(dir.absolutePath());
            return false;
        }
        if (!trustedOwner(st.st_uid) || ((st.st_mode & 022) && !(st.st_mode & S_ISVTX))) {
            *error = u"папка %1 доступна на запись другим пользователям"_s.arg(dir.absolutePath());
            return false;
        }
        if (dir.isRoot())
            break;
    }
    return true;
}

void cleanupStaleNetwork()
{
    if (QFile::exists(u"/sys/class/net/superpupervpn"_s))
        run(u"ip"_s, {u"link"_s, u"del"_s, u"superpupervpn"_s});
    for (const QString &fam : {u"-4"_s, u"-6"_s}) {
        const QJsonArray rules = QJsonDocument::fromJson(run(u"ip"_s, {fam, u"-j"_s, u"rule"_s, u"show"_s})).array();
        for (const QJsonValue r : rules) {
            const int prio = r[u"priority"_s].toInt(-1);
            const QString table = r[u"table"_s].toVariant().toString();
            if ((prio >= kRouteIndex && prio < kRouteIndex + 100) || table == QString::number(kRouteIndex))
                run(u"ip"_s, {fam, u"rule"_s, u"del"_s, u"priority"_s, QString::number(prio)});
        }
        run(u"ip"_s, {fam, u"route"_s, u"flush"_s, u"table"_s, QString::number(kRouteIndex)});
    }
    int rc = -1;
    run(u"nft"_s, {u"list"_s, u"table"_s, u"inet"_s, u"mihomo"_s}, &rc);
    if (rc == 0)
        run(u"nft"_s, {u"delete"_s, u"table"_s, u"inet"_s, u"mihomo"_s});
}

QStringList conflictingAdapters()
{
    static const QRegularExpression vpnName(u"^(tun|tap|wg|tailscale|zt|nordlynx|proton|warp|utun)"_s);
    QStringList out;
    for (const QNetworkInterface &i : QNetworkInterface::allInterfaces())
        if (i.flags().testFlag(QNetworkInterface::IsUp) && i.name() != u"superpupervpn" && vpnName.match(i.name()).hasMatch())
            out << i.name();
    return out;
}

bool ipForwarding()
{
    QFile f(u"/proc/sys/net/ipv4/ip_forward"_s);
    return f.open(QIODevice::ReadOnly) && f.readAll().trimmed() == "1";
}

bool hotspotActive()
{
    return false;
}

bool tunPresent()
{
    return QFile::exists(u"/sys/class/net/superpupervpn"_s);
}

QString physicalInterface()
{
    QFile f(u"/proc/net/route"_s);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QString best;
    int bestMetric = INT_MAX;
    f.readLine();
    while (!f.atEnd()) {
        const QList<QByteArray> c = f.readLine().simplified().split(' ');
        if (c.size() < 8 || c[1] != "00000000" || c[7] != "00000000" || c[0] == "superpupervpn")
            continue;
        if (const int metric = c[6].toInt(); metric < bestMetric) {
            bestMetric = metric;
            best = QString::fromLatin1(c[0]);
        }
    }
    return best;
}

}

namespace rl {
HelperServer::HelperServer(QObject *parent) : QObject(parent), m_server(new QLocalServer(this))
{
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *s = m_server->nextPendingConnection())
            authorize(s);
    });
}

HelperServer::~HelperServer() = default;

bool HelperServer::listen(QString *error)
{
    const QString path = ipc::helperSocketName();
    QLocalServer::removeServer(path);
    m_server->setSocketOptions(QLocalServer::WorldAccessOption);
    if (!m_server->listen(path)) {
        *error = m_server->errorString();
        return false;
    }
    return true;
}

void HelperServer::authorize(QLocalSocket *socket)
{
    ucred cred{};
    socklen_t len = sizeof(cred);
    if (getsockopt(int(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
        socket->abort();
        socket->deleteLater();
        return;
    }
    QFile stat(u"/proc/%1/stat"_s.arg(cred.pid));
    QByteArray s = stat.open(QIODevice::ReadOnly) ? stat.readAll() : QByteArray();
    const QList<QByteArray> fields = s.mid(s.lastIndexOf(')') + 2).split(' ');
    const QByteArray startTime = fields.size() > 19 ? fields[19] : QByteArray("0");
    const QString peer = u"uid=%1 pid=%2"_s.arg(cred.uid).arg(cred.pid);
    qInfo("connection from %s", qPrintable(peer));

    auto *check = new QProcess(this);
    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    connect(check, &QProcess::finished, this, [=, this](int code, QProcess::ExitStatus status) {
        check->deleteLater();
        if (socket->state() != QLocalSocket::ConnectedState)
            return;
        if (status == QProcess::NormalExit && code == 0) {
            emit clientReady(socket, peer);
            return;
        }
        qWarning("polkit denied %s", qPrintable(peer));
        socket->write(ipc::encode({{u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, QJsonValue::Null},
                                   {u"error"_s, QJsonObject{{u"code"_s, -32003},
                                                           {u"message"_s, u"Нет прав на управление службой"_s}}}}));
        socket->disconnectFromServer();
    });
    check->start(u"pkcheck"_s, {u"--action-id"_s, devOverride("SUPERPUPERVPN_POLKIT_ACTION", u"org.superpupervpn.helper.control"_s),
                                u"--process"_s, u"%1,%2,%3"_s.arg(cred.pid).arg(QString::fromLatin1(startTime)).arg(cred.uid)});
}

}
