dnl This redefines AC_PROG_CC to a version which errors out instead. This is
dnl because all our tests should be done with the C++ compiler. This should
dnl catch stuff which accidentally uses the C compiler.
AC_DEFUN([AC_PROG_CC], [m4_errprint(__file__:__line__[: Something is trying to use the C compiler. Since this is a C++ project, this should not happen!
])m4_exit(1)])

dnl Needed for AC_PATH_PROGS_FEATURE_CHECK which was added in 2.62
AC_PREREQ([2.62])
dnl Keep the version number in sync with version.h!
AC_INIT([znc], [1.7.5])
LIBZNC_VERSION=1.7.5
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/znc.cpp])
AC_LANG([C++])
AC_CONFIG_HEADERS([include/znc/zncconfig.h])
AH_TOP([#ifndef ZNCCONFIG_H
#define ZNCCONFIG_H])
AH_BOTTOM([#endif /* ZNCCONFIG_H */])

AC_DEFUN([ZNC_AUTO_FAIL], [
	# This looks better in the summary at the end
	$1="not found"
	if test "x$old_$1" != "xauto" ; then
		AC_MSG_ERROR([$2])
	else
		AC_MSG_WARN([$3])
	fi
])

# AC_PROG_CXX sets CXXFLAGS to "-O2 -g" if it is unset which we don't want
CXXFLAGS="$CXXFLAGS "
AC_PROG_CXX
# "Optional" because we want custom error message
AX_CXX_COMPILE_STDCXX_11([noext], [optional])
if test x"$HAVE_CXX11" != x1; then
	AC_MSG_ERROR([Upgrade your compiler. GCC 4.8+ and Clang 3.2+ are known to work.])
fi

appendLib () {
	if test "$LIBS" != ""; then
		LIBS="$LIBS $*"
	else
		LIBS=$*
	fi
}

appendCXX () {
	if test "$CXXFLAGS" != ""; then
		CXXFLAGS="$CXXFLAGS $*"
	else
		CXXFLAGS=$*
	fi
}

appendMod () {
	if test "$MODFLAGS" != ""; then
		MODFLAGS="$MODFLAGS $*"
	else
		MODFLAGS=$*
	fi
}

appendLD () {
	if test "$LDFLAGS" != ""; then
		LDFLAGS="$LDFLAGS $*"
	else
		LDFLAGS=$*
	fi
}

AC_PROG_INSTALL
AC_PROG_GREP
AC_PROG_SED
AC_CANONICAL_HOST
AC_SYS_LARGEFILE
ZNC_VISIBILITY
AC_PATH_PROG([GIT], [git])
PKG_PROG_PKG_CONFIG()

AC_ARG_ENABLE( [debug],
	AS_HELP_STRING([--enable-debug], [enable debugging]),
	[DEBUG="$enableval"],
	[DEBUG="no"])
AC_ARG_ENABLE( [ipv6],
	AS_HELP_STRING([--disable-ipv6], [disable ipv6 support]),
	[IPV6="$enableval"],
	[IPV6="yes"])
AC_ARG_ENABLE( [openssl],
	AS_HELP_STRING([--disable-openssl], [disable openssl]),
	[SSL="$enableval"],
	[SSL="auto"])
AC_ARG_ENABLE( [zlib],
	AS_HELP_STRING([--disable-zlib], [disable zlib]),
	[ZLIB="$enableval"],
	[ZLIB="auto"])
AC_ARG_ENABLE( [perl],
	AS_HELP_STRING([--enable-perl], [enable perl]),
	[PERL="$enableval"],
	[PERL="no"])
AC_ARG_ENABLE( [python],
	AS_HELP_STRING([--enable-python[[[=python3]]]], [enable python.
			By default python3.pc of pkg-config is used, but you can use
			another name, for example python-3.1]),
	[PYTHON="$enableval"],
	[PYTHON="no"])
AC_ARG_ENABLE( [swig],
	AS_HELP_STRING([--enable-swig], [Enable automatic generation of source files needed for modperl/modpython.
			This value is ignored if perl and python are disabled.
			Usually no need to enable it.
			]),
	[USESWIG="$enableval"],
	[USESWIG="auto"])
AC_ARG_ENABLE( [cyrus],
	AS_HELP_STRING([--enable-cyrus], [enable cyrus]),
	[if test "$enableval" = "yes" ; then CYRUS=1; fi],)
AC_ARG_ENABLE( [optimization],
	AS_HELP_STRING([--disable-optimization], [Disable some compiler optimizations to
			decrease memory usage while compiling]),
	[OPTIMIZE="$enableval"],
	[OPTIMIZE="yes"])
AC_ARG_ENABLE( [tdns],
	AS_HELP_STRING([--disable-tdns], [disable threads usage for DNS resolving]),
	[TDNS="$enableval"],
	[TDNS="auto"])
AC_ARG_ENABLE( [run-from-source],
	AS_HELP_STRING([--enable-run-from-source], [ZNC will be runnable without installation]),
	[if test "x$enableval" = "xyes" ; then
		AC_DEFINE([RUN_FROM_SOURCE], [1],
			  [Define if ZNC should be runnable without installation])
	fi
	RUNFROMSOURCE="$enableval"],
	[RUNFROMSOURCE="no"])
AC_ARG_ENABLE( [poll],
	AS_HELP_STRING([--disable-poll], [use select() instead of poll()]),
	[POLL="$enableval"],
	[POLL="yes"])

AC_ARG_WITH( [gtest],
	AS_HELP_STRING([--with-gtest=DIR], [Path to directory with src/gtest-all.cc file.
					If not specified, git submodule will be used.
					If it is not available either, "make test" will fail.]))
if test "x$with_gtest" != xno; then
	AC_SUBST([GTEST_DIR], [$with_gtest])
fi
AC_ARG_WITH([gmock],
	AS_HELP_STRING([--with-gmock=DIR], [Path to directory with src/gmock-all.cc and src/gmock_main.cc files.
					If not specified, git submodule will be used.
					If it is not available either, "make test" will fail.]))
if test "x$with_gmock" != xno; then
	AC_SUBST([GMOCK_DIR], [$with_gmock])
fi


AC_ARG_WITH([systemdsystemunitdir],
	AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
	[
		if test x"$with_systemdsystemunitdir" = xyes; then
			with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)
		fi
	])
if test "x$with_systemdsystemunitdir" != xno; then
	AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir" -a "x$with_systemdsystemunitdir" != xno ])

case "${host_os}" in
	freebsd*)
		# -D__GNU_LIBRARY__ makes this work on fbsd 4.11
		appendCXX -I/usr/local/include -D__GNU_LIBRARY__
		appendLib -L/usr/local/lib -lcompat
		appendMod -L/usr/local/lib
	;;
	solaris*)
		appendLib -lsocket -lnsl -lresolv
		ISSUN=1
	;;
	cygwin)
		# We don't want to use -std=gnu++11 instead of -std=c++11, but among other things, -std=c++11 defines __STRICT_ANSI__ which makes cygwin not to compile: undefined references to strerror_r, to fdopen, to strcasecmp, etc (their declarations in system headers are between ifdef)
		appendCXX -U__STRICT_ANSI__
		ISCYGWIN=1
	;;
	darwin*)
		ISDARWIN=1

		AC_PATH_PROG([BREW], [brew])
		if test -n "$BREW"; then
			# add default homebrew paths

			if test "x$HAVE_ICU" != "xno"; then
				AC_MSG_CHECKING([icu4c via homebrew])
				icu4c_prefix=`$BREW --prefix icu4c`
				if test -n "$icu4c_prefix"; then
					export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$icu4c_prefix/lib/pkgconfig"
					AC_MSG_RESULT([$icu4c_prefix])
				else
					AC_MSG_RESULT([no])
				fi
			fi

			if test "x$PYTHON" != "xno"; then
				brew_python_pc="$PYTHON"
				# This is duplication of non-darwin python logic below...
				if test "x$brew_python_pc" = "xyes"; then
					brew_python_pc="python3"
				fi
				AC_MSG_CHECKING([python3 via homebrew])
				python3_prefix=`$BREW --prefix python3`
				if test -n "$python3_prefix"; then
					python3_prefix=`find "$python3_prefix/" -name $brew_python_pc.pc | head -n1`
					if test -n "$python3_prefix"; then
						python3_prefix=`dirname "$python3_prefix"`
						export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$python3_prefix"
						AC_MSG_RESULT([$python3_prefix])
					else
						AC_MSG_RESULT([no $brew_python_pc.pc found])
					fi
				else
					AC_MSG_RESULT([no])
				fi
			fi

			if test "x$SSL" != "xno"; then
				AC_MSG_CHECKING([openssl via homebrew])
				openssl_prefix=`$BREW --prefix openssl`
				if test -n "$openssl_prefix"; then
					export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$openssl_prefix/lib/pkgconfig"
					AC_MSG_RESULT([$openssl_prefix])
				else
					AC_MSG_RESULT([no])
				fi
			fi
		fi
	;;
