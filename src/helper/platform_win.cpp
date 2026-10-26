#include "platform.h"

#include "protocol.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QSettings>
#include <QWinEventNotifier>

#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <aclapi.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <sddl.h>
#include <tlhelp32.h>

#include <memory>

using namespace Qt::StringLiterals;

namespace rl::platform {
namespace {
constexpr wchar_t kPrivateDirSddl[] = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)";

bool trustedSid(PSID sid)
{
    if (IsWellKnownSid(sid, WinLocalSystemSid) || IsWellKnownSid(sid, WinBuiltinAdministratorsSid))
        return true;
    PSID ti = nullptr;
    if (!ConvertStringSidToSidW(L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464", &ti))
        return false;
    const bool same = EqualSid(sid, ti);
    LocalFree(ti);
    return same;
}

bool onlyAdminsCan(const QString &path, ACCESS_MASK rights, QString *who)
{
    PSID owner = nullptr;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (GetNamedSecurityInfoW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()), SE_FILE_OBJECT,
                              OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, nullptr, &dacl,
                              nullptr, &sd) != ERROR_SUCCESS)
        return false;
    std::unique_ptr<void, decltype(&LocalFree)> guard(sd, &LocalFree);
    if (!trustedSid(owner)) {
        *who = QCoreApplication::translate("Platform", "owner");
        return false;
    }
    if (!dacl) {
        *who = QCoreApplication::translate("Platform", "everyone");
        return false;
    }
    for (DWORD i = 0; i < dacl->AceCount; ++i) {
        ACE_HEADER *h = nullptr;
        if (!GetAce(dacl, i, reinterpret_cast<void **>(&h)) || h->AceType != ACCESS_ALLOWED_ACE_TYPE
            || (h->AceFlags & INHERIT_ONLY_ACE))
            continue;
        auto *ace = reinterpret_cast<ACCESS_ALLOWED_ACE *>(h);
        if ((ace->Mask & (rights | GENERIC_ALL | GENERIC_WRITE)) && !trustedSid(&ace->SidStart)) {
            *who = QCoreApplication::translate("Platform", "regular users");
            return false;
        }
    }
    return true;
}

QString wide(const wchar_t *s)
{
    return QString::fromWCharArray(s);
}

template<typename Fn>
void forEachAdapter(Fn fn)
{
    ULONG size = 16 * 1024;
    std::unique_ptr<char[]> buf;
    ULONG rc = ERROR_BUFFER_OVERFLOW;
    for (int i = 0; i < 3 && rc == ERROR_BUFFER_OVERFLOW; ++i) {
        buf.reset(new char[size]);
        rc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                  nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buf.get()), &size);
    }
    if (rc != NO_ERROR)
        return;
    for (auto *a = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buf.get()); a; a = a->Next)
        fn(*a);
}

}

QString dataDir()
{
    return qEnvironmentVariable("ProgramData", u"C:/ProgramData"_s) + u"/SuperPuperVPN/core"_s;
}

QString coreBinary() { return QCoreApplication::applicationDirPath() + u"/core/mihomo.exe"_s; }

QString coreController(const QString &instance)
{
    return u"\\\\.\\pipe\\superpupervpn-"_s + instance;
}

QProcessEnvironment coreEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(u"LISTEN_NAMEDPIPE_SDDL"_s, u"D:PAI(A;OICI;GWGR;;;SY)"_s);
    return env;
}

bool ensurePrivateDir(const QString &path, QString *error)
{
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(kPrivateDirSddl, SDDL_REVISION_1, &sd, nullptr)) {
        *error = u"SDDL"_s;
        return false;
    }
    std::unique_ptr<void, decltype(&LocalFree)> guard(sd, &LocalFree);
    SECURITY_ATTRIBUTES sa{sizeof(sa), sd, FALSE};
    const QStringList parts = {QFileInfo(path).path(), path};
    for (const QString &dir : parts) {
        const auto native = reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(dir).utf16());
        if (!CreateDirectoryW(native, &sa) && GetLastError() != ERROR_ALREADY_EXISTS) {
            *error = QCoreApplication::translate("Platform", "failed to create %1").arg(dir);
            return false;
        }
        QString who;
        if (!onlyAdminsCan(dir, FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE | WRITE_DAC | WRITE_OWNER, &who)) {
            *error = QCoreApplication::translate("Platform", "%1: write access allowed (%2)").arg(dir, who);
            return false;
        }
    }
    return true;
}

