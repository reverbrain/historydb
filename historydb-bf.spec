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
BuildRequires:	gcc44 gcc44-c++
%else
BuildRequires:	gcc gcc-c++
%endif

BuildRequires:	boost-devel, boost-thread, boost-system
BuildRequires:	cmake28
BuildRequires:	msgpack-devel
BuildRequires:	elliptics-client-devel
BuildRequires:	fastcgi-daemon2-libs-devel
BuildRequires:	eblob-devel

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
export DESTDIR="%{buildroot}"
%if 0%{?rhel} < 6
export CC=gcc44
export CXX=g++44
%{cmake} -DBoost_DIR=/usr/lib64/boost141 -DBOOST_INCLUDEDIR=/usr/include/boost141 -DCMAKE_CXX_COMPILER=g++44 -DCMAKE_C_COMPILER=gcc44 .
%else
%{cmake} .
%endif
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

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