esac

if test "$DEBUG" != "no"; then
	appendCXX -ggdb3
	AC_DEFINE([_DEBUG], [1], [Define for debugging])
	if test "x$ISCYGWIN" != x1; then
		# These enable some debug options in g++'s STL, e.g. invalid use of iterators
		# But they cause crashes on cygwin while loading modules
		AC_DEFINE([_GLIBCXX_DEBUG], [1], [Enable extra debugging checks in libstdc++])
		AC_DEFINE([_GLIBCXX_DEBUG_PEDANTIC], [1], [Enable extra debugging checks in libstdc++])
	fi
else
	if test "x$OPTIMIZE" = "xyes"; then
		appendCXX -O2

		# In old times needed to define _FORTIFY_SOURCE to 2 ourself.
		# Then GCC started to define it itself to 2. It was ok.
		# But then GCC 4.7 started to define it to 0 or 2 depending on optimization level, and it started to conflict with our define.
		AC_MSG_CHECKING([whether compiler predefines _FORTIFY_SOURCE])
		AC_COMPILE_IFELSE([
			AC_LANG_PROGRAM([[
			]], [[
				#ifndef _FORTIFY_SOURCE
				#error "Just checking, nothing fatal here"
				#endif
			]])
		], [
			AC_MSG_RESULT([yes])
		], [
			AC_MSG_RESULT([no])
			appendCXX "-D_FORTIFY_SOURCE=2"
		])
	fi
