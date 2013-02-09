#!/bin/sh

# Get the path to the source directory
GIT_DIR=`dirname $0`

# Our first argument should be the file that we write to
OUTPUT="$1"

# Our second argument should be the path to git
GIT="$2"
if [ "x$GIT" = "x" ]
then
	EXTRA=""
else
	GIT="${GIT} --git-dir=${GIT_DIR}/.git"

	# Figure out the information we need
	LATEST_TAG=`${GIT} describe --abbrev=0 HEAD`
	COMMITS_SINCE=`${GIT} log --format=oneline ${LATEST_TAG}..HEAD | wc -l`
	SHORT_ID=`${GIT} rev-parse --short HEAD`

	# If this commit is tagged, don't print anything
	# (the assumption here is: this is a release)
	if [ "x$COMMITS_SINCE" = "x0" ]
	then
		EXTRA=""
	else
		EXTRA="-git-${COMMITS_SINCE}-${SHORT_ID}"
	fi
fi

# Generate output file, if any
if [ "x$OUTPUT" != "x" ]
then
	echo '#include <znc/version.h>' > $OUTPUT
	echo "const char* ZNC_VERSION_EXTRA = \"$EXTRA\";" >> $OUTPUT
fi

echo "$EXTRA"
