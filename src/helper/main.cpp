#include "helper.h"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <QSocketNotifier>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
#ifndef Q_OS_WIN
int g_sigFd[2];

void onSignal(int)
{
    const char c = 1;
    [[maybe_unused]] auto n = ::write(g_sigFd[0], &c, 1);
}

void quitOnSignals(QCoreApplication *app)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, g_sigFd) != 0)
        return;
    auto *sn = new QSocketNotifier(g_sigFd[1], QSocketNotifier::Read, app);
    QObject::connect(sn, &QSocketNotifier::activated, app, [] {
        char c;
        [[maybe_unused]] auto n = ::read(g_sigFd[1], &c, 1);
        QCoreApplication::quit();
    });
    std::signal(SIGTERM, onSignal);
    std::signal(SIGINT, onSignal);
}
#endif

int runHelper(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("superpupervpn-helper"));
#ifndef Q_OS_WIN
    quitOnSignals(&app);
#endif
    rl::Helper helper;
    QString error;
    if (!helper.start(&error)) {
        qCritical("superpupervpn-helper: %s", qPrintable(error));
        return 1;
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &helper, &rl::Helper::shutdown);
    return app.exec();
}

}

#ifdef Q_OS_WIN

namespace {
SERVICE_STATUS_HANDLE g_status = nullptr;

void report(DWORD state)
{
    SERVICE_STATUS st{};
    st.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    st.dwCurrentState = state;
    st.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    st.dwWaitHint = 5000;
    SetServiceStatus(g_status, &st);
}

DWORD WINAPI control(DWORD code, DWORD, void *, void *)
{
    if (code == SERVICE_CONTROL_STOP || code == SERVICE_CONTROL_SHUTDOWN) {
        report(SERVICE_STOP_PENDING);
        QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
    }
    return NO_ERROR;
}

void WINAPI serviceMain(DWORD, LPWSTR *)
{
    g_status = RegisterServiceCtrlHandlerExW(L"superpupervpn-helper", control, nullptr);
    report(SERVICE_RUNNING);
    char name[] = "superpupervpn-helper";
    char *argv[] = {name, nullptr};
    runHelper(1, argv);
    report(SERVICE_STOPPED);
}

}

int main(int argc, char **argv)
{
    SERVICE_TABLE_ENTRYW table[] = {{const_cast<LPWSTR>(L"superpupervpn-helper"), serviceMain}, {nullptr, nullptr}};
    if (StartServiceCtrlDispatcherW(table))
        return 0;
    if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
        return runHelper(argc, argv);
    return 1;
}

#else

int main(int argc, char **argv)
{
    return runHelper(argc, argv);
}

#endif
