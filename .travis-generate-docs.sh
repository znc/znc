#!/bin/bash -x

echo "Generating docs with doxygen..."

doxygen

mkdir -p ~/.ssh
chmod 0600 ~/znc-docs-key
cat <<EOF >> ~/.ssh/config
Host znc-docs
HostName github.com
User git
IdentityFile ~/znc-docs-key
StrictHostKeyChecking no
UserKnownHostsFile /dev/null
EOF

cd "$HOME"
git config --global user.email "travis-ci@znc.in"
git config --global user.name "znc-travis"
git clone --branch=gh-pages znc-docs:znc/docs.git gh-pages || exit 1

cd gh-pages
git rm -rf .

cp -rf "$TRAVIS_BUILD_DIR"/doc/html/* ./ || exit 1
echo docs.znc.in > CNAME

git add .
git commit -F- <<EOF
Latest docs on successful travis build $TRAVIS_BUILD_NUMBER

ZNC commit $TRAVIS_COMMIT
EOF
git push origin gh-pages

echo "Published docs to gh-pages."
