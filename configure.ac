dnl Needed for AC_PATH_PROGS_FEATURE_CHECK which was added in 2.62
AC_PREREQ([2.62])
dnl Keep the version number in sync with main.h!
AC_INIT([znc], [0.202])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([znc.cpp])
AC_LANG([C++])
AC_CONFIG_HEADERS([zncconfig.h])
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
AC_PROG_INSTALL
AC_PROG_GREP
AC_PROG_SED
AC_CANONICAL_HOST
AC_SYS_LARGEFILE
gl_VISIBILITY

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

appendCXX "-D_FORTIFY_SOURCE=2"

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
		ISCYGWIN=1
	;;
	darwin*)
		ISDARWIN=1
	;;
esac

AC_ARG_WITH( [openssl],
	AS_HELP_STRING([--with-openssl=DIR], [openssl installation prefix]),
	[OPENSSL=$withval],)
AC_ARG_ENABLE( [extra],
	AS_HELP_STRING([--enable-extra], [enable some extra modules]),
	[EXTRA="$enableval"],
	[EXTRA="no"])
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
	AS_HELP_STRING([--disable-swig], [Disable automatic generation of source files needed for modperl/modpython.
			This value is ignored if perl and python are disabled. If you disable swig,
			but still want to use these modules, please download the necessary files to modules/modperl and/or to modules/modpython.
			Check http://en.znc.in/wiki/Modperl and http://en.znc.in/wiki/Modpython for details.]),
	[USESWIG="$enableval"],
	[USESWIG="auto"])
AC_ARG_ENABLE( [sasl],
	AS_HELP_STRING([--enable-sasl], [enable sasl]),
	[if test "$enableval" = "yes" ; then SASL=1; fi],)
AC_ARG_ENABLE( [optimization],
	AS_HELP_STRING([--disable-optimization], [Disable some compiler optimizations to
			decrease memory usage while compiling]),
	[OPTIMIZE="$enableval"],
	[OPTIMIZE="yes"])
AC_ARG_ENABLE( [c-ares],
	AS_HELP_STRING([--disable-c-ares], [disable c-ares usage]),
	[ARES="$enableval"],
	[ARES="auto"])
AC_ARG_ENABLE( [run-from-source],
	AS_HELP_STRING([--enable-run-from-source], [znc will be runnable without installation]),
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

if test "$DEBUG" != "no"; then
	appendCXX -ggdb
	AC_DEFINE([_DEBUG], [1], [Define for debugging])
	# These enable some debug options in g++'s STL, e.g. invalid use of iterators
	AC_DEFINE([_GLIBCXX_DEBUG], [1], [Enable extra debugging checks in libstdc++])
	AC_DEFINE([_GLIBCXX_DEBUG_PEDANTIC], [1], [Enable extra debugging checks in libstdc++])
	AC_DEFINE([_GLIBCXX_CONCEPT_CHECKS], [1], [Enable extra debugging checks in libstdc++])
else
	if test "x$OPTIMIZE" = "xyes"; then
		appendCXX -O2
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
AC_CHECK_FUNCS([lstat getopt_long getphassphrase])
PKG_PROG_PKG_CONFIG()

# ----- Check for dlopen

AC_SEARCH_LIBS([dlopen], [dl], [],
		[AC_MSG_ERROR([Could not find dlopen. ZNC will not work on this box until you upgrade this ancient system or at least install the necessary system libraries.])])

# ----- Check for c-ares

if test "x$ARES" != "xno"; then
	old_ARES=$ARES
	if test -z "$PKG_CONFIG"
	then
		ARES=no
		ZNC_AUTO_FAIL([ARES],
			[pkg-config not found. Try --disable-c-ares.
Disabling c-ares may result in a slight performance decrease but will not have any other side-effects],
			[pkg-config was not found and thus c-ares was disabled])
	else
		PKG_CHECK_MODULES([c_ares], [libcares >= 1.5.0],, ARES=no)

		if test "x$ARES" = "xno" ; then
			ZNC_AUTO_FAIL([ARES],
				[c-ares not found. Try --disable-c-ares.
Disabling c-ares may result in a slight performance decrease but will not have any other side-effects],
				[c-ares was not found and thus disabled])
		else
			AC_DEFINE([HAVE_C_ARES], [1], [Define if c-ares is enabled])
			appendCXX $c_ares_CFLAGS
			appendLib $c_ares_LIBS
			ARES=yes
		fi
	fi
fi

# ----- Check for openssl

if test "x$SSL" != "xno"; then
	if test -n "$OPENSSL"; then
		appendLib -L${OPENSSL}/lib
		appendLib -L${OPENSSL}/lib64
		appendCXX -I${OPENSSL}/include
		PKG_CONFIG_PATH="$OPENSSL/lib/pkgconfig/:$OPENSSL/lib64/pkgconfig/:$PKG_CONFIG_PATH"
	fi

	old_SSL=$SSL
	PKG_CHECK_MODULES([openssl], [openssl], [
		appendLib "$openssl_LIBS"
		appendCXX "$openssl_CFLAGS"
	], [
		# Don't reorder this!
		# On some arches libssl depends on libcrypto without linking to it :(
		AC_CHECK_LIB( crypto, BIO_new,, SSL=no )
		AC_CHECK_LIB( ssl, SSL_shutdown,, SSL=no )
	])

	if test "x$SSL" = "xno" ; then
		ZNC_AUTO_FAIL([SSL],
			[OpenSSL not found. Try --disable-openssl.],
			[OpenSSL was not found and thus disabled])
		NOSSL=1
	else
		AC_DEFINE([HAVE_LIBSSL], [1], [Define if openssl is enabled])
		SSL=yes
	fi
else
	NOSSL=1
fi

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
	# But cygwin does want most of znc in a shared lib
	LIBZNC="libznc.dll"
	LIBZNCDIR="$bindir"
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
		AC_PROG_SWIG([1.3.40])
		test -z "$SWIG" && USESWIG=no
	fi
	if test "x$USESWIG" = xno; then
		SWIG=""
		AC_MSG_WARN([$SWIG_ERROR])
		if test "x$old_USESWIG" = xno; then
			USESWIG="no, need manual download for perl/python bindings"
			AC_MSG_WARN([swig was disabled, so you should put needed sources to modules/
Please check http://en.znc.in/wiki/Modperl or http://en.znc.in/wiki/Modpython for details.])
		else
			AC_MSG_ERROR([swig was not found. Try --disable-perl --disable-python, or download the necessary sources and use --disable-swig.
Please check http://en.znc.in/wiki/Modperl or http://en.znc.in/wiki/Modpython for details.])
		fi
	else
		USESWIG=yes
	fi
else
	if test "x$USESWIG" = "xyes"; then
		AC_MSG_WARN([swig is needed only for perl and python, but both are disabled. Disabling swig.])
	fi
	USESWIG=no
fi

if test "x$PERL" != "xno"; then
	old_PERL="$PERL"
	AC_PATH_PROG([PERL_BINARY], [perl], [])
	if test -n "$PERL_BINARY" && eval "$PERL_BINARY -e'use 5.010'"; then
		my_saved_LDFLAGS="$LDFLAGS"
		appendLD `$PERL_BINARY -MExtUtils::Embed -e ccopts -e ldopts`
		AC_CHECK_LIB(perl, perl_alloc,
			     [: No, we do not want autoconf to do sth automatically],
			     PERL="no")
		LDFLAGS="$my_saved_LDFLAGS"
	else
		PERL="no"
	fi
	if test "x$PERL" = "xno"; then
		ZNC_AUTO_FAIL([PERL],
			[perl not found. Try --disable-perl.],
			[perl was not found and thus disabled])
		PERL_BINARY=""
	else
		PERL="yes"
	fi
fi

if test "x$PYTHON" != "xno"; then
	# Default value for just --enable-python
	if test "x$PYTHON" = "xyes"; then
		PYTHON="python3"
	fi
	old_PYTHON="$PYTHON"
	if test -z "$PKG_CONFIG"; then
		AC_MSG_ERROR([pkg-config is required for modpython.])
	fi
	PKG_CHECK_MODULES([python], [$PYTHON >= 3.0],, AC_MSG_ERROR([$PYTHON.pc not found or is wrong. Try --disable-python or install python3.]))
	if test "x$USESWIG" = xyes; then :
		PKG_CHECK_MODULES([python32], [$PYTHON >= 3.2], [PYTHON_32=yes], [PYTHON_32=no])
		if test "x$PYTHON_32" = xyes; then
			AC_MSG_NOTICE([python 3.2+ found, requiring newer swig.])
			AC_PROG_SWIG([2.0.4])
			if test -z "$SWIG"; then
				AC_MSG_ERROR([$SWIG_ERROR])
			fi
		fi
	fi
	my_saved_LIBS="$LIBS"
	my_saved_CXXFLAGS="$CXXFLAGS"
	appendLib $python_LIBS
	appendCXX $python_CFLAGS
	AC_CHECK_FUNC([Py_Initialize], [], [PYTHON="no"])
	if test "x$PYTHON" != "xno"; then
		# Yes, modpython depends on perl.
		AC_PATH_PROG([PERL_BINARY], [perl])
		if test -z "$PERL_BINARY"; then
			AC_MSG_ERROR([To compile modpython you need to be able to execute perl scripts. Try --disable-python or install perl.])
		fi
		LIBS="$my_saved_LIBS"
		CXXFLAGS="$my_saved_CXXFLAGS"
	else
		PYTHON="no"
	fi
	if test "x$PYTHON" = "xno"; then
		ZNC_AUTO_FAIL([PYTHON],
			[python not found. Try --disable-python.],
			[python was not found and thus disabled])
		PYTHONCFG_BINARY=""
	else
		PYTHON="yes"
	fi
fi

if test -n "$SASL"; then
	AC_CHECK_LIB( sasl2, sasl_server_init,
		[: Dont let autoconf add -lsasl2, Makefile handles that],
		AC_MSG_ERROR([could not find libsasl2. Try --disable-sasl.]))
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
	for path in $TCL_DIR /usr/lib /usr/lib/tcl8.4 /usr/lib/tcl8.5
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

if test x"$EXTRA" = "xyes"
then
	AC_CHECK_FUNC( iconv, CHARSET=1 )
	AC_CHECK_LIB( iconv, iconv_open, LIBICONV=-liconv ; CHARSET=1 )
	if test "x$CHARSET" = "x1"
	then
		AC_MSG_CHECKING([whether iconv expects a const second argument])
		AC_COMPILE_IFELSE([
			AC_LANG_PROGRAM([[
		#include <iconv.h>
		]], [[
			const char input[] = "X"; const char *inptr = input; size_t inbytes = 1;
			char buf[1]; char *outptr = buf; size_t outbytes = 1;
			iconv_t ic = iconv_open("ISO8859-1", "UTF8");
			iconv(ic, (char**)&inptr, &inbytes, &outptr, &outbytes);
		]])], [
			AC_MSG_RESULT([no])
			appendMod -DICONV_CONST=
		], [
			AC_MSG_RESULT([yes])
			appendMod -DICONV_CONST=const
		])
	else
		AC_MSG_WARN([libiconv was not found, disabling the charset module])
	fi
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
GNUMAKE_DIRNAME=`AS_DIRNAME(["$ac_cv_path_GNUMAKE"])`
GNUMAKE=`echo $ac_cv_path_GNUMAKE | $SED "s%$GNUMAKE_DIRNAME/%%"`

AC_SUBST([CXXFLAGS])
AC_SUBST([CPPFLAGS])
AC_SUBST([MODFLAGS])
AC_SUBST([LDFLAGS])
AC_SUBST([LIBS])
AC_SUBST([LIBZNC])
AC_SUBST([LIBZNCDIR])
AC_SUBST([MODLINK])
AC_SUBST([NOSSL])
AC_SUBST([EXTRA])
AC_SUBST([TCL_FLAGS])
AC_SUBST([SASL])
AC_SUBST([CHARSET])
AC_SUBST([LIBICONV])
AC_SUBST([MODDIR])
AC_SUBST([DATADIR])
AC_SUBST([PERL])
AC_SUBST([PYTHON])
AC_SUBST([SWIG])
AC_SUBST([python_CFLAGS])
AC_SUBST([python_LIBS])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([znc-config])
AC_CONFIG_FILES([znc-buildmod])
AC_CONFIG_FILES([man/Makefile])
AC_CONFIG_FILES([znc.pc])
AC_CONFIG_FILES([znc-uninstalled.pc])
AC_CONFIG_FILES([modules/Makefile])
AC_CONFIG_FILES([test/Makefile])
AC_OUTPUT

echo
echo znc AC_PACKAGE_VERSION configured
echo
echo "prefix:       $prefix"
echo "debug:        $DEBUG"
echo "ipv6:         $IPV6"
echo "openssl:      $SSL"
echo "c-ares:       $ARES"
echo "perl:         $PERL"
echo "python:       $PYTHON"
echo "swig:         $USESWIG"
if test x"$SASL" = "x" ; then
	echo "sasl:         no"
else
	echo "sasl:         yes"
fi
echo "extra:        $EXTRA"
if test x"$TCL_FLAGS" = "x" ; then
	echo "tcl:          no"
else
	echo "tcl:          yes"
fi
if test x"$CHARSET" = "x" ; then
	echo "charset:      no"
else
	echo "charset:      yes"
fi
echo "run from src: $RUNFROMSOURCE"
echo
echo "Now you can run \"$GNUMAKE\" to compile ZNC"