bool coreBinaryTrusted(const QString &path, QString *error)
{
    const QFileInfo fi(path);
    if (!fi.isFile()) {
        *error = QCoreApplication::translate("Platform", "core not found: %1").arg(path);
        return false;
    }
    QString who;
    if (!onlyAdminsCan(fi.absoluteFilePath(), FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE | WRITE_DAC | WRITE_OWNER, &who)) {
        *error = QCoreApplication::translate("Platform", "the core file is writable (%1)").arg(who);
        return false;
    }
    QDir dir = fi.dir();
    ACCESS_MASK rights = FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_DELETE_CHILD | DELETE | WRITE_DAC | WRITE_OWNER;
    for (;;) {
        if (!onlyAdminsCan(dir.absolutePath(), rights, &who)) {
            *error = QCoreApplication::translate("Platform", "folder %1 is writable (%2)").arg(QDir::toNativeSeparators(dir.absolutePath()), who);
            return false;
        }
        if (!dir.cdUp())
            break;
        rights = FILE_DELETE_CHILD | DELETE | WRITE_DAC | WRITE_OWNER;
    }
    return true;
}

void cleanupStaleNetwork()
{
}

QStringList conflictingAdapters()
{
    QStringList out;
    forEachAdapter([&](const IP_ADAPTER_ADDRESSES &a) {
        const QString desc = wide(a.Description), name = wide(a.FriendlyName);
        if (a.OperStatus != IfOperStatusUp || name == u"superpupervpn")
            return;
        for (const char16_t *k : {u"Wintun", u"WireGuard", u"TAP-", u"OpenVPN", u"ZeroTier", u"Tailscale"})
            if (desc.contains(QString::fromUtf16(k), Qt::CaseInsensitive)) {
                out << name;
                return;
            }
    });
    return out;
}

QStringList stopOtherVpns()
{
    static const QStringList names = {
        u"v2rayn.exe"_s, u"xray.exe"_s, u"v2ray.exe"_s, u"sing-box.exe"_s, u"mihomo.exe"_s, u"clash.exe"_s,
        u"clash-meta.exe"_s, u"clash-verge.exe"_s, u"verge-mihomo.exe"_s, u"verge-mihomo-alpha.exe"_s,
        u"flclash.exe"_s, u"flclashcore.exe"_s, u"hiddify.exe"_s, u"hiddifycli.exe"_s, u"nekoray.exe"_s,
        u"nekobox_core.exe"_s, u"throne.exe"_s, u"happ.exe"_s, u"v2raytun.exe"_s, u"amneziavpn.exe"_s,
        u"amneziavpn-service.exe"_s, u"wireguard.exe"_s, u"openvpn.exe"_s, u"openvpn-gui.exe"_s,
        u"outline.exe"_s, u"tun2socks.exe"_s};
    const QString ownCore = QFileInfo(coreBinary()).absoluteFilePath();
    QStringList stopped;
    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return stopped;
    PROCESSENTRY32W e{};
    e.dwSize = sizeof(e);
    for (BOOL ok = Process32FirstW(snap, &e); ok; ok = Process32NextW(snap, &e)) {
        const QString exe = QString::fromWCharArray(e.szExeFile);
        if (e.th32ProcessID == GetCurrentProcessId() || !names.contains(exe, Qt::CaseInsensitive))
            continue;
        const HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
        if (!h)
            continue;
        wchar_t path[MAX_PATH];
        DWORD n = MAX_PATH;
        const bool own = QueryFullProcessImageNameW(h, 0, path, &n)
            && QFileInfo(QString::fromWCharArray(path, int(n))).absoluteFilePath().compare(ownCore, Qt::CaseInsensitive) == 0;
        if (!own && TerminateProcess(h, 1))
            stopped << exe;
        CloseHandle(h);
    }
    CloseHandle(snap);
    return stopped;
}

bool ipForwarding()
{
    QSettings s(u"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"_s, QSettings::NativeFormat);
    return s.value(u"IPEnableRouter"_s).toInt() == 1;
}

bool hotspotActive()
{
    bool on = false;
    forEachAdapter([&](const IP_ADAPTER_ADDRESSES &a) {
        if (a.OperStatus == IfOperStatusUp && wide(a.Description).contains(u"Wi-Fi Direct Virtual Adapter"_s))
            on = true;
    });
    return on;
}

