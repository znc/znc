#!/bin/bash -x

echo "Generating docs with doxygen..."

doxygen

cd "$HOME"
git config --global user.email "travis@travis-ci.org"
git config --global user.name "travis-ci"
git clone --branch=gh-pages https://${GH_TOKEN}@github.com/znc/znc gh-pages || exit 1

cd gh-pages
git rm -rf *
cp -rf "$TRAVIS_BUILD_DIR"/doc/html/* ./ || exit 1
git add .
git commit -m "Latest docs on successful travis build $TRAVIS_BUILD_NUMBER (commit $TRAVIS_COMMIT) auto-pushed to gh-pages"
git push origin gh-pages

echo "Published docs to gh-pages."
