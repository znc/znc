#!/bin/sh
# Run this to generate all the initial makefiles, etc.
# This is based on various examples which can be found everywhere.
set -e

FLAGS=${FLAGS--Wall}
ACLOCAL=${ACLOCAL-aclocal}
AUTOHEADER=${AUTOHEADER-autoheader}
AUTOCONF=${AUTOCONF-autoconf}
AUTOMAKE=${AUTOMAKE-automake}
ACLOCAL_FLAGS="${ACLOCAL_FLAGS} ${FLAGS}"
AUTOHEADER_FLAGS="${AUTOHEADER_FLAGS} ${FLAGS}"
AUTOCONF_FLAGS="${AUTOCONF_FLAGS} ${FLAGS}"
AUTOMAKE_FLAGS="${AUTOMAKE_FLAGS---add-missing} ${FLAGS}"

# Allow invocation from a separate build directory
srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd "$srcdir"

die() {
	echo "$@"
	exit 1
}
do_cmd() {
	echo "Running '$@'"
	$@
}

test -f configure.ac || die "No $srcdir/configure.ac found."

# Generate aclocal.m4 for use by autoconf
do_cmd $ACLOCAL    $ACLOCAL_FLAGS
# Generate zncconfig.h.in for configure
do_cmd $AUTOHEADER $AUTOHEADER_FLAGS
# Generate configure
do_cmd $AUTOCONF   $AUTOCONF_FLAGS
# Copy config.sub, config.guess, install.sh, ...
# This will complain that we don't use automake, let's just ignore that
do_cmd $AUTOMAKE   $AUTOMAKE_FLAGS || true
test -f config.guess -a -f config.sub -a -f install-sh ||
	die "Automake didn't install config.guess, config.sub and install-sh!"

cd "$ORIGDIR" || exit 1

if test -z "$NOCONFIGURE"
then
	do_cmd "$srcdir/configure" --cache-file=config.cache ${1+"$@"} || exit 1
fi
