#!/bin/sh
set -e

printUsage()
{
	echo "Usage: ./make-tarball.sh (--nightly) (--output <path>)"
}

TMPDIR=`mktemp -d`
trap 'rm -rf $TMPDIR' EXIT

SRCDIR=`cd $(dirname $0) && pwd`

if [ "x$1" = "x--nightly" ]; then
	SIGN=0
	DATE=`date +%Y-%m-%d`
	SHA1=`git --git-dir $SRCDIR/.git rev-parse --short HEAD`
	DEFINES="-DZNC_VERSION_EXTRA=$DATE-$SHA1"
	shift
else
	SIGN=1
	DEFINES="-DFEATURE_GIT=OFF"
fi

if [ "x$1" != "x" ] && [ "x$1" != "x--output" ]; then
	printUsage
fi

OUTPUT=$2
EXT=`echo $OUTPUT | awk -F . '{print $NF}'`

if [ "x$EXT" == "xgz" ]; then
	printUsage
	OUTPUT=`dirname $OUTPUT`
fi

cd $TMPDIR && cmake $SRCDIR $DEFINES -DCPACK_OUTPUT_FILE_PREFIX=$OUTPUT

make package_source
if [ $SIGN = 1 ]; then
	make package_sign
fi
