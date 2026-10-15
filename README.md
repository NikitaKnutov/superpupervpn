<p align="center">
  <img src="assets/icons/superpupervpn.svg" width="96" alt="SuperPuperVPN logo">
</p>

<h1 align="center">SuperPuperVPN</h1>

<p align="center">
  <b>One button. Your own VLESS + REALITY servers. Windows and Linux.</b><br>
  A clean desktop client that turns a <code>vless://</code> link into a working connection in seconds.
</p>

<p align="center">
  <a href="https://github.com/NikitaKnutov/superpupervpn/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/NikitaKnutov/superpupervpn?color=C6F046&labelColor=0B0D0F"></a>
  <a href="https://github.com/NikitaKnutov/superpupervpn/actions/workflows/build.yml"><img alt="Build" src="https://img.shields.io/github/actions/workflow/status/NikitaKnutov/superpupervpn/build.yml?labelColor=0B0D0F"></a>
  <img alt="Platforms" src="https://img.shields.io/badge/platforms-Windows%2010%2F11%20%7C%20Fedora-C6F046?labelColor=0B0D0F">
</p>

<p align="center">
  <img src="docs/screenshots/home-dark.png" width="260" alt="Home screen, dark theme">
  &nbsp;
  <img src="docs/screenshots/servers-dark.png" width="260" alt="Server list with country flags">
  &nbsp;
  <img src="docs/screenshots/home-light.png" width="260" alt="Home screen, light theme">
</p>

---

## Why

Most VLESS clients are built for people who already know what `spx`, `sid` and `fp` mean, and many of them
open an unauthenticated local SOCKS port that any program on your computer can use to leak your real IP.

SuperPuperVPN is the opposite:

- **Paste a link, press the button.** Nothing else to configure.
- **Secure by default.** No open ports in TUN mode, an authenticated privileged helper, encrypted profiles.
- **Built for your own servers** (3x-ui, Marzban, Remnawave or a hand-written Xray config) and for the people
  you share them with.

## Features

- **Import** from the clipboard (`Ctrl+V`): one link, many links or a base64 blob; or by subscription URL with
  traffic and expiry shown from the `subscription-userinfo` header. `vless://` and `superpupervpn://` deep links.
- **Two modes**
  - **System proxy**: browsers and apps that respect the OS proxy go through the server.
  - **TUN**: all traffic of the computer goes through the server, including games and the terminal.
- **All traffic through the tunnel**, DNS included (DoH through the connection, fake-IP in TUN), IPv6 never leaks.
- **Server list** with search, sorting, pinning, real latency (`generate_204` through the server itself),
  ping on tap and **country flags detected from the server IP** with an offline database.
- **Honest status**: "Connected" only after a real request went through the server.
- **Auto-recovery**: the core is restarted if it crashes; the system proxy is restored after a crash or a power loss.
- **Dark and light themes**, tray icon, autostart, connect on start, a log viewer with masked export.
- **No telemetry.** Ever.

## Install

