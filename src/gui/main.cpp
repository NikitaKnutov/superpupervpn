#include "app.h"
#include "storage.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFontDatabase>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace {
rl::App *g_app = nullptr;
QtMessageHandler g_defaultHandler = nullptr;

void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    g_defaultHandler(type, ctx, msg);
    static thread_local bool inside = false;
    if (!g_app || inside || QThread::currentThread() != qApp->thread())
        return;
    inside = true;
    const QString level = type == QtDebugMsg ? u"debug"_s : type == QtInfoMsg ? u"info"_s : type == QtWarningMsg ? u"warning"_s : u"error"_s;
    g_app->appLog(level, msg);
    inside = false;
}

}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(u"SuperPuperVPN"_s);
    app.setDesktopFileName(u"superpupervpn"_s);
    app.setApplicationVersion(QStringLiteral(SUPERPUPERVPN_VERSION));
    app.setWindowIcon(QIcon(u":/icons/superpupervpn.svg"_s));
    app.setQuitOnLastWindowClosed(false);

    const QString instance = u"superpupervpn-gui-"_s
        + QString::fromLatin1(QCryptographicHash::hash(QDir::homePath().toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
    const QStringList args = app.arguments().mid(1);
    {
        QLocalSocket probe;
        probe.connectToServer(instance);
        if (probe.waitForConnected(300)) {
            probe.write(args.join(u'\n').toUtf8() + '\n');
            probe.waitForBytesWritten(1000);
            return 0;
        }
    }
    QLocalServer single;
    single.setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(instance);
    single.listen(instance);

    rl::Storage storage;
    QString error;
    if (!storage.load(&error)) {
        QMessageBox::critical(nullptr, u"SuperPuperVPN"_s, error);
        return 1;
    }

    QFontDatabase::addApplicationFont(u":/fonts/InterVariable.ttf"_s);
    QFont font(u"Inter Variable"_s);
    font.setPixelSize(14);
    app.setFont(font);
    QQuickStyle::setStyle(u"Basic"_s);

    const bool autostarted = args.contains(u"--autostart"_s);
    rl::App controller(&storage, autostarted);
    g_app = &controller;
    g_defaultHandler = qInstallMessageHandler(messageHandler);

    QObject::connect(&single, &QLocalServer::newConnection, &controller, [&] {
        while (QLocalSocket *s = single.nextPendingConnection()) {
            QObject::connect(s, &QLocalSocket::readyRead, s, [s, &controller] {
                if (!s->canReadLine())
                    return;
                const QStringList forwarded = QString::fromUtf8(s->readAll()).split(u'\n', Qt::SkipEmptyParts);
                emit controller.showWindowRequested();
                controller.handleArgs(forwarded);
                s->disconnectFromServer();
            });
            QObject::connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
        }
    });

    QQmlApplicationEngine engine;
    engine.loadFromModule("SuperPuperVPN", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    controller.start(autostarted);
    controller.handleArgs(args);
#ifdef SUPERPUPERVPN_DEV
    if (const QString grab = qEnvironmentVariable("SUPERPUPERVPN_GRAB"); !grab.isEmpty())
    {
        auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
        if (const auto width = qEnvironmentVariableIntegerValue("SUPERPUPERVPN_GRAB_WIDTH"))
            w->setWidth(*width);
        if (const auto page = qEnvironmentVariableIntegerValue("SUPERPUPERVPN_GRAB_PAGE"))
            w->setProperty("page", *page);
        QTimer::singleShot(qEnvironmentVariableIntegerValue("SUPERPUPERVPN_GRAB_DELAY").value_or(1500), [w, grab] {
            w->grabWindow().save(grab);
            QCoreApplication::exit(0);
        });
    }
#endif
    const int rc = app.exec();
    g_app = nullptr;
    return rc;
}