fi

if test "$IPV6" != "no"; then
	AC_DEFINE([HAVE_IPV6], [1], [Define if IPv6 support is enabled])
fi

if test "x$GXX" = "xyes"; then
	appendCXX -Wall -W -Wno-unused-parameter -Woverloaded-virtual -Wshadow
fi

if test "$POLL" = "yes"; then
	# poll() is broken on Mac OS, it fails with POLLNVAL for pipe()s.
	if test -n "$ISDARWIN"
	then
		# Did they give us --enable-poll?
		if test -n "$enable_poll"
		then
			# Yes, they asked for this.
			AC_MSG_WARN([poll() is known to be broken on Mac OS X. You have been warned.])
		else
			# No, our default value of "yes" got applied.
			AC_MSG_WARN([poll() is known to be broken on Mac OS X. Using select() instead.])
			AC_MSG_WARN([Use --enable-poll for forcing poll() to be used.])
			POLL=no
		fi
	fi
	if test "$POLL" = "yes"; then
		AC_DEFINE([CSOCK_USE_POLL], [1], [Use poll() instead of select()])
	fi
fi

AC_CHECK_LIB( gnugetopt, getopt_long,)
AC_CHECK_FUNCS([lstat getopt_long getpassphrase clock_gettime tcsetattr])

