# [![ZNC](http://wiki.znc.in/resources/assets/wiki.png)](http://znc.in) - An advanced IRC bouncer

[![Travis Build Status](https://img.shields.io/travis/znc/znc/master.svg?label=linux%2Fmacos)](https://travis-ci.org/znc/znc)
[![Jenkins Build Status](https://img.shields.io/jenkins/s/http/jenkins.znc.in/job/znc/master.svg?label=freebsd)](http://jenkins.znc.in/job/znc/job/master/)
[![AppVeyor Build status](https://img.shields.io/appveyor/ci/DarthGandalf/znc/master.svg?label=windows)](https://ci.appveyor.com/project/DarthGandalf/znc/branch/master)
[![Bountysource](https://www.bountysource.com/badge/tracker?tracker_id=1759)](https://www.bountysource.com/trackers/1759-znc?utm_source=1759&utm_medium=shield&utm_campaign=TRACKER_BADGE)
[![Coverage Status](https://coveralls.io/repos/znc/znc/badge.svg?branch=master&service=github)](https://coveralls.io/github/znc/znc?branch=master)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/6778.svg)](https://scan.coverity.com/projects/znc-coverity)

[![Build Status](https://travis-ci.org/znc/znc.png?branch=master)](https://travis-ci.org/znc/znc)

## Table of contents

- Minimal Requirements
- Optional Requirements
- Installing ZNC
- Setting up znc.conf
- Special config options
- Using ZNC
- File Locations
- ZNC's config file
- Writing own modules
- Further infos

## Minimal Requirements

Core:

* GNU make
* pkg-config
* GCC 4.7 or clang 3.2
* Either of:
    * autoconf and automake (but only if building from git, not from tarball)
    * CMake

## Optional Requirements

SSL/TLS support:
* openssl 0.9.7d or later
    * try installing openssl-dev, openssl-devel or libssl-dev
    * macOS: OpenSSL from Homebrew is preferred over system

modperl:
* perl and its bundled libperl
* SWIG if building from git

modpython:
* python and its bundled libpython
* perl is required
* macOS: Python from Homebrew is preferred over system version
* SWIG if building from git

cyrusauth:
* This module needs cyrus-sasl2

Character Encodings:
* To get proper character encoding and charsets install ICU (`libicu4-dev`)

## Installing ZNC

Currently there are 2 build systems in place: CMake and `./configure`.
`./configure` will eventually be removed.
There is also `configure.sh` which should make migration to CMake easier:
it accepts the same parameters as `./configure`,
but calls CMake with CMake-style parameters.

### Installing with CMake

Installation from source code is performed using the CMake toolchain.

```shell
cmake .
make
make install
```

You can use `cmake-gui` or `ccmake` for more interactiveness.

Note for FreeBSD users:
By default base OpenSSL is selected.
If you want the one from ports, use `-DOPENSSL_ROOT_DIR=/usr/local`.

For troubleshooting, `cmake --system-information` will show you details.

### Installing with `./configure`

Installation from source code is performed using the `automake` toolchain.
If you are building from git, you will need to run `./autogen.sh` first to
produce the `configure` script.

```shell
./configure
make
make install
```

You can use `./configure --help` if you want to get a list of options, though
the defaults should be suiting most needs.

## Setting up znc.conf

For setting up a configuration file in `~/.znc` you can simply do
`znc --makeconf` or `./znc --makeconf` for in-place execution.

If you are using SSL you should do `znc --makepem`

## Special config options

When you create your ZNC configuration file via --makeconf, you are asked
two questions which might not be easy to understand.

> Number of lines to buffer per channel

How many messages should be buffered for each channel. When you connect to
ZNC you get a buffer replay for each channel which shows what was said
last. This option selects the number of lines this replay should consist
of. Increasing this can greatly increase ZNC's memory usage if you are
hosting many users. The default value should be fine for most setups.

> Would you like to keep buffers after replay?

If this is disabled, you get the buffer playback only once and then it is
deleted. If this is enabled, the buffer is not deleted. This may be useful
if you regularly use more than one client to connect to ZNC.

## Using ZNC

Once you have started ZNC you can connect with your favorite IRC-client to
ZNC. You should use `username:password` as the server password (e.g.
`/pass user:pass`).

Once you are connected you can do `/msg *status help` for some commands.
Every module you have loaded (`/msg *status listmods`) should additionally
provide `/msg *modulename help`

## File Locations

In its data dir (`~/.znc` is default) ZNC saves most of its data. The only
exception are modules and module data, which are saved in
`<prefix>/lib/znc` and `<prefix>/share/znc`, and the znc binary itself.
More modules (e.g. if you install some later) can be saved in
`<data dir>/modules` (-> `~/.znc/modules`).

In the datadir is only one file:

- `znc.pem` - This is the server certificate ZNC uses for listening and is
created with `znc --makepem`.

These directories are also in there:

- configs - Contains `znc.conf` (ZNC's config file) and backups of older
  configs.
- modules - ZNC also looks in here for a module.
- moddata - Global modules save their settings here.
  (e.g. webadmin saves the current skin name in here)
- users   - This is per-user data and mainly contains just a moddata
  directory.

## ZNC's config file

This file shouldn't be too hard too understand. An explanation of all the
items can be found on the
[Configuration](http://wiki.znc.in/Configuration)-Page.
**Warning: better not to edit config, while ZNC is running.** Use  the
[webadmin] and [controlpanel] modules instead.

[webadmin]:http://wiki.znc.in/Webadmin
[controlpanel]:http://wiki.znc.in/Controlpanel

If you changed some settings while ZNC is running, a simple
`pkill -SIGUSR1 znc` will make ZNC rewrite its config file. Alternatively
you can use `/msg *status saveconfig`

## Writing own modules

You can write your own modules in either C++, python or perl.

C++ modules are compiled by either saving them in the modules source dir
and running make or with the `znc-buildmod` shell script.

For additional info look in the wiki:

- [Writing modules](http://wiki.znc.in/Writing_modules)

Perl modules are loaded through the global module
[ModPerl](http://wiki.znc.in/Modperl).

Python modules are loaded through the global module
[ModPython](http://wiki.znc.in/Modpython).

## Further infos

Please visit http://znc.in/ or
[#znc on freenode](ircs://irc.freenode.net:6697/#znc) if you still have
questions.

You can get the latest development version with git:
`git clone https://github.com/znc/znc.git --recursive`
