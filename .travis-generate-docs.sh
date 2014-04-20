#!/bin/bash -x

echo "Generating docs with doxygen..."

doxygen

mkdir -p ~/.ssh
cat <<EOF >> ~/.ssh/config
Host znc-docs
HostName github.com
User git
IdentityFile ~/znc-docs-key
EOF

cd "$HOME"
git config --global user.email "travis@travis-ci.org"
git config --global user.name "travis-ci"
git clone --branch=gh-pages znc-docs:znc/docs.git gh-pages || exit 1

cd gh-pages
git rm -rf *
cp -rf "$TRAVIS_BUILD_DIR"/doc/html/* ./ || exit 1
git add .
git commit -m "Latest docs on successful travis build $TRAVIS_BUILD_NUMBER (commit $TRAVIS_COMMIT) auto-pushed to gh-pages"
git push origin gh-pages

echo "Published docs to gh-pages."
