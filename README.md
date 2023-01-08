# [![ZNC](https://wiki.znc.in/resources/assets/wiki.png)](https://znc.in) - An advanced IRC bouncer

[![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/znc/znc/build.yml?branch=master&label=linux)](https://github.com/znc/znc/actions/workflows/build.yml)
[![Jenkins Build Status](https://img.shields.io/jenkins/s/https/jenkins.znc.in/job/znc/job/znc/job/master.svg?label=freebsd)](https://jenkins.znc.in/job/znc/job/znc/job/master/)
[![AppVeyor Build status](https://img.shields.io/appveyor/ci/DarthGandalf/znc/master.svg?label=windows)](https://ci.appveyor.com/project/DarthGandalf/znc/branch/master)
[![Coverage Status](https://img.shields.io/codecov/c/github/znc/znc.svg)](https://codecov.io/gh/znc/znc)

## Table of contents

- [Minimal Requirements](#minimal-requirements)
- [Optional Requirements](#optional-requirements)
- [Installing ZNC](#installing-znc)
- [Setting up znc.conf](#setting-up-zncconf)
- [Special config options](#special-config-options)
- [Using ZNC](#using-znc)
- [File Locations](#file-locations)
- [ZNC's config file](#zncs-config-file)
- [Writing own modules](#writing-own-modules)
- [Further information](#further-information)

## Minimal Requirements

Core:

* GNU make
* pkg-config
* GCC 4.8 or clang 3.2
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
* python 3.4+ and its bundled libpython
* perl is a build dependency
* macOS: Python from Homebrew is preferred over system version
* SWIG if building from git

cyrusauth:
* This module needs cyrus-sasl2

Character Encodings:
* To get proper character encoding and charsets install ICU (`libicu4-dev`)

I18N (UI translation)
* Boost.Locale
* gettext is a build dependency

## Installing ZNC

Installation from source code is performed using the CMake toolchain.

```shell
mkdir build
cd build
cmake ..
make
make install
```

You can use `cmake-gui` or `ccmake` for more interactiveness.

There is also `configure.sh` which should make migration to CMake easier:
it accepts the same parameters as old `./configure`,
but calls CMake with CMake-style parameters.

Note for FreeBSD users:
By default base OpenSSL is selected.
If you want the one from ports, use `-DOPENSSL_ROOT_DIR=/usr/local`.

For troubleshooting, `cmake --system-information` will show you details.

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
[Configuration](https://wiki.znc.in/Configuration) page.
**Warning: it is better not to edit config while ZNC is running.** Use the
[webadmin] and [controlpanel] modules instead.

[webadmin]:https://wiki.znc.in/Webadmin
[controlpanel]:https://wiki.znc.in/Controlpanel

If you changed some settings while ZNC is running, a simple
`pkill -SIGUSR1 znc` will make ZNC rewrite its config file. Alternatively
you can use `/msg *status saveconfig`

## Writing own modules

You can write your own modules in either C++, python or perl.

C++ modules are compiled by either saving them in the modules source dir
and running make or with the `znc-buildmod` shell script.

For additional info look in the wiki:

- [Writing modules](https://wiki.znc.in/Writing_modules)

Perl modules are loaded through the global module
[ModPerl](https://wiki.znc.in/Modperl).

Python modules are loaded through the global module
[ModPython](https://wiki.znc.in/Modpython).

## Further information

Please visit https://znc.in/ or #znc on Libera.Chat if you still have questions:
- [ircs://irc.libera.chat:6697/#znc](ircs://irc.libera.chat:6697/#znc)

You can get the latest development version with git:
`git clone https://github.com/znc/znc.git --recursive`
