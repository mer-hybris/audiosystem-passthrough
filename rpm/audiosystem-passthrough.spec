Name:       audiosystem-passthrough
Summary:    AudioSystem Passthrough Helper
Version:    1.0.0
Release:    1
Group:      System/Daemons
License:    BSD
Source0:    %{name}-%{version}.tar.bz2

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

%prep
%setup -q -n %{name}-%{version}

%build
%make_build

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} PREFIX=%{_prefix} install

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
