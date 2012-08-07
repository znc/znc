# vim: filetype=make

ifeq "$(PERL_ON)" "yes"
# We execute this now so that we see the 'beauty' of these flags in make's output
PERL_CXX   := $(shell $(PERL) -MExtUtils::Embed -e perl_inc)
PERL_LD    := $(shell $(PERL) -MExtUtils::Embed -e ldopts)
# Perl API is ugly, casting string literals to char* and redeclaring functions :(
PERL_CXX   += -Wno-write-strings -Wno-redundant-decls -Wno-missing-declarations
PERL_CXX   += -Wno-type-limits -Wno-sign-compare -Wno-strict-overflow
# This is for SWIG
PERL_CXX   += -DSWIG_TYPE_TABLE=znc
modperlCXXFLAGS := $(PERL_CXX)
modperlLDFLAGS  := $(PERL_LD)
# Find additional headers for out-of-tree build
modperlCXXFLAGS += -I.

PERLHOOK     := modperl_install
CLEAN    += modperl/ZNC.so modperl/ZNC.pm modperl/ZNC.o
CLEAN    += modperl/swigperlrun.h modperl/ZNC.cpp modperl/functions.cpp
CLEAN    += modperl/gen

else
FILES    := $(shell echo $(FILES) | sed -e "s/modperl//")
endif

.PHONY: modperl_install

install: $(PERLHOOK)

modperl.o: modperl/ZNC.so modperl/swigperlrun.h modperl/functions.cpp

modperl/ZNC.so: modperl/ZNC.o Makefile
	$(E) Linking ZNC Perl bindings library...
	$(Q)$(CXX) $(MODFLAGS) $(LDFLAGS) $(MODLINK) $(PERL_LD) -o $@ $<

modperl/ZNC.o: modperl/ZNC.cpp Makefile
	@mkdir -p modperl
	@mkdir -p .depend
	$(E) Building ZNC Perl bindings library...
	$(Q)$(CXX) $(MODFLAGS) -I$(srcdir) -MD -MF .depend/modperl.library.dep $(PERL_CXX) -Wno-unused-variable -Wno-shadow -o $@ $< -c

modperl/swigperlrun.h modperl/functions.cpp modperl/ZNC.cpp modperl/ZNC.pm: modperl/modperl.tar.bz2
	@mkdir -p modperl
	$(E) Unpacking modperl files...
	$(Q)tar -xf $^ -C modperl
	$(Q)touch modperl/swigperlrun.h modperl/functions.cpp modperl/ZNC.cpp modperl/ZNC.pm

ifneq "$(SWIG)" ""
modperl/modperl.tar.bz2: modperl/codegen.pl modperl/functions.in Makefile
	@mkdir -p modperl .depend
	$(Q)$(MAKE) -f $(srcdir)/modperl/Makefile.gen $(C) SWIG="$(SWIG)" srcdir="$(srcdir)" PERL="$(PERL)" VERBOSE="$(VERBOSE)" SED="$(SED)"
endif

modperl_install: install_datadir modperl.so
	for i in $(wildcard $(srcdir)/*.pm); do \
		$(INSTALL_DATA) $$i $(DESTDIR)$(MODDIR); \
	done
	mkdir -p $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_PROGRAM) modperl/ZNC.so $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_DATA) modperl/ZNC.pm $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_DATA) $(srcdir)/modperl/startup.pl $(DESTDIR)$(MODDIR)/modperl
