set -x

pwd
ls -la

cpanm --local-lib=~/perl5 local::lib
eval $(perl -I ~/perl5/lib/perl5/ -Mlocal::lib)
cpanm --notest Devel::Cover::Report::Clover
pip3 install --user coverage
export ZNC_MODPERL_COVERAGE=1
#export ZNC_MODPYTHON_COVERAGE=1

case "${CC:-gcc}" in
	gcc)
		export CXXFLAGS="$CXXFLAGS --coverage"
		export LDFLAGS="$LDFLAGS --coverage"
		;;
	clang)
		export CXXFLAGS="$CXXFLAGS -fprofile-instr-generate -fcoverage-mapping"
		export LDFLAGS="$LDFLAGS -fprofile-instr-generate"
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
ls -lRa

~/perl5/bin/cover --no-gcov --report=clover

case "${CC:-gcc}" in
	gcc)
		lcov --directory . --capture --output-file lcov-coverage.txt
		lcov --list lcov-coverage.txt
		;;
	clang)
		if [[ x$(uname) == xDarwin ]]; then
			export PATH=$PATH:/Library/Developer/CommandLineTools/usr/bin
		fi
		llvm-profdata merge unittest.profraw -o unittest.profdata
		llvm-profdata merge inttest.profraw -o inttest.profdata
		llvm-cov show -show-line-counts-or-regions -instr-profile=unittest.profdata test/unittest_bin > unittest-cmake-coverage.txt
		llvm-cov show -show-line-counts-or-regions -instr-profile=inttest.profdata /usr/local/bin/znc > inttest-znc-coverage.txt
		find /usr/local/lib/znc -name '*.so' -or -name '*.bundle' | while read f; do llvm-cov show -show-line-counts-or-regions -instr-profile=inttest.profdata $f > inttest-$(basename $f)-coverage.txt; done
		;;
esac
