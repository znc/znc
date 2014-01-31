#!/bin/sh
set -e

TMPDIR=`mktemp -d`
VERSION=$1

trap 'rm -rf $TMPDIR' EXIT

if [ ! -f include/znc/main.h ] ; then
    echo "Can't find source!"
	exit -1
fi

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

ZNC=znc-$VERSION
TAR=$ZNC.tar
TARGZ=$TAR.gz

echo "Exporting . to $TMPDIR/$ZNC..."
git checkout-index --all --prefix=$TMPDIR/$ZNC/
(
	cd $TMPDIR/$ZNC
	echo "Generating configure"
	AUTOMAKE_FLAGS="--add-missing --copy" ./autogen.sh
	rm -r autom4te.cache/
    mkdir -p modules/.depend
    make -C modules -f modperl/Makefile.gen srcdir=. SWIG=/usr/bin/swig
    make -C modules -f modpython/Makefile.gen srcdir=. SWIG=/usr/bin/swig
    rm -fr modules/.depend
	rm make-tarball.sh
	sed -e "s/THIS_IS_NOT_NIGHTLY//" -i Makefile.in
	echo '#include <znc/version.h>' > src/version.cpp
	echo "const char* ZNC_VERSION_EXTRA = \"-rc1\";" >> src/version.cpp
	# "rc1" part is to be fixed later
)
(
	cd $TMPDIR
	echo "Creating tarball"
	tar -cf $TAR $ZNC
	gzip -9 $TAR
)
echo "Done"
mv $TMPDIR/$TARGZ .
echo "Signing $TARGZ..."
gpg --detach-sig $TARGZ
echo "Created $TARGZ and $TARGZ.sig"
