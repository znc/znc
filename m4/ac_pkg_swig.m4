dnl @synopsis AC_PROG_SWIG([major.minor.micro])
dnl
dnl NOTICE: for new code, use http://www.gnu.org/s/autoconf-archive/ax_pkg_swig.html instead.
dnl
dnl This macro searches for a SWIG installation on your system. If
dnl found you should call SWIG via $(SWIG). You can use the optional
dnl first argument to check if the version of the available SWIG is
dnl greater than or equal to the value of the argument. It should have
dnl the format: N[.N[.N]] (N is a number between 0 and 999. Only the
dnl first N is mandatory.)
dnl
dnl If the version argument is given (e.g. 1.3.17), AC_PROG_SWIG checks
dnl that the swig package is this version number or higher.
dnl
dnl In configure.in, use as:
dnl
dnl	     AC_PROG_SWIG(1.3.17)
dnl	     SWIG_ENABLE_CXX
dnl	     SWIG_MULTI_MODULE_SUPPORT
dnl	     SWIG_PYTHON
dnl
dnl @category InstalledPackages
dnl @author Sebastian Huber <sebastian-huber@web.de>
dnl @author Alan W. Irwin <irwin@beluga.phys.uvic.ca>
dnl @author Rafael Laboissiere <rafael@laboissiere.net>
dnl @author Andrew Collier <abcollier@yahoo.com>
dnl @version 2004-09-20
dnl
dnl Modified by Alexey Sokolov <alexey@asokolov.org> on 2012-08-08
dnl @license GPLWithACException
dnl
dnl NOTICE: for new code, use http://www.gnu.org/s/autoconf-archive/ax_pkg_swig.html instead.

AC_DEFUN([AC_PROG_SWIG],[
	SWIG_ERROR=""

	if test -n "$1"; then
		# Calculate the required version number components
		[required=$1]
		[required_major=`echo $required | sed 's/[^0-9].*//'`]
		if test -z "$required_major" ; then
			[required_major=0]
		fi
		[required=`echo $required | sed 's/[0-9]*[^0-9]//'`]
		[required_minor=`echo $required | sed 's/[^0-9].*//'`]
		if test -z "$required_minor" ; then
			[required_minor=0]
		fi
		[required=`echo $required | sed 's/[0-9]*[^0-9]//'`]
		[required_patch=`echo $required | sed 's/[^0-9].*//'`]
		if test -z "$required_patch" ; then
			[required_patch=0]
		fi
	fi

	# for "python 3 abc set" and "PyInt_FromSize_t in python3" checks

	cat <<-END > conftest-python.i
		%module conftest;
		%include <pyabc.i>
		%include <std_set.i>
		%template(SInt) std::set<int>;
	END

	# check if perl has std::...::size_type defined. Don't add new tests to this .i; it'll break this test due to check for "NewPointerObj(("

	cat <<-END > conftest-perl.i
		%module conftest;
		%include <std_vector.i>
		%include <std_list.i>
		%include <std_deque.i>
		%template() std::vector<int>;
		%template() std::list<int>;
		%template() std::deque<int>;
		std::vector<int>::size_type checkVector();
		std::list<int>::size_type checkList();
		std::deque<int>::size_type checkDeque();
	END
	SWIG_installed_versions=""
	AC_CACHE_CHECK([for SWIG >= $1], [znc_cv_path_SWIG], [
		AC_PATH_PROGS_FEATURE_CHECK([SWIG], [swig swig2.0 swig3.0], [
			echo trying $ac_path_SWIG >&AS_MESSAGE_LOG_FD
			$ac_path_SWIG -version >&AS_MESSAGE_LOG_FD
			[swig_version=`$ac_path_SWIG -version 2>&1 | grep 'SWIG Version' | sed 's/.*\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/g'`]
			if test -n "$swig_version"; then
				swig_right_version=1

				SWIG_installed_versions="$SWIG_installed_versions $swig_version "

				if test -n "$required"; then
					# Calculate the available version number components
					[available=$swig_version]

					[available_major=`echo $available | sed 's/[^0-9].*//'`]
					if test -z "$available_major" ; then
						[available_major=0]
					fi
					[available=`echo $available | sed 's/[0-9]*[^0-9]//'`]
					[available_minor=`echo $available | sed 's/[^0-9].*//'`]
					if test -z "$available_minor" ; then
						[available_minor=0]
					fi
					[available=`echo $available | sed 's/[0-9]*[^0-9]//'`]
					[available_patch=`echo $available | sed 's/[^0-9].*//'`]
					if test -z "$available_patch" ; then
						[available_patch=0]
					fi

					if test $available_major -lt $required_major; then
						swig_right_version=0
					elif test $available_major -eq $required_major; then
						if test $available_minor -lt $required_minor; then
							swig_right_version=0
						elif test $available_minor -eq $required_minor; then
							if test $available_patch -lt $required_patch; then
								swig_right_version=0
							fi
						fi
					fi
				fi
				
				if test $swig_right_version -eq 1; then
					# "python 3 abc set", "PyInt_FromSize_t in python3" and "perl size_type" checks
					echo "checking behavior of this SWIG" >&AS_MESSAGE_LOG_FD

					$ac_path_SWIG -python -py3 -c++ -shadow conftest-python.i >&AS_MESSAGE_LOG_FD && \
						echo "python wrapper created" >&AS_MESSAGE_LOG_FD && \
						echo "testing std::set... ">&AS_MESSAGE_LOG_FD && \
						grep SInt_discard conftest.py > /dev/null 2>&1 && \
						echo "std::set works" >&AS_MESSAGE_LOG_FD && \
						echo "testing PyInt_FromSize_t..." >&AS_MESSAGE_LOG_FD && \
						grep '#define PyInt_FromSize_t' conftest-python_wrap.cxx > /dev/null 2>&1 && \
						echo "PyInt_FromSize_t is defined" >&AS_MESSAGE_LOG_FD && \
					$ac_path_SWIG -perl -c++ -shadow conftest-perl.i >&AS_MESSAGE_LOG_FD && \
						echo "perl wrapper created" >&AS_MESSAGE_LOG_FD && \
						echo "testing size_type..." >&AS_MESSAGE_LOG_FD && \
						test 0 -eq `grep -c 'NewPointerObj((' conftest-perl_wrap.cxx` && \
						echo "size_type work" >&AS_MESSAGE_LOG_FD && \
					znc_cv_path_SWIG=$ac_path_SWIG \
						ac_path_SWIG_found=:
					if test "x$ac_path_SWIG_found" != "x:"; then
						echo "fail" >&AS_MESSAGE_LOG_FD
					fi
					rm -f conftest-python_wrap.cxx conftest.py
					rm -f conftest-perl_wrap.cxx conftest.pm


				else
					echo "SWIG version >= $1 is required.  You have '$swig_version'" >&AS_MESSAGE_LOG_FD
				fi
			fi
			echo end trying $ac_path_SWIG >&AS_MESSAGE_LOG_FD
		])
	])
	rm -f conftest-python.i conftest-perl.i
	if test -n "$SWIG_installed_versions"; then
		AC_MSG_NOTICE([Following SWIG versions are found:$SWIG_installed_versions])
	fi

	AC_SUBST([SWIG], [$znc_cv_path_SWIG])
	if test -n "$SWIG"; then
		SWIG_LIB=`$SWIG -swiglib`
	fi
	AC_SUBST([SWIG_LIB])
])

