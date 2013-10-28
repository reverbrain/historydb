Name:		historydb
Version:	0.2.5.1
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
BuildRequires:	cmake msgpack-devel
BuildRequires:	elliptics-client-devel >= 2.24.14.21
BuildRequires:	fastcgi-daemon2-libs-devel
BuildRequires:	libthevoid-devel >= 0.5.5.1

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
%{_bindir}/*
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root,-)
%{_bindir}/*
%{_includedir}/%{name}
%{_libdir}/*.so


%changelog
* Mon Oct 14 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.5.1
- Updated thevoid and elliptics version.

* Mon Sep 30 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.5.0
- Compatibility with elliptics 2.24.14.15

* Thu Aug 08 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.4.2
- Fixed error handling while async add_log_with_activity.

* Tue Aug 06 2013 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.4.1
- Revert "Get rid of cmake-generated files"

* Fri Aug 02 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.4.0
- Enabled cache for adding logs and activity.
- Added logs for async operation.

* Thu Jul 11 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.3.0
- Added hand for appending log and updating activity
- Updated version of elliptics.

* Mon Jul 08 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.2.0
- Removed sharding activity on historydb side. Updated elliptics depency

* Fri Jun 28 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.1.2
- Removed cerr from HistoryDB-TheVoid initialization

* Fri Jun 28 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.1.1
- New version of swarm: 0.5.1

* Fri Jun 28 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.1.0
- Increased timeout for elliptics operation.
- Implemented HistoryDB-TheVoid web-server.
- Updated HistoryDB HTTP and C++ API.
- Added pre-version of combine tool.

* Fri Jun  21 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.0.4
- Fixed missing Content-Length in HTTP response header.

* Mon Jun  10 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.0.3
- Added extra exception check while adding data to user log.

* Wed Jun  5 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.0.2
- Fixed http api. Changed missed parameters http code.

* Tue Jun  4 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.0.1
- Fixed activity statistics methods

* Mon Jun  3 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.2.0.0
- New API implementation. Removed inheritance and virtual functions.

* Tue May  7 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.1.1.1
- Fixed spec file

* Mon May  6 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.1.1.0
- Implemented fastcgi proxy library and include it in debian package.
- Chose json as output format for fastcgi proxy.
- Replaced using write_cas'es to using indexes.
- Improved configuring HistoryDB via fastcgi config file and c++ interface.
- Extended logging HistoryDB.
- Fixed build on rhel 5/6.
- Added separated methods for adding user log and incrementing user activity.

* Wed Apr  3 2013 Evgeniy Polyakov <zbr@ioremap.net> - 0.1.0.0-1
- initial build

