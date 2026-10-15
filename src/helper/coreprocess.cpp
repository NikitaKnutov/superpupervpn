#include "coreprocess.h"

#include "platform.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>

#ifdef Q_OS_LINUX
#include <csignal>
#include <sys/prctl.h>
#endif

using namespace Qt::StringLiterals;

namespace rl {
bool verifyCore(QString *error)
{
    const QString path = platform::coreBinary();
    if (!platform::coreBinaryTrusted(path, error))
        return false;
    const QByteArray pinned = QByteArrayLiteral(SUPERPUPERVPN_CORE_SHA256).toLower();
    if (pinned.isEmpty()) {
#ifdef SUPERPUPERVPN_DEV
        qWarning("core SHA-256 is not pinned (SUPERPUPERVPN_DEV build), skipping the check");
        return true;
#else
        *error = u"сборка без закреплённого хеша ядра"_s;
        return false;
#endif
    }
    QFile f(path);
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!f.open(QIODevice::ReadOnly) || !h.addData(&f) || h.result().toHex() != pinned) {
        *error = u"хеш ядра не совпадает с закреплённым — файл подменён или повреждён"_s;
        return false;
    }
    return true;
}

QString coreVersion()
{
    QProcess p;
    p.start(platform::coreBinary(), {u"-v"_s});
    if (!p.waitForFinished(5000))
        return {};
    static const QRegularExpression re(u"v\\d+\\.\\d+\\.\\d+"_s);
    return re.match(QString::fromUtf8(p.readAll())).captured();
}

CoreProcess::CoreProcess(const QString &instance, QObject *parent)
    : QObject(parent), m_instance(instance), m_nam(new QNetworkAccessManager(this)), m_probe(new QTimer(this))
{
    m_probe->setInterval(250);
    connect(m_probe, &QTimer::timeout, this, &CoreProcess::probe);
}

CoreProcess::~CoreProcess()
{
    stop();
}

CoreRuntime CoreProcess::prepare()
{
    m_rt = {};
    QByteArray secret(32, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(secret.data()), secret.size() / 4);
    m_rt.secret = QString::fromLatin1(secret.toHex());
    const QString controller = platform::coreController(m_instance);
#ifdef Q_OS_WIN
    m_rt.controllerPipe = controller;
#else
    m_rt.controllerUnix = controller;
    QString error;
    if (!platform::ensurePrivateDir(QFileInfo(controller).path(), &error))
        qWarning("%s", qPrintable(error));
    QFile::remove(controller);
#endif
    masker.clear();
    masker.addSecret(m_rt.secret);
    return m_rt;
}

void CoreProcess::start(const QJsonObject &config)
{
    stop();
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    m_proc->setProcessEnvironment(platform::coreEnvironment());
#ifdef Q_OS_LINUX
    m_proc->setChildProcessModifier([] { ::prctl(PR_SET_PDEATHSIG, SIGTERM); });
#endif
    connect(m_proc, &QProcess::readyRead, this, &CoreProcess::readOutput);
    connect(m_proc, &QProcess::finished, this, [this, proc = m_proc] {
        m_probe->stop();
        if (m_traffic)
            m_traffic->abort();
        const bool wasReady = m_ready;
        m_ready = false;
        proc->deleteLater();
        if (m_proc == proc)
            m_proc = nullptr;
        if (!m_stopping)
            emit exited(!wasReady, m_lastError);
    });
    m_nam->clearConnectionCache();
    m_lastError.clear();
    m_buf.clear();
    m_ready = false;
    m_probes = 0;
    m_expected.clear();
    for (const QJsonValue p : config.value(u"proxies"_s).toArray())
        m_expected << p[u"name"_s].toString();
    m_proc->start(platform::coreBinary(), {u"-d"_s, platform::dataDir(), u"-f"_s, u"-"_s});
    if (!m_proc->waitForStarted(5000)) {
        m_lastError = u"не удалось запустить ядро: %1"_s.arg(m_proc->errorString());
        return;
    }
    m_proc->write(QJsonDocument(config).toJson(QJsonDocument::Compact));
    m_proc->closeWriteChannel();
    m_probe->start();
}

