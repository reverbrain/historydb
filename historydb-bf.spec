Name:		historydb
Version:	0.1.0.0
Release:	1%{?dist}
Summary:	Sccalable distributed archive system

Group:		System Environment/Base
License:	LGPLv2+
URL:		http://www.reverbrain.com
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%if 0%{?rhel} < 6
BuildRequires:	boost141-devel
%else
BuildRequires:	boost-devel
%endif
BuildRequires:	cmake28
BuildRequires:	msgpack-devel
BuildRequires:	elliptics-devel
BuildRequires:	elliptics-c++-devel
BuildRequires:	eblob-devel < 0.18
BuildRequires:	fastcgi-daemon2-libs-devel


%description
History DB is a trully scalable (hundreds of millions updates per day)
distributed archive system with per user and per day activity statistics.

%package devel
Summary:	historydb development package
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}

%description devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.

%prep
%setup -q


%build
%if 0%{?rhel} < 6
CXXFLAGS="-pthread -I/usr/include/boost141" LDFLAGS="-L/usr/lib64/boost141" %{__cmake28} .
%else
%{__cmake28} .
%endif
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%post -n /sbin/ldconfig
%postun -n /sbin/ldconfig

%files
%defattr(-,root,root,-)
%doc README.md
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root,-)
%{_includedir}/%{name}
%{_libdir}/*.so


%changelog
* Wed Apr  3 2013 Evgeniy Polyakov <zbr@ioremap.net> - 0.1.0.0-1
- initial build

