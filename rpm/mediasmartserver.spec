Name:           mediasmartserver
Version:        %{_version}
Release:        1%{?dist}
Summary:        LED status daemon for HP MediaSmart Server and compatible hardware
License:        Zlib
URL:            https://github.com/amd989/mediasmartserverd
Source0:        %{name}-%{version}.tar.gz

%define debug_package %{nil}

BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  systemd-rpm-macros
BuildRequires:  systemd-devel

Requires:       systemd

%description
Linux daemon that controls drive bay status LEDs on HP MediaSmart Server
EX48X, Acer H340/H341/H342, Acer Altos easyStore M2, and Lenovo IdeaCentre
D400. Also supports Arduino-based USB serial LED control for custom builds.

Monitors disk presence and activity via udev/sysfs, provides system update
notifications via LED colors, and supports light show animations.

%prep
%setup -q

%build
make mediasmartserverd

%install
install -D -m 0755 mediasmartserverd %{buildroot}%{_sbindir}/mediasmartserverd
install -D -m 0644 lib/systemd/system/mediasmartserver.service %{buildroot}%{_unitdir}/mediasmartserver.service

%files
%license LICENSE
%doc README.md
%{_sbindir}/mediasmartserverd
%{_unitdir}/mediasmartserver.service

%post
%systemd_post mediasmartserver.service

%preun
%systemd_preun mediasmartserver.service

%postun
%systemd_postun_with_restart mediasmartserver.service

%changelog
