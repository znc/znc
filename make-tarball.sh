#!/bin/sh
set -e

TMPDIR=`mktemp -d`
TMPDIR2=`mktemp -d`
trap 'rm -rf $TMPDIR $TMPDIR2' EXIT

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
	NIGHTLY=1
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
	DESC="$(sed -En 's/set\(alpha_version "(.*)"\).*/\1/p' CMakeLists.txt)"
	NIGHTLY=0
fi

TARGZ=`readlink -f -- $TARGZ`

echo "Exporting . to $TMPDIR/$ZNCDIR..."
git checkout-index --all --prefix=$TMPDIR/$ZNCDIR/
mkdir -p --mode=0755 $TMPDIR/$ZNCDIR/third_party/Csocket
cp -p third_party/Csocket/Csocket.cc third_party/Csocket/Csocket.h $TMPDIR/$ZNCDIR/third_party/Csocket/
mkdir -p --mode=0755 $TMPDIR/$ZNCDIR/third_party/cctz
cp -Rp third_party/cctz/src third_party/cctz/include third_party/cctz/LICENSE.txt $TMPDIR/$ZNCDIR/third_party/cctz/
mkdir -p --mode=0755 $TMPDIR/$ZNCDIR/third_party/gtest-parallel
cp -p third_party/gtest-parallel/LICENSE third_party/gtest-parallel/gtest-parallel third_party/gtest-parallel/gtest_parallel.py $TMPDIR/$ZNCDIR/third_party/gtest-parallel/
(
	cd $TMPDIR2
	cmake $TMPDIR/$ZNCDIR -DWANT_PERL=yes -DWANT_PYTHON=yes
	make modperl_dist modpython_dist
)
(
	cd $TMPDIR/$ZNCDIR
	rm -rf .travis* .appveyor* .ci/ .github/
	rm make-tarball.sh
	if [ "x$DESC" != "x" ]; then
		if [ $NIGHTLY = 1 ]; then
			echo $DESC > .nightly
		fi
	fi
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
