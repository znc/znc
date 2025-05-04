#!groovy

timestamps {
  node('freebsd') {
    // AWS EC2 AMI: FreeBSD 14.1-STABLE-amd64-20241003 UEFI-PREFERRED base UFS
    // pkg install git openjdk22 cmake icu pkgconf swig python3 boost-libs gettext-tools qt6-base libargon2
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
          sh "cmake $wsdir -DWANT_PERL=ON -DWANT_PYTHON=ON -DWANT_ARGON=ON -DCMAKE_INSTALL_PREFIX=$wsdir/build/install-prefix"
          sh 'make VERBOSE=1 all'
        }
        stage('Unit test') {
          withEnv(['GTEST_OUTPUT=xml:unit-test.xml']) {
            sh 'make unittest'
          }
        }
        stage('Integration test') {
          withEnv(['GTEST_OUTPUT=xml:integration-XXX-test.xml']) {
            sh 'make install'
            sh 'make inttest'
          }
        }
        junit '**/*test.xml'
      }
    }
  }
}