# ----- Check for dlopen

AC_SEARCH_LIBS([dlopen], [dl], [],
		[AC_MSG_ERROR([Could not find dlopen. ZNC will not work on this box until you upgrade this ancient system or at least install the necessary system libraries.])])

# ----- Check for pthreads

AX_PTHREAD([
	AC_DEFINE([HAVE_PTHREAD], [1], [Define if you have POSIX threads libraries and header files.])
	appendCXX "$PTHREAD_CFLAGS"
	appendLib "$PTHREAD_LIBS"
], [
	AC_MSG_ERROR([This compiler/OS doesn't seem to support pthreads.])
])

# Note that old broken systems, such as OpenBSD, NetBSD, which don't support AI_ADDRCONFIG, also have thread-unsafe getaddrinfo().
# Gladly, they fixed thread-safety before support of AI_ADDRCONFIG, so this can be abused to detect the thread-safe getaddrinfo().
#
# TODO: drop support of blocking DNS at some point. OpenBSD supports AI_ADDRCONFIG since Nov 2014, and their getaddrinfo() is thread-safe since Nov 2013. NetBSD's one is thread-safe since ages ago.
DNS_TEXT=blocking
if test "x$TDNS" != "xno"; then
	old_TDNS=$TDNS
	AC_MSG_CHECKING([whether getaddrinfo() supports AI_ADDRCONFIG])
	AC_COMPILE_IFELSE([
		AC_LANG_PROGRAM([[
			#include <sys/types.h>
			#include <sys/socket.h>
			#include <netdb.h>
		]], [[
			int x = AI_ADDRCONFIG;
			(void) x;
		]])
	], [
		AC_MSG_RESULT([yes])
		TDNS=yes
	], [
		AC_MSG_RESULT([no])
		TDNS=no
	])
	if test "x$TDNS" = "xyes"; then
		DNS_TEXT=threads
		AC_DEFINE([HAVE_THREADED_DNS], [1], [Define if threaded DNS is enabled])
	else
		ZNC_AUTO_FAIL([TDNS],
			[support for threaded DNS not found. Try --disable-tdns.
Disabling it may result in a slight performance decrease but will not have any other side-effects],
			[support for threaded DNS not found, so DNS resolving will be blocking])
	fi
fi

# ----- Check for openssl

SSL_TEXT="$SSL"
if test "x$SSL" != "xno"; then
	old_SSL=$SSL
	PKG_CHECK_MODULES([openssl], [openssl], [
		appendLib "$openssl_LIBS"
		appendCXX "$openssl_CFLAGS"
	], [
		# Don't reorder this!
		# On some arches libssl depends on libcrypto without linking to it :(
		AC_CHECK_LIB( crypto, BIO_new,, SSL=no ; SSL_TEXT="no (libcrypt not found)" )
		AC_CHECK_LIB( ssl, SSL_shutdown,, SSL=no ; SSL_TEXT="no (libssl not found)" )
	])

	if test "x$SSL" != "xno"; then
		AC_MSG_CHECKING([whether openssl is usable])
		AC_LINK_IFELSE([
			AC_LANG_PROGRAM([[
				#include <openssl/ssl.h>
				#include <openssl/dh.h>
			]], [[
				SSL_CTX* ctx = SSL_CTX_new(SSLv23_method());
				SSL* ssl = SSL_new(ctx);
				DH* dh = DH_new();
				DH_free(dh);
				SSL_free(ssl);
				SSL_CTX_free(ctx);
			]])
		], [
			AC_MSG_RESULT([yes])
		], [
			AC_MSG_RESULT([no])
			SSL=no
			SSL_TEXT="no (openssl not usable)"
		])

	fi

	if test "x$SSL" = "xno" ; then
		ZNC_AUTO_FAIL([SSL],
			[OpenSSL not found. Try --disable-openssl.],
			[OpenSSL was not found and thus disabled])
		NOSSL=1
	else
		AC_DEFINE([HAVE_LIBSSL], [1], [Define if openssl is enabled])
		SSL=yes
		SSL_TEXT=yes
	fi
else
	NOSSL=1
	SSL_TEXT="no (explicitly disabled)"
fi

# ----- Check for zlib

old_ZLIB="$ZLIB"
ZLIB_TEXT="$ZLIB"
if test "x$ZLIB" != "xno"; then
	AC_MSG_CHECKING([whether zlib is usable])
	my_saved_LIBS="$LIBS"
	appendLib "-lz"
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[
			#include "zlib.h"
		]], [[
			z_stream zs;
			(void) deflateInit2(&zs, 0, 0, 0, 0, 0);
			(void) deflate(&zs, 0);
		]])
		], [
			AC_MSG_RESULT([yes])
			ZLIB=yes
			ZLIB_TEXT=yes
		], [
			AC_MSG_RESULT([no])
			ZLIB=no
			ZLIB_TEXT="no (libz not found)"
	])
	if test "x$ZLIB" = "xno"; then
		ZNC_AUTO_FAIL([ZLIB],
			[zlib was not found. Try --disable-zlib],
			[zlib was not found and thus disabled])
		LIBS="$my_saved_LIBS"
	else
		AC_DEFINE([HAVE_ZLIB], [1], [Define if zlib is available])
	fi
