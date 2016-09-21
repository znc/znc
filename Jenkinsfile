#!groovy

node('freebsd') {
  // freebsd 10.3 + pkg install git openjdk cmake icu pkgconf swig30 python3 boost-libs gettext-tools qt5-buildtools qt5-network qt5-qmake
  timestamps {
    timeout(time: 30, unit: 'MINUTES') {
      wrap([$class: 'AnsiColorBuildWrapper', colorMapName: 'xterm']) {
        def wsdir = pwd()
        stage('Checkout') {
          step([$class: 'WsCleanup'])
          checkout scm
          sh 'git submodule update --init --recursive'
        }
        dir("$wsdir/build") {
          stage('Build') {
            sh "cmake $wsdir -DWANT_PERL=ON -DWANT_PYTHON=ON -DCMAKE_INSTALL_PREFIX=$wsdir/build/install-prefix"
            sh 'make VERBOSE=1 all'
          }
          stage('Unit test') {
            withEnv(['GTEST_OUTPUT=xml:unit-test.xml']) {
              sh 'make unittest'
            }
          }
          stage('Integration test') {
            withEnv(['GTEST_OUTPUT=xml:integration-test.xml']) {
              sh 'make install'
              sh 'make inttest'
            }
          }
          junit '**/*test.xml'
        }
      }
    }
  }
}
