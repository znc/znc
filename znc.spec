Name: znc
Version: 0.051
Release: 1
Packager: imaginos@imaginos.net
Summary: znc - an advanced IRC bouncer
License: GPL
Group: Applications/Communications
URL: http://sourceforge.net/projects/znc/
BuildRequires: gcc >= 3.2
BuildRequires: openssl >= 0.9.7d
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Prefix: /usr
Source: %{name}-%{version}.tar.gz

%description
ZNC is an IRC bouncer with many advanced features like detaching, multiple users, per channel playback buffer, SSL, transparent DCC bouncing, and c++ module support to name a few.

%prep
%setup -n %{name}-%{version}

%build
./configure --prefix=%{prefix}
make all

%clean
rm -rf $RPM_BUILD_ROOT

%install
make install DESTDIR=$RPM_BUILD_ROOT


%files
%doc AUTHORS
%doc docs/*.html docs/*.txt docs/*.pm
%{prefix}