void CoreProcess::stop()
{
    QPointer<QProcess> p = m_proc;
    if (!p)
        return;
    m_stopping = true;
    m_probe->stop();
#ifdef Q_OS_WIN
    p->kill();
#else
    p->terminate();
    if (!p->waitForFinished(3000) && p)
        p->kill();
#endif
    if (p && p->state() != QProcess::NotRunning)
        p->waitForFinished(3000);
    m_stopping = false;
}

bool CoreProcess::isRunning() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

static QNetworkRequest apiRequest(const CoreRuntime &rt, const QString &pathAndQuery, int timeoutMs)
{
    QNetworkRequest req(QUrl(u"local+http://localhost"_s + pathAndQuery));
    req.setAttribute(QNetworkRequest::FullLocalServerNameAttribute,
                     rt.controllerUnix.isEmpty() ? rt.controllerPipe : rt.controllerUnix);
    req.setRawHeader("Authorization", "Bearer " + rt.secret.toLatin1());
    req.setTransferTimeout(timeoutMs);
    return req;
}

QNetworkReply *CoreProcess::get(const QString &pathAndQuery, int timeoutMs)
{
    return m_nam->get(apiRequest(m_rt, pathAndQuery, timeoutMs));
}

QNetworkReply *CoreProcess::del(const QString &path)
{
    return m_nam->deleteResource(apiRequest(m_rt, path, 3000));
}

void CoreProcess::probe()
{
    if (!isRunning() || m_ready)
        return;
    if (++m_probes > 60) {
        m_lastError = u"ядро не ответило за 15 с"_s;
        m_probe->stop();
        m_proc->kill();
        return;
    }
    m_probe->stop();
    QNetworkReply *r = get(u"/proxies"_s, 2000);
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        if (!isRunning())
            return;
        const QJsonObject proxies = QJsonDocument::fromJson(r->readAll()).object().value(u"proxies"_s).toObject();
        const bool loaded = r->error() == QNetworkReply::NoError
            && std::all_of(m_expected.cbegin(), m_expected.cend(), [&](const QString &n) { return proxies.contains(n); });
        if (loaded) {
            m_ready = true;
            emit ready();
        } else {
            m_probe->start();
        }
    });
}

void CoreProcess::readOutput()
{
    m_buf += m_proc->readAll();
    static const QRegularExpression re(uR"re(level=(\w+)\s+msg="((?:[^"\\]|\\.)*)")re"_s);
    qsizetype nl;
    while ((nl = m_buf.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_buf.left(nl)).trimmed();
        m_buf.remove(0, nl + 1);
        if (line.isEmpty())
            continue;
        QString level = u"info"_s, msg = line;
        if (const auto m = re.match(line); m.hasMatch()) {
            level = m.captured(1);
            msg = m.captured(2).replace(u"\\\""_s, u"\""_s).replace(u"\\\\"_s, u"\\"_s);
        }
        msg = masker.mask(msg);
        if (level == u"error" || level == u"fatal")
            m_lastError = msg;
        emit logLine(level, msg);
    }
}

void CoreProcess::watchTraffic()
{
    if (m_traffic || !m_ready)
        return;
    m_traffic = get(u"/traffic"_s, 0);
    connect(m_traffic, &QNetworkReply::readyRead, this, [this] {
        while (m_traffic->canReadLine()) {
            const QJsonObject o = QJsonDocument::fromJson(m_traffic->readLine()).object();
            if (!o.isEmpty())
                emit traffic(o);
        }
    });
    connect(m_traffic, &QNetworkReply::finished, this, [this] {
        m_traffic->deleteLater();
        m_traffic = nullptr;
    });
}

}