bool tunPresent()
{
    bool found = false;
    forEachAdapter([&](const IP_ADAPTER_ADDRESSES &a) {
        if (wide(a.FriendlyName) == u"superpupervpn")
            found = true;
    });
    return found;
}

QString physicalInterface()
{
    PMIB_IPFORWARD_TABLE2 table = nullptr;
    if (GetIpForwardTable2(AF_INET, &table) != NO_ERROR)
        return {};
    const QStringList other = conflictingAdapters();
    QString best;
    ULONG bestMetric = ULONG_MAX;
    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IPFORWARD_ROW2 &r = table->Table[i];
        if (r.DestinationPrefix.PrefixLength != 0)
            continue;
        wchar_t alias[IF_MAX_STRING_SIZE + 1] = {};
        if (ConvertInterfaceLuidToAlias(&r.InterfaceLuid, alias, IF_MAX_STRING_SIZE) != NO_ERROR
            || wide(alias) == u"superpupervpn" || other.contains(wide(alias)))
            continue;
        MIB_IPINTERFACE_ROW iface{};
        iface.Family = AF_INET;
        iface.InterfaceLuid = r.InterfaceLuid;
        const ULONG metric = r.Metric + (GetIpInterfaceEntry(&iface) == NO_ERROR ? iface.Metric : 0);
        if (metric < bestMetric) {
            bestMetric = metric;
            best = wide(alias);
        }
    }
    FreeMibTable(table);
    return best;
}

}

namespace rl {
HelperServer::HelperServer(QObject *parent)
    : QObject(parent), m_event(CreateEventW(nullptr, TRUE, FALSE, nullptr)), m_overlapped(new OVERLAPPED{})
{
    m_notifier = new QWinEventNotifier(m_event, this);
    m_notifier->setEnabled(false);
    connect(m_notifier, &QWinEventNotifier::activated, this, &HelperServer::onPipeConnected);
}

HelperServer::~HelperServer()
{
    if (m_pipe) {
        CancelIo(m_pipe);
        CloseHandle(m_pipe);
    }
    CloseHandle(m_event);
    delete static_cast<OVERLAPPED *>(m_overlapped);
}

bool HelperServer::listen(QString *error)
{
    listenNext();
    if (!m_pipe)
        *error = QCoreApplication::translate("Platform", "failed to create the pipe \\\\.\\pipe\\superpupervpn-helper");
    return m_pipe != nullptr;
}

void HelperServer::listenNext()
{
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;GA;;;SY)(A;;GRGW;;;IU)", SDDL_REVISION_1, &sd, nullptr))
        return;
    SECURITY_ATTRIBUTES sa{sizeof(sa), sd, FALSE};
    const DWORD openMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (m_first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0);
    HANDLE h = CreateNamedPipeW(L"\\\\.\\pipe\\superpupervpn-helper", openMode,
                                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                PIPE_UNLIMITED_INSTANCES, 64 * 1024, 64 * 1024, 0, &sa);
    LocalFree(sd);
    if (h == INVALID_HANDLE_VALUE) {
        qWarning("CreateNamedPipe failed: %lu", GetLastError());
        return;
    }
    m_first = false;
    m_pipe = h;
    auto *ov = static_cast<OVERLAPPED *>(m_overlapped);
    *ov = {};
    ov->hEvent = m_event;
    ResetEvent(m_event);
    if (!ConnectNamedPipe(h, ov)) {
        switch (GetLastError()) {
        case ERROR_IO_PENDING:
            m_notifier->setEnabled(true);
            return;
        case ERROR_PIPE_CONNECTED:
            break;
        default:
            qWarning("ConnectNamedPipe failed: %lu", GetLastError());
            CloseHandle(h);
            m_pipe = nullptr;
            return;
        }
    }
    onPipeConnected();
}

void HelperServer::onPipeConnected()
{
    m_notifier->setEnabled(false);
    ResetEvent(m_event);
    HANDLE h = m_pipe;
    m_pipe = nullptr;
    ULONG pid = 0;
    GetNamedPipeClientProcessId(h, &pid);
    const QString peer = u"pid=%1"_s.arg(pid);
    qInfo("connection from %s", qPrintable(peer));
    auto *s = new QLocalSocket(this);
    if (s->setSocketDescriptor(qintptr(h))) {
        connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
        emit clientReady(s, peer);
    } else {
        CloseHandle(h);
        delete s;
    }
    listenNext();
}

}
