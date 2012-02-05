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
ifneq "$(SWIG)" ""
modperl/swigperlrun.h:
	@mkdir -p modperl
	$(Q)$(SWIG) -perl5 -c++ -shadow -external-runtime $@
modperl/ZNC.cpp: modperl/modperl.i Makefile modperl/module.h modperl/CString.i
	$(E) Generating ZNC API for Perl...
	@mkdir -p modperl
	@mkdir -p .depend
	$(Q)$(SWIG) -perl5 -c++ -shadow -outdir modperl -I$(srcdir) -MD -MF .depend/modperl.swig.dep -w362,315,401,402 -o $@ $<
endif
modperl/functions.cpp: modperl/functions.in modperl/codegen.pl
	@mkdir -p modperl
	$(Q)$(PERL) $(srcdir)/modperl/codegen.pl $< $@

modperl_install: install_datadir modperl.so
	for i in $(wildcard $(srcdir)/*.pm); do \
		$(INSTALL_DATA) $$i $(DESTDIR)$(MODDIR); \
	done
	mkdir -p $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_PROGRAM) modperl/ZNC.so $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_DATA) modperl/ZNC.pm $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_DATA) $(srcdir)/modperl/startup.pl $(DESTDIR)$(MODDIR)/modperl
