#!groovy

node('freebsd') {
  // freebsd 10.3 + pkg install git openjdk cmake icu pkgconf swig30 python3 boost-libs gettext-tools qt5-buildtools qt5-network qt5-qmake
  timestamps {
    timeout(time: 30, unit: 'MINUTES') {
      def srcdir = pwd()
      def tmpdir = pwd([tmp: true])
      stage('Checkout') {
        step([$class: 'WsCleanup'])
        checkout scm
        sh 'git submodule update --init --recursive'
      }
      dir("$tmpdir/build") {
        stage('Build') {
          sh "cmake $srcdir -DWANT_PERL=ON -DWANT_PYTHON=ON -DCMAKE_INSTALL_PREFIX=$tmpdir/install-prefix"
          sh 'make VERBOSE=1 all'
        }
        stage('Unit test') {
          sh 'make unittest'
        }
        stage('Integration test') {
          sh 'make install'
          sh 'make inttest'
        }
      }
    }
  }
}
