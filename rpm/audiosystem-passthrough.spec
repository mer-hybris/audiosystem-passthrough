Name:       audiosystem-passthrough
Summary:    AudioSystem Passthrough Helper
Version:    1.1.1
Release:    1
Group:      System/Daemons
License:    BSD
Source0:    %{name}-%{version}.tar.bz2
Source1:    audiosystem-passthrough-dummy-af.service
Source2:    audiosystem-passthrough-dummy-hw2_0.service
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  libtool-ltdl-devel
BuildRequires:  pkgconfig(libgbinder) >= 1.0.32
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  systemd

%description
Service for communicating with Android binder services.

%package    devel
Summary:    Binder AudioFlinger, HIDL and android.hardware.audio@2.0 passthrough helper.
Group:      System/Libraries

%description devel
Common headers for service for communicating with Android binder services.

%package    dummy-af
Summary:    Binder AudioFlinger dummy service.
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description dummy-af
Binder AudioFlinger dummy service.

%package    dummy-hw2_0
Summary:    Binder android.hardware.audio@2.0 dummy service.
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description dummy-hw2_0
Binder android.hardware.audio@2.0 dummy service.

%prep
%setup -q -n %{name}-%{version}

%build
%make_build

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} PREFIX=%{_prefix} LIBDIR=%{_libdir} install
install -D -m 644 %{SOURCE1} %{buildroot}%{_unitdir}/audiosystem-passthrough-dummy-af.service
install -D -m 644 %{SOURCE2} %{buildroot}%{_userunitdir}/audiosystem-passthrough-dummy-hw2_0.service
install -d -m 755 %{buildroot}%{_unitdir}/multi-user.target.wants
install -d -m 755 %{buildroot}%{_userunitdir}/user-session.target.wants
ln -s ../audiosystem-passthrough-dummy-af.service %{buildroot}%{_unitdir}/multi-user.target.wants/audiosystem-passthrough-dummy-af.service
ln -s ../audiosystem-passthrough-dummy-hw2_0.service %{buildroot}%{_userunitdir}/user-session.target.wants/audiosystem-passthrough-dummy-hw2_0.service

%post

%preun

%postun

%files
%defattr(-,root,root,-)
%{_libexecdir}/%{name}/%{name}

%files devel
%defattr(-,root,root,-)
%dir %{_includedir}/%{name}
%{_includedir}/%{name}/common.h
%{_libdir}/pkgconfig/%{name}.pc

%files dummy-af
%defattr(-,root,root,-)
%{_unitdir}/audiosystem-passthrough-dummy-af.service
%{_unitdir}/multi-user.target.wants/audiosystem-passthrough-dummy-af.service

%files dummy-hw2_0
%defattr(-,root,root,-)
%{_userunitdir}/audiosystem-passthrough-dummy-hw2_0.service
%{_userunitdir}/user-session.target.wants/audiosystem-passthrough-dummy-hw2_0.service
