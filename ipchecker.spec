Name:           ipchecker
Version:        1.1.0
Release:        1%{?dist}
Summary:        IP Inspector Pro - Modern Qt6 network identity inspector

License:        MIT
URL:            https://github.com/amg/ipchecker

%description
A fast, modern Qt6 desktop application that reveals your network identity — your local and public IP (IPv4 & IPv6), geolocation, MAC addresses, hostnames, and every network interface.

%prep
# No setup needed for local build

%build
/usr/bin/qmake-qt6 ipchecker.pro || /usr/lib64/qt6/bin/qmake ipchecker.pro || qmake ipchecker.pro
%make_build

%install
mkdir -p %{buildroot}%{_bindir}
cp ipchecker %{buildroot}%{_bindir}/ipchecker

%files
%{_bindir}/ipchecker

%changelog
* Fri Sep 18 2026 IP Inspector Team <support@ipinspector.local> - 1.1.0-1
- Add IPv6 support and update packaging
