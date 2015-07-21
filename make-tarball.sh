#!/bin/sh
set -e

TMPDIR=`mktemp -d`
trap 'rm -rf $TMPDIR' EXIT

if [ ! -f include/znc/main.h ] ; then
	echo "Can't find source!"
	exit -1
fi

if [ "x$1" = "x--nightly" ]; then
	# e.g. ./make-tarball.sh --nightly znc-git-2014-04-21 path/to/output/znc-foo.tar.gz
	ZNCDIR=$2
	TARGZ=$3
	SIGN=0
	DESC=-nightly-`date +%Y%m%d`-`git $GITDIR rev-parse HEAD | cut -b-8`
else
	VERSION=$1
	if [ "x$VERSION" = "x" ] ; then
		AWK_ARG='/#define VERSION_MAJOR/ { maj = $3 }
			/#define VERSION_MINOR/ { min = $3 }
			END { printf "%.1f", (maj + min / 10) }'
		VERSION=$(awk "$AWK_ARG" include/znc/main.h)
	fi
	if [ "x$VERSION" = "x" ] ; then
		echo "Couldn't get version number"
		exit -1
	fi

	ZNCDIR=znc-$VERSION
	TARGZ=$ZNCDIR.tar.gz
	SIGN=1
	DESC=""
	# DESC="-rc1"
fi

TARGZ=`realpath $TARGZ`

echo "Exporting . to $TMPDIR/$ZNCDIR..."
git checkout-index --all --prefix=$TMPDIR/$ZNCDIR/
sed -e 's:#include "Csocket.h":#include <znc/Csocket.h>:' third_party/Csocket/Csocket.cc > $TMPDIR/$ZNCDIR/src/Csocket.cpp
sed -e 's:#include "defines.h":#include <znc/defines.h>:' third_party/Csocket/Csocket.h > $TMPDIR/$ZNCDIR/include/znc/Csocket.h
(
	cd $TMPDIR/$ZNCDIR
	echo "Generating configure"
	AUTOMAKE_FLAGS="--add-missing --copy" ./autogen.sh
	rm -r autom4te.cache/
	mkdir -p modules/.depend
	make -C modules -f modperl/Makefile.gen srcdir=. SWIG=`which swig` PERL=`which perl`
	make -C modules -f modpython/Makefile.gen srcdir=. SWIG=`which swig` PERL=`which perl`
	rm -rf modules/.depend
	rm .travis*
	rm make-tarball.sh
	sed -e "s/THIS_IS_NOT_TARBALL//" -i Makefile.in
	echo '#include <znc/version.h>' > src/version.cpp
	echo "const char* ZNC_VERSION_EXTRA = VERSION_EXTRA \"$DESC\";" >> src/version.cpp
)
(
	cd $TMPDIR
	echo "Creating tarball"
	env GZIP=-9 tar -czf $TARGZ $ZNCDIR
)
echo "Done"

if [ $SIGN = 1 ]; then
	echo "Signing $TARGZ..."
	gpg --detach-sig $TARGZ
	echo "Created $TARGZ and $TARGZ.sig"
fi
