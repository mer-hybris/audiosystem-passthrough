Name:       audiosystem-passthrough
Summary:    AudioSystem Passthrough Helper
Version:    1.0.1
Release:    1
Group:      System/Daemons
License:    BSD
Source0:    %{name}-%{version}.tar.bz2
Source1:    audiosystem-passthrough-dummy-af.service
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  libtool-ltdl-devel
BuildRequires:  pkgconfig(libgbinder) >= 1.0.32
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)

%description
Service for communicating with Android binder services.

%package    devel
Summary:    Binder AudioFlinger or HIDL passthrough helper.
Group:      System/Libraries

%description devel
Common headers for service for communicating with Android binder services.

%package    dummy-af
Summary:    Binder AudioFlinger dummy service.
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description dummy-af
Binder AudioFlinger dummy service.

%prep
%setup -q -n %{name}-%{version}

%build
%make_build

# FIXME: remove this workaround when systemd macros are globally enabled
%if %{undefined _userunitdir}
%define _userunitdir /usr/lib/systemd/user
%endif

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} PREFIX=%{_prefix} install
install -D -m 644 %{SOURCE1} %{buildroot}%{_userunitdir}/audiosystem-passthrough-dummy-af.service
install -d -m 755 %{buildroot}%{_userunitdir}/user-session.target.wants
ln -s ../audiosystem-passthrough-dummy-af.service %{buildroot}%{_userunitdir}/user-session.target.wants/audiosystem-passthrough-dummy-af.service

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
%{_userunitdir}/audiosystem-passthrough-dummy-af.service
%{_userunitdir}/user-session.target.wants/audiosystem-passthrough-dummy-af.service
