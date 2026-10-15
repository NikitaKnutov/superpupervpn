%global mihomo_version v1.19.32

Name:           superpupervpn
Version:        0.1.1
Release:        1%{?dist}
Summary:        SuperPuperVPN, a VLESS/REALITY desktop client
License:        LicenseRef-Proprietary AND GPL-3.0-only AND OFL-1.1
URL:            https://github.com/NikitaKnutov/superpupervpn
Source0:        %{name}-%{version}.tar.gz
Source1:        https://github.com/MetaCubeX/mihomo/releases/download/%{mihomo_version}/mihomo-linux-amd64-compatible-%{mihomo_version}.gz
ExclusiveArch:  x86_64

BuildRequires:  cmake >= 3.21
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= 6.8
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  openssl-devel
BuildRequires:  libsecret-devel
BuildRequires:  systemd-rpm-macros
Requires:       polkit
Requires:       iproute
Requires:       nftables
Requires:       qt6-qtsvg
Requires:       hicolor-icon-theme
%{?systemd_requires}

%description
SuperPuperVPN imports vless:// links and subscriptions and connects through a system proxy or a TUN
interface. A privileged helper service runs the pinned mihomo core; the GUI never touches it directly.

%prep
%autosetup -n %{name}-%{version}
. ./core.lock
echo "$MIHOMO_LINUX_SHA256  %{SOURCE1}" | sha256sum -c
gzip -dc %{SOURCE1} > mihomo

%build
%cmake -G Ninja -DSUPERPUPERVPN_CORE_SHA256=$(sha256sum mihomo | cut -d' ' -f1)
%cmake_build

%check
%ctest

%install
%cmake_install
install -Dm 0644 mihomo %{buildroot}%{_libexecdir}/superpupervpn/mihomo
install -Dm 0644 packaging/linux/superpupervpn-helper.service %{buildroot}%{_unitdir}/superpupervpn-helper.service
install -Dm 0644 packaging/linux/org.superpupervpn.helper.policy %{buildroot}%{_datadir}/polkit-1/actions/org.superpupervpn.helper.policy
install -Dm 0644 packaging/linux/superpupervpn.desktop %{buildroot}%{_datadir}/applications/superpupervpn.desktop
install -Dm 0644 assets/icons/superpupervpn.svg %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/superpupervpn.svg

%post
%systemd_post superpupervpn-helper.service
if [ $1 -eq 1 ]; then
    systemctl enable --now superpupervpn-helper.service >/dev/null 2>&1 || :
fi

%preun
%systemd_preun superpupervpn-helper.service

%postun
%systemd_postun_with_restart superpupervpn-helper.service

%files
%license licenses/*
%{_bindir}/superpupervpn
%dir %{_libexecdir}/superpupervpn
%{_libexecdir}/superpupervpn/superpupervpn-helper
%attr(0755,root,root) %{_libexecdir}/superpupervpn/mihomo
%{_unitdir}/superpupervpn-helper.service
%{_datadir}/polkit-1/actions/org.superpupervpn.helper.policy
%{_datadir}/applications/superpupervpn.desktop
%{_datadir}/icons/hicolor/scalable/apps/superpupervpn.svg

%changelog
* Thu Oct 15 2026 SuperPuperVPN <noreply@superpupervpn.invalid> - 0.1.1-1
- Install on Fedora 42 and newer: no dependency on private Qt API

* Thu Oct 15 2026 SuperPuperVPN <noreply@superpupervpn.invalid> - 0.1.0-5
- Theme: light or dark only

* Thu Oct 15 2026 SuperPuperVPN <noreply@superpupervpn.invalid> - 0.1.0-4
- Country flags by server IP, ping on tap, readable dark drop-downs

* Thu Oct 15 2026 SuperPuperVPN <noreply@superpupervpn.invalid> - 0.1.0-3
- New look: dark theme with a lime accent, redesigned screens

* Thu Oct 15 2026 SuperPuperVPN <noreply@superpupervpn.invalid> - 0.1.0-2
- REALITY: send the X25519MLKEM768 key share like current Xray clients

* Thu Oct 15 2026 SuperPuperVPN <noreply@superpupervpn.invalid> - 0.1.0-1
- First build