fi

AC_ARG_ENABLE( [charset],
	AS_HELP_STRING([--disable-charset], [disable ICU support]),
	[HAVE_ICU="$enableval"],
	[HAVE_ICU="auto"])
if test "x$HAVE_ICU" != "xno"
then
	old_HAVE_ICU="$HAVE_ICU"
	PKG_CHECK_MODULES([icu], [icu-uc], [
		appendLib "$icu_LIBS"
		appendCXX "$icu_CFLAGS"
		HAVE_ICU=yes
		AC_DEFINE([HAVE_ICU], [1], [Enable ICU library for Unicode handling])
		AC_DEFINE([U_USING_ICU_NAMESPACE], [0], [Do not clutter global namespace with ICU C++ stuff])
	], [
		ZNC_AUTO_FAIL([HAVE_ICU],
			      [support for charset conversion not found. Try --disable-charset.],
			      [support for charset conversion not found and thus disabled])
		HAVE_ICU="no (icu-uc not found via pkg-config)"
	])
fi

# For integration test only
PKG_CHECK_MODULES([qt], [Qt5Network >= 5.4], [], [:])

AC_ARG_WITH( [module-prefix],
	AS_HELP_STRING([--with-module-prefix], [module object code [LIBDIR/znc]]),
	[MODDIR=$withval],
	[MODDIR="${libdir}/znc"] )

AC_ARG_WITH( [module-data-prefix],
	AS_HELP_STRING([--with-module-data-prefix=DIR],
		[static module data (webadmin skins) [DATADIR/znc]]),
	[DATADIR=$withval],
	[DATADIR="${datadir}/znc"] )

appendMod "$CXXFLAGS"
appendMod "$CFLAG_VISIBILITY"

if test -z "$ISSUN" -a -z "$ISDARWIN" -a -z "$ISCYGWIN"; then
	# This is an unknown compiler flag on some OS
	appendLD -Wl,--export-dynamic
fi

if test -z "$ISCYGWIN" ; then
	# cygwin doesn't need -fPIC, everything else does (for modules)
	# warning: -fPIC ignored for target (all code is position independent)
	appendMod -fPIC
else
	# But cygwin does want most of ZNC in a shared lib
	# See https://cygwin.com/ml/cygwin-apps/2015-07/msg00108.html for the reasoning behind the name.
	LIBZNC="cygznc-${LIBZNC_VERSION}.dll"
	LIBZNCDIR="$bindir"
	# See above about __STRICT_ANSI__
	qt_CFLAGS="$qt_CFLAGS -U__STRICT_ANSI__"
