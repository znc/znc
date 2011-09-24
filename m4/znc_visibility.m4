# visibility.m4 serial 3 (gettext-0.18)
dnl Copyright (C) 2005, 2008-2010 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

dnl Changes done by Uli Schlachter (C) 2011:
dnl - Renamed everything from gl_ to znc_
dnl - Instead of using CFLAGS, this now uses CXXFLAGS (the macro would actually
dnl   silently break if you called AC_LANG with anything but C before it)
dnl - Because of the above, this now requiers AC_PROG_CXX and $GXX instead
dnl   of $GCC
dnl - Added calls to AC_LANG_PUSH([C++]) and AC_LANG_POP
dnl - Replaced AC_TRY_COMPILE with AC_COMPILE_IFELSE
dnl - Added a definition for dummyfunc() so that this works with
dnl   -Wmissing-declarations

dnl Tests whether the compiler supports the command-line option
dnl -fvisibility=hidden and the function and variable attributes
dnl __attribute__((__visibility__("hidden"))) and
dnl __attribute__((__visibility__("default"))).
dnl Does *not* test for __visibility__("protected") - which has tricky
dnl semantics (see the 'vismain' test in glibc) and does not exist e.g. on
dnl MacOS X.
dnl Does *not* test for __visibility__("internal") - which has processor
dnl dependent semantics.
dnl Does *not* test for #pragma GCC visibility push(hidden) - which is
dnl "really only recommended for legacy code".
dnl Set the variable CFLAG_VISIBILITY.
dnl Defines and sets the variable HAVE_VISIBILITY.

AC_DEFUN([ZNC_VISIBILITY],
[
  AC_REQUIRE([AC_PROG_CXX])
  AC_LANG_PUSH([C++])
  CFLAG_VISIBILITY=
  HAVE_VISIBILITY=0
  if test -n "$GXX"; then
    dnl First, check whether -Werror can be added to the command line, or
    dnl whether it leads to an error because of some other option that the
    dnl user has put into $CC $CFLAGS $CPPFLAGS.
    AC_MSG_CHECKING([whether the -Werror option is usable])
    AC_CACHE_VAL([znc_cv_cc_vis_werror], [
      znc_save_CXXFLAGS="$CXXFLAGS"
      CXXFLAGS="$CXXFLAGS -Werror"
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
        [znc_cv_cc_vis_werror=yes],
        [znc_cv_cc_vis_werror=no])
      CXXFLAGS="$znc_save_CXXFLAGS"])
    AC_MSG_RESULT([$znc_cv_cc_vis_werror])
    dnl Now check whether visibility declarations are supported.
    AC_MSG_CHECKING([for simple visibility declarations])
    AC_CACHE_VAL([znc_cv_cc_visibility], [
      znc_save_CXXFLAGS="$CXXFLAGS"
      CXXFLAGS="$CXXFLAGS -fvisibility=hidden"
      dnl We use the option -Werror and a function dummyfunc, because on some
      dnl platforms (Cygwin 1.7) the use of -fvisibility triggers a warning
      dnl "visibility attribute not supported in this configuration; ignored"
      dnl at the first function definition in every compilation unit, and we
      dnl don't want to use the option in this case.
      if test $znc_cv_cc_vis_werror = yes; then
        CXXFLAGS="$CXXFLAGS -Werror"
      fi
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
       [[extern __attribute__((__visibility__("hidden"))) int hiddenvar;
         extern __attribute__((__visibility__("default"))) int exportedvar;
         extern __attribute__((__visibility__("hidden"))) int hiddenfunc (void);
         extern __attribute__((__visibility__("default"))) int exportedfunc (void);
         void dummyfunc (void);
         void dummyfunc (void) {}]],
        [])],
        [znc_cv_cc_visibility=yes],
        [znc_cv_cc_visibility=no])
      CXXFLAGS="$znc_save_CXXFLAGS"])
    AC_MSG_RESULT([$znc_cv_cc_visibility])
    if test $znc_cv_cc_visibility = yes; then
      CFLAG_VISIBILITY="-fvisibility=hidden"
      HAVE_VISIBILITY=1
    fi
  fi
  AC_SUBST([CFLAG_VISIBILITY])
  AC_SUBST([HAVE_VISIBILITY])
  AC_DEFINE_UNQUOTED([HAVE_VISIBILITY], [$HAVE_VISIBILITY],
    [Define to 1 or 0, depending whether the compiler supports simple visibility declarations.])
  AC_LANG_POP
])
