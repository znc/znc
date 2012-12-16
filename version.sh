#!/bin/sh

# Change into the source directory
cd `dirname $0`

# Our first argument should be the path to git
GIT="$1"
if [ "x$GIT" = "x" ]
then
	# Let's hope the best
	GIT=git
fi

# Figure out the information we need
LATEST_TAG=`${GIT} describe --abbrev=0 HEAD`
COMMITS_SINCE=`${GIT} log --format=oneline ${LATEST_TAG}..HEAD | wc -l`
SHORT_ID=`${GIT} rev-parse --short HEAD`

# If this commit is tagged, don't print anything
# (the assumption here is: this is a release)
if [ "x$COMMITS_SINCE" = "x0" ]
then
	exit 0
fi
echo "-DVERSION_EXTRA=\\\"-git-${COMMITS_SINCE}-${SHORT_ID}\\\""
