#!groovy

node('freebsd') {
  // freebsd 10.3 + pkg install git openjdk cmake icu pkgconf swig30 python3
  timestamps {
    update_github_status()
    try {
      timeout(time: 30, unit: 'MINUTES') {
        def srcdir = pwd()
        def tmpdir = pwd([tmp: true])
        stage('Checkout') {
          sh 'env'
          checkout scm
          sh 'git submodule update --init --recursive'
        }
        stage('Build') {
          dir("$tmpdir/build") {
            sh "cmake $srcdir -DWANT_PERL=ON -DWANT_PYTHON=ON -DCMAKE_INSTALL_PREFIX=$tmpdir/install-prefix"
            sh 'make VERBOSE=1 all'
          }
        }
        stage('Unit test') {
          dir("$tmpdir/build") {
            sh 'make unittest'
          }
        }
        stage('Integration test') {
          dir("$tmpdir/build") {
            sh 'make install'
            // TODO: run make inttest
          }
        }
      }
      currentBuild.result = 'SUCCESS'
    } catch (err) {
      echo "Error: ${err}"
      currentBuild.result = 'FAILURE'
    }
    update_github_status()
  }
}

def update_github_status() {
  step([$class: 'GitHubCommitStatusSetter',
        contextSource: [$class: 'ManuallyEnteredCommitContextSource',
                        context: 'continuous-integration/jenkins']])
}