fi

if test -z "$ISDARWIN"; then
	MODLINK="-shared"
else
	# Mac OS X differentiates between shared libs (-dynamiclib)
	# and loadable modules (-bundle).
	MODLINK="-bundle -flat_namespace -undefined suppress"
	# TODO test if -twolevel_namespace and/or
	# -undefined dynamic_lookup work
	# (dynamic_lookup might only work on 10.4 and later)
fi

if test "x$PERL" != xno -o "x$PYTHON" != xno; then
	old_USESWIG="$USESWIG"
	if test "x$USESWIG" != "xno"; then
		AC_PROG_SWIG([3.0.0])
		test -z "$SWIG" && USESWIG=no
		if test "x$USESWIG" = xno -a "x$old_USESWIG" = yes; then
			AC_MSG_ERROR([Could not found appropriate SWIG installation. Check config.log for details.])
		fi
	fi
	if test -r "$srcdir/modules/modperl/generated.tar.gz" -a -r "$srcdir/modules/modpython/generated.tar.gz"; then
		AC_MSG_NOTICE([modperl/modpython files are found, disabling SWIG])
		USESWIG=no
	fi
	if test "x$USESWIG" = xno; then
		if test ! -r "$srcdir/modules/modperl/generated.tar.gz" -o ! -r "$srcdir/modules/modpython/generated.tar.gz"; then
			AC_MSG_ERROR([Can not build modperl/modpython. Either install SWIG, or build ZNC from a tarball, or disable modperl/modpython. Check config.log for details.])
		else
			AC_MSG_NOTICE([modperl/modpython files are found, no SWIG needed])
		fi
		USESWIG="not needed"
		SWIG=""
	else
		USESWIG=yes
	fi
else
	if test "x$USESWIG" = "xyes"; then
		AC_MSG_WARN([swig is used only for perl and python, but both are disabled. Disabling swig.])
	fi
	USESWIG='not needed'
fi

PERL_TEXT="$PERL"
if test "x$PERL" != "xno"; then
	old_PERL="$PERL"
	AC_PATH_PROG([PERL_BINARY], [perl], [])
	if test -n "$PERL_BINARY" && eval "$PERL_BINARY -e'use 5.010'"; then
		my_saved_LDFLAGS="$LDFLAGS"
		appendLD `$PERL_BINARY -MExtUtils::Embed -e ccopts -e ldopts`
		AC_CHECK_LIB(perl, perl_alloc,
			     [: No, we do not want autoconf to do sth automatically],
			     PERL="no" ; PERL_TEXT="no (libperl not found)")
		LDFLAGS="$my_saved_LDFLAGS"
	else
		PERL="no"
		PERL_TEXT="no (perl binary not found or too old)"
	fi
	if test "x$PERL" = "xno"; then
		ZNC_AUTO_FAIL([PERL],
			[perl not found. Try --disable-perl.],
			[perl was not found and thus disabled])
		PERL_BINARY=""
	else
		PERL="yes"
		PERL_TEXT="yes"
	fi
fi