# SWIG_ENABLE_CXX()
#
# Enable SWIG C++ support.  This affects all invocations of $(SWIG).
AC_DEFUN([SWIG_ENABLE_CXX],[
    AC_REQUIRE([AC_PROG_SWIG])
    AC_REQUIRE([AC_PROG_CXX])
    SWIG="$SWIG -c++"
])

# SWIG_MULTI_MODULE_SUPPORT()
#
# Enable support for multiple modules.  This effects all invocations
# of $(SWIG).  You have to link all generated modules against the
# appropriate SWIG runtime library.  If you want to build Python
# modules for example, use the SWIG_PYTHON() macro and link the
# modules against $(SWIG_PYTHON_LIBS).
#
AC_DEFUN([SWIG_MULTI_MODULE_SUPPORT],[
    AC_REQUIRE([AC_PROG_SWIG])
    SWIG="$SWIG -noruntime"
])

# SWIG_PYTHON([use-shadow-classes = {no, yes}])
#
# Checks for Python and provides the $(SWIG_PYTHON_CPPFLAGS),
# and $(SWIG_PYTHON_OPT) output variables.
#
# $(SWIG_PYTHON_OPT) contains all necessary SWIG options to generate
# code for Python.  Shadow classes are enabled unless the value of the
# optional first argument is exactly 'no'.  If you need multi module
# support (provided by the SWIG_MULTI_MODULE_SUPPORT() macro) use
# $(SWIG_PYTHON_LIBS) to link against the appropriate library.  It
# contains the SWIG Python runtime library that is needed by the type
# check system for example.
AC_DEFUN([SWIG_PYTHON],[
    AC_REQUIRE([AC_PROG_SWIG])
    AC_REQUIRE([AC_PYTHON_DEVEL])
    test "x$1" != "xno" || swig_shadow=" -noproxy"
    AC_SUBST([SWIG_PYTHON_OPT],[-python$swig_shadow])
    AC_SUBST([SWIG_PYTHON_CPPFLAGS],[$PYTHON_CPPFLAGS])
])