Grab the latest build from **[Releases](https://github.com/NikitaKnutov/superpupervpn/releases/latest)**.

### Windows 10 / 11 (x64)

1. Download `SuperPuperVPN-<version>-windows-x64.msi`.
2. Run it. Windows asks for administrator rights once: the installer sets up the *SuperPuperVPN Helper*
   service that TUN mode needs.
3. Start **SuperPuperVPN** from the Start menu.

The builds are not code-signed yet, so SmartScreen may warn you: choose *More info → Run anyway*.
A portable `.zip` is attached as well; in that case run `superpupervpn-helper.exe` as administrator yourself.

> Windows support is new. If something does not work, please open an issue with the log export
> (*«Логи» → «Экспорт»*).

### Fedora Linux (x86_64)

```bash
sudo dnf install ./superpupervpn-*.x86_64.rpm
```

The package installs and starts the `superpupervpn-helper` system service. Launch **SuperPuperVPN** from the
app grid or run `superpupervpn`.

On GNOME the tray icon needs the
[AppIndicator extension](https://extensions.gnome.org/extension/615/appindicator-support/)
(`gnome-shell-extension-appindicator`); without it the app works fine and asks what to do when you close the window.

Uninstall: `sudo dnf remove superpupervpn`.

Other distributions: build from source (see below).

## Quick start

The interface is in Russian; button names are given below as **«Русский»** (English).

1. Copy your `vless://...` link (or a subscription URL).
2. Open SuperPuperVPN and press **`Ctrl+V`** (or the **+** button) → **«Добавить»** (Add).
3. Pick **«Системный прокси»** (System proxy) or **TUN** and press the big button.

Tap a server in **«Серверы»** (Servers) to select it and measure its latency; **«Проверить все»** (Check all) tests every server.

## How it works

```mermaid
flowchart LR
    GUI["SuperPuperVPN<br/>Qt Quick GUI<br/>(your user)"] -- "JSON-RPC over<br/>unix socket / named pipe<br/>(polkit / ACL)" --> Helper["superpupervpn-helper<br/>(root / SYSTEM)"]
    Helper -- "config via stdin<br/>REST over a private socket" --> Core["mihomo core<br/>(pinned version)"]
    Core -- "VLESS + REALITY" --> Server[("Your server")]
```

- The **GUI** never touches the core. It stores profiles in `profiles.enc` (AES-256-GCM, key in the OS keyring:
  libsecret on Linux, DPAPI on Windows) and sends a structured profile to the helper.
- The **helper** re-validates every field, generates the core config from a whitelist, starts the pinned
  [mihomo](https://github.com/MetaCubeX/mihomo) binary after checking its SHA-256 and passes the config through
  stdin, so it never touches the disk.
- The core API is reachable only through a socket in a `0700` directory (Linux) or a SYSTEM-only named pipe
  (Windows). No TCP ports. In TUN mode nothing listens on localhost at all; in proxy mode exactly one TCP port on
  `127.0.0.1` is open.
- Access to the helper: on Linux every connection is checked with polkit (active local session only), on Windows
  the pipe is limited to SYSTEM and interactive users and refuses network clients.

### Supported links

`vless://` with `security=reality|tls|none`, `flow=xtls-rprx-vision`, transports `tcp`, `ws`, `httpupgrade`,
`grpc`, `xhttp`, `http` (h2), ML-KEM `encryption`, `alpn`, `ech`, certificate pinning (`pcs`).
Links with parameters the core cannot honour (`pqv`, `fm`, `spx` other than `/`, `type=kcp`, `vcn`) are refused
with a clear message instead of connecting half-way. Other schemes (vmess, trojan, ss, ...) are recognised and
reported as unsupported.

## Troubleshooting

| Symptom | What to do |
|---|---|
| «Служба не запущена» (service is not running) | *«Настройки» → «Служба» → «Запустить»*, or `sudo systemctl start superpupervpn-helper` |
| «сервер не отвечает» (server does not respond) | The server refused the handshake or is unreachable. Check the link with another client; get a fresh link from your panel. |
| No tray icon on GNOME | Install and enable the AppIndicator extension. |
| Proxy mode does not work in a terminal | CLI tools ignore the desktop proxy setting; use TUN mode. |

## Build from source

Requirements: CMake ≥ 3.21, a C++20 compiler, Qt ≥ 6.8 (Base, Declarative, Network), OpenSSL 3,
libsecret (Linux).

```bash
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel openssl-devel libsecret-devel
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build
```

Release builds pass `-DSUPERPUPERVPN_CORE_SHA256=<sha256 of the mihomo binary>`; without it the helper refuses
to start the core. The pinned core and its hashes live in [`core.lock`](core.lock).

Packages:

- **RPM**: put the source tarball and the core archive from `core.lock` into `~/rpmbuild/SOURCES`
  (`spectool -g packaging/linux/superpupervpn.spec`) and run `rpmbuild -bb packaging/linux/superpupervpn.spec`.
- **MSI / ZIP**: built by the `windows` job in [`.github/workflows/build.yml`](.github/workflows/build.yml).

### Developer mode

`-DSUPERPUPERVPN_DEV=ON` lets both binaries run unprivileged from a scratch directory (never use it for packages):

- helper: `SUPERPUPERVPN_SOCKET`, `SUPERPUPERVPN_RUN_DIR`, `SUPERPUPERVPN_DATA_DIR`, `SUPERPUPERVPN_CORE_PATH`,
  `SUPERPUPERVPN_POLKIT_ACTION`; an unpinned core is allowed;
- GUI: `SUPERPUPERVPN_SOCKET`; `SUPERPUPERVPN_GRAB=out.png QT_QPA_PLATFORM=offscreen` renders one frame and exits.

`SUPERPUPERVPN_TEST_MIHOMO=/path/to/mihomo ctest` additionally validates every generated config with `mihomo -t`.

### Project layout

```
src/core     link parser, validation, subscriptions, core config generator, masking, GeoIP (no I/O)
src/helper   privileged service: JSON-RPC server, core process manager, platform layer
src/gui      Qt Quick app: storage, system proxy, tray, QML screens
tests        unit tests (link vectors, config security checks, theme contrast, translations)
packaging    systemd unit, polkit action, .desktop, RPM spec, WiX installer
tools        make-geo.py: rebuilds assets/geo/country.bin from the DB-IP Lite CSV
licenses     third-party license texts shipped with the packages
```

### Releasing

Bump the version in `CMakeLists.txt` and `packaging/linux/superpupervpn.spec`, commit, then push a tag:

```bash
git tag v0.1.1 && git push origin v0.1.1
```

GitHub Actions builds the RPM, MSI and ZIP and publishes them as a release.

## License

SuperPuperVPN itself: © Nikita Knutov, all rights reserved.

It ships third-party components under their own licenses, see [`licenses/`](licenses):
[mihomo](https://github.com/MetaCubeX/mihomo) (GPL-3.0, separate process), Qt (LGPL-3.0), OpenSSL (Apache-2.0),
libsecret (LGPL-2.1+), Inter font (OFL-1.1) and
[IP Geolocation by DB-IP](https://db-ip.com) (CC BY 4.0).