PYTHON_TEXT="$PYTHON"
if test "x$PYTHON" != "xno"; then
	# Default value for just --enable-python
	if test "x$PYTHON" = "xyes"; then
		PYTHON="python3"
	fi
	old_PYTHON="$PYTHON"
	if test -z "$PKG_CONFIG"; then
		AC_MSG_ERROR([pkg-config is required for modpython.])
	fi
	PKG_CHECK_MODULES([python], [$PYTHON-embed >= 3.0],, [
		PKG_CHECK_MODULES([python], [$PYTHON >= 3.0],, AC_MSG_ERROR([$PYTHON.pc not found or is wrong. Try --disable-python or install python3.]))
	])
	my_saved_LIBS="$LIBS"
	my_saved_CXXFLAGS="$CXXFLAGS"
	appendLib $python_LIBS
	appendCXX $python_CFLAGS
	AC_CHECK_FUNC([Py_Initialize], [], [PYTHON="no" ; PYTHON_TEXT="no (libpython not found)"])
	if test "x$PYTHON" != "xno"; then
		# Yes, modpython depends on perl.
		AC_PATH_PROG([PERL_BINARY], [perl])
		if test -z "$PERL_BINARY"; then
			AC_MSG_ERROR([To compile modpython you need to be able to execute perl scripts. Try --disable-python or install perl.])
		fi
		LIBS="$my_saved_LIBS"
		CXXFLAGS="$my_saved_CXXFLAGS"
	fi
	if test "x$HAVE_ICU" != "xyes"; then
		AC_MSG_ERROR([Modpython requires ZNC to be compiled with charset support, but ICU library not found. Try --disable-python or install libicu.])
	fi
	if test "x$PYTHON" = "xno"; then
		ZNC_AUTO_FAIL([PYTHON],
			[python not found. Try --disable-python.],
			[python was not found and thus disabled])
		PYTHONCFG_BINARY=""
	else
		PYTHON="yes"
		PYTHON_TEXT="yes"
	fi
fi

if test -n "$CYRUS"; then
	AC_CHECK_LIB( sasl2, sasl_server_init,
		[: Dont let autoconf add -lsasl2, Makefile handles that],
		AC_MSG_ERROR([could not find libsasl2. Try --disable-cyrus.]))
fi

# Check if we want modtcl
AC_ARG_ENABLE( [tcl],
	AS_HELP_STRING([--enable-tcl], [enable modtcl]),
	[TCL="$enableval"],
	[TCL="no"])

AC_ARG_WITH( [tcl-flags],
	AS_HELP_STRING([--with-tcl-flags=FLAGS],
		[The flags needed for compiling and linking modtcl]),
	[TCL_FLAGS="$withval"],)

if test x"$TCL" = "xyes"
then
	AC_ARG_WITH( [tcl],
		AS_HELP_STRING([--with-tcl=DIR],
			[directory containing tclConfig.sh]),
			TCL_DIR="${withval}")

	# This will need to be extended in the future, but I don't think
	# it's a good idea to stuff a shitload of random stuff in here right now
	for path in $TCL_DIR /usr/lib /usr/lib/tcl8.4 /usr/lib/tcl8.5 /usr/lib/tcl8.6
	do
		file="${path}/tclConfig.sh"
		AC_MSG_CHECKING([for ${file}])
		if test -r ${file}
		then
			TCL_CONF=${file}
			AC_MSG_RESULT([yes])
			break
		fi
		AC_MSG_RESULT([no])
	done

	if test x"${TCL_CONF}" = x
	then
		# They --enable-tcl'd, so give them some sane default
		TCL_FLAGS="-I/usr/include/tcl -ltcl"
		AC_MSG_WARN([Could not find tclConfig.sh, using some sane defaults.])
	else
		AC_MSG_CHECKING([modtcl flags])
		. ${TCL_CONF}
		# eval because those vars depend on other vars in there
		eval "TCL_LIB_SPEC=\"${TCL_LIB_SPEC}\""
		eval "TCL_INCLUDE_SPEC=\"${TCL_INCLUDE_SPEC}\""
		TCL_FLAGS="$TCL_INCLUDE_SPEC $TCL_LIB_SPEC"
		AC_MSG_RESULT([$TCL_FLAGS])
	fi
	my_saved_LIBS="$LIBS"
	appendLib "$TCL_FLAGS"
	AC_CHECK_FUNC([Tcl_CreateInterp], [TCL_TEST=yes], [TCL_TEST=no])
	if test x"$TCL_TEST" = "xno"; then
		AC_MSG_ERROR([tcl not found, try --disable-tcl, or install tcl properly. If tcl is installed to a non-standard path, use --enable-tcl --with-tcl=/path])
	fi
	LIBS="$my_saved_LIBS"
