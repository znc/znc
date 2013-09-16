#!/bin/sh

# When changing this file, remember also about nightlies

# Get the path to the source directory
GIT_DIR=`dirname $0`

# Our argument should be the path to git
GIT="$1"
if [ "x$GIT" = "x" ]
then
	EXTRA=""
else
	GIT="${GIT} --git-dir=${GIT_DIR}/.git"

	# Figure out the information we need
	LATEST_TAG=`${GIT} describe --abbrev=0 HEAD`
	COMMITS_SINCE=`${GIT} log --format=oneline ${LATEST_TAG}..HEAD | wc -l`
	# Explicitly make it a number: on openbsd wc -l returns "    42" instead of "42"
	let COMMITS_SINCE=COMMITS_SINCE
	SHORT_ID=`${GIT} rev-parse --short HEAD`

	if [ "x$COMMITS_SINCE" = "x0" ]
	then
		if [ "x$LATEST_TAG" = "x" ]
		then
			if [ "x$SHORT_ID" = "x" ]
			then
				EXTRA=""
			else
				EXTRA="-git-${SHORT_ID}"
			fi
		else
			# If this commit is tagged, don't print anything
			# (the assumption here is: this is a release)
			EXTRA=""
		fi
	else
		EXTRA="-git-${COMMITS_SINCE}-${SHORT_ID}"
	fi
fi

# Generate output file, if any
if [ "x$WRITE_OUTPUT" = "xyes" ]
then
	echo '#include <znc/version.h>' > src/version.cpp
	echo "const char* ZNC_VERSION_EXTRA = \"$EXTRA\";" >> src/version.cpp
fi

echo "$EXTRA"
