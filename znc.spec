Name: znc
Version: 2.0
Release: 1
Packager: imaginos@imaginos.net
Summary: znc
License: GPL
Group: none
URL: http://sourceforge.net/projects/znc/
BuildRequires: gcc >= 3.2
BuildRequires: openssl >= 0.9b
BuildRoot: /tmp/znc-TMP/
Prefix: /usr
Source: znc.tar.gz

%description
ZNC is an IRC bounce with many advanced features like detaching, multiple users, per channel playback buffer, SSL, transparent DCC bouncing, and c++ module support to name a few.

%prep
%setup -n znc

%build
./configure --prefix=%{prefix}
make all

%clean
rm -rf $RPM_BUILD_ROOT

%install
make install DEST=$RPM_BUILD_ROOT


%files
%{prefix}