fi

AC_CACHE_CHECK([for GNU make], [ac_cv_path_GNUMAKE], [
	AC_PATH_PROGS_FEATURE_CHECK([GNUMAKE], [make gmake], [[
			if $ac_path_GNUMAKE --version | $GREP GNU > /dev/null; then
				ac_cv_path_GNUMAKE=$ac_path_GNUMAKE
				ac_path_GNUMAKE_found=:
			fi
		]], [AC_MSG_ERROR([could not find GNU make])]
	)
])
GNUMAKE=`echo $ac_cv_path_GNUMAKE | $SED "s%.*/%%"`

# this is in the end, for not trying to include it when it doesn't exist yet
appendCXX "-include znc/zncconfig.h"
appendMod "-include znc/zncconfig.h"

AC_SUBST([CXXFLAGS])
AC_SUBST([CPPFLAGS])
AC_SUBST([MODFLAGS])
AC_SUBST([LDFLAGS])
AC_SUBST([LIBS])
AC_SUBST([LIBZNC])
AC_SUBST([LIBZNCDIR])
AC_SUBST([ISCYGWIN])
AC_SUBST([MODLINK])
AC_SUBST([NOSSL])
AC_SUBST([TCL_FLAGS])
AC_SUBST([CYRUS])
AC_SUBST([MODDIR])
AC_SUBST([DATADIR])
AC_SUBST([PERL])
AC_SUBST([PYTHON])
AC_SUBST([SWIG])
AC_SUBST([python_CFLAGS])
AC_SUBST([python_LIBS])
AC_SUBST([qt_CFLAGS])
AC_SUBST([qt_LIBS])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([znc-buildmod])
AC_CONFIG_FILES([man/Makefile])
AC_CONFIG_FILES([znc.pc])
AC_CONFIG_FILES([znc-uninstalled.pc])
AC_CONFIG_FILES([modules/Makefile])
AC_OUTPUT

if test "x$ISCYGWIN" = x1; then
		# Side effect of undefining __STRICT_ANSI__
		# http://llvm.org/bugs/show_bug.cgi?id=13530
		echo                                 >> include/znc/zncconfig.h
		echo '#ifndef ZNCCONFIG_H_ADDITIONS' >> include/znc/zncconfig.h
		echo '#define ZNCCONFIG_H_ADDITIONS' >> include/znc/zncconfig.h
		echo '#ifdef __clang__'              >> include/znc/zncconfig.h
		echo 'struct __float128;'            >> include/znc/zncconfig.h
		echo '#endif'                        >> include/znc/zncconfig.h
		echo '#endif'                        >> include/znc/zncconfig.h
fi

echo
echo ZNC AC_PACKAGE_VERSION configured
echo
echo "The configure script is deprecated and will be removed from ZNC"
echo "in some future version. Use either CMake or the convenience wrapper"
echo "configure.sh which takes the same parameters as configure."
echo
echo "prefix:       $prefix"
echo "debug:        $DEBUG"
echo "ipv6:         $IPV6"
echo "openssl:      $SSL_TEXT"
echo "dns:          $DNS_TEXT"
echo "perl:         $PERL_TEXT"
echo "python:       $PYTHON_TEXT"
echo "swig:         $USESWIG"
if test x"$CYRUS" = "x" ; then
	echo "cyrus:        no"
else
	echo "cyrus:        yes"
fi
if test x"$TCL_FLAGS" = "x" ; then
	echo "tcl:          no"
else
	echo "tcl:          yes"
fi
echo "charset:      $HAVE_ICU"
echo "zlib:         $ZLIB_TEXT"
echo "run from src: $RUNFROMSOURCE"
echo
echo "Now you can run \"$GNUMAKE\" to compile ZNC"

