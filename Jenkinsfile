#!groovy

timestamps {
  node('freebsd') {
    // freebsd 13.1 + pkg install git openjdk17 cmake icu pkgconf swig python3 boost-libs gettext-tools qt5-buildtools qt5-network qt5-qmake
    // Then fill known_hosts with https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/githubs-ssh-key-fingerprints
    // (needed for crowdin cron job in jenkins)
    timeout(time: 30, unit: 'MINUTES') {
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
