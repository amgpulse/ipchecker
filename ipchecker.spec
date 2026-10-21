Name:           ipchecker
Version:        1.3.0
Release:        1%{?dist}
Summary:        IP Inspector Pro - Modern Qt6 network identity inspector
License:        MIT
URL:            https://github.com/amgpulse/ipchecker
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  qt6-qtbase-devel
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  desktop-file-utils

# Binary is not stripped here; debuginfo is not useful for a GUI app package.
%global debug_package %{nil}

%description
A fast, modern Qt6 desktop application that reveals your network identity:
your local and public IP (IPv4 and IPv6), geolocation, MAC addresses,
hostnames, and every network interface.

%prep
%autosetup

%build
/usr/bin/qmake-qt6 ipchecker.pro || /usr/lib64/qt6/bin/qmake ipchecker.pro || qmake ipchecker.pro
%make_build

%install
install -D -m 0755 ipchecker %{buildroot}%{_bindir}/ipchecker
install -D -m 0644 packaging/deb/usr/share/applications/ipchecker.desktop \
    %{buildroot}%{_datadir}/applications/ipchecker.desktop
install -D -m 0644 packaging/deb/usr/share/icons/hicolor/256x256/apps/ipchecker.png \
    %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/ipchecker.png

%files
%{_bindir}/ipchecker
%{_datadir}/applications/ipchecker.desktop
%{_datadir}/icons/hicolor/256x256/apps/ipchecker.png

%changelog
* Thu Oct 22 2026 AMG <amgpulse@users.noreply.github.com> - 1.3.0-1
- Extract testable network logic into netutils and add a Qt Test suite (CI-gated)
- Persist the geolocation cache, cap stored history/geo entries
- Show last-known public IP with a stale marker when a refresh fails
- Move light/dark themes into QSS resources

* Thu Oct 15 2026 AMG <amgpulse@users.noreply.github.com> - 1.2.0-1
- Fix network timeouts, JSON parsing and IPv4/IPv6 display race
- Add IP history viewer, configurable refresh, single-instance support
- Package desktop entry and application icon

* Fri Sep 18 2026 IP Inspector Team <support@ipinspector.local> - 1.1.0-1
- Add IPv6 support and update packaging
