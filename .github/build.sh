set -x

pwd
ls -la

cpanm --local-lib=~/perl5 local::lib
eval $(perl -I ~/perl5/lib/perl5/ -Mlocal::lib)
cpanm --notest Devel::Cover::Report::Clover
pip3 install --user coverage
export ZNC_MODPERL_COVERAGE=1"
export ZNC_MODPYTHON_COVERAGE=1"

case "$RUNNER_OS" in
	Linux)
		CXX_FOR_COV=$CXX
		;;
	macOS)
		CXX_FOR_COV=clang++
		;;
esac
case "$CXX_FOR_COV" in
	g++)
		CXXFLAGS+=" --coverage"
		LDFLAGS+=" --coverage"
		;;
	clang++)
		CXXFLAGS+=" -fprofile-instr-generate -fcoverage-mapping"
		LDFLAGS+=" -fprofile-instr-generate"
		;;
esac

mkdir build
cd build
../configure --enable-debug --enable-perl --enable-python --enable-tcl --enable-cyrus --enable-charset $CFGFLAGS
cmake --system-information

make -j2 VERBOSE=1
env LLVM_PROFILE_FILE="$PWD/unittest.profraw" make VERBOSE=1 unittest
sudo make install
/usr/local/bin/znc --version

# TODO: use DEVEL_COVER_OPTIONS for https://metacpan.org/pod/Devel::Cover
env LLVM_PROFILE_FILE="$PWD/inttest.profraw" ZNC_MODPERL_COVERAGE_OPTS="-db,$PWD/cover_db" PYTHONWARNINGS=error make VERBOSE=1 inttest

case "$RUNNER_OS" in
	Linux)
		~/perl5/bin/cover --no-gcov --report=clover
		;;
	macOS)
		xcrun llvm-profdata merge unittest.profraw -o unittest.profdata
		xcrun llvm-profdata merge inttest.profraw -o inttest.profdata
		xcrun llvm-cov show -show-line-counts-or-regions -instr-profile=unittest.profdata test/unittest_bin > unittest-cmake-coverage.txt
		xcrun llvm-cov show -show-line-counts-or-regions -instr-profile=inttest.profdata /usr/local/bin/znc > inttest-znc-coverage.txt
		find /usr/local/lib/znc -name '*.so' -or -name '*.bundle' | while read f; do xcrun llvm-cov show -show-line-counts-or-regions -instr-profile=inttest.profdata $f > inttest-$(basename $f)-coverage.txt; done
		;;
esac
