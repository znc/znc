# vim: filetype=make

ifeq "$(PERL_ON)" "yes"
# We execute this now so that we see the 'beauty' of these flags in make's output
PERLCOMMON   := $(shell $(PERL) -MExtUtils::Embed -e perl_inc -e ldopts)
# Perl API is ugly, casting string literals to char* and redeclaring functions :(
PERLCOMMON   += -Wno-write-strings -Wno-redundant-decls -Wno-missing-declarations
# This is for SWIG
PERLCOMMON   += -DSWIG_TYPE_TABLE=znc
modperlFLAGS := $(PERLCOMMON)
# Find additional headers for out-of-tree build
modperlFLAGS += -I.

PERLHOOK     := modperl_install
CLEAN    += modperl/ZNC.so modperl/ZNC.pm
CLEAN    += modperl/swigperlrun.h modperl/ZNC.cpp modperl/functions.cpp

else
FILES    := $(shell echo $(FILES) | sed -e "s/modperl//")
endif

.PHONY: modperl_install

install: $(PERLHOOK)

modperl.so: modperl/ZNC.so modperl/swigperlrun.h
modperl/ZNC.so: modperl/ZNC.cpp Makefile modperl/functions.cpp
	@mkdir -p modperl
	@mkdir -p .depend
	$(E) Building ZNC Perl bindings library...
	$(Q)$(CXX) $(MODFLAGS) $(LDFLAGS) $(MODLINK) -I$(srcdir) -MMD -MF .depend/modperl.library.dep $(PERLCOMMON) -Wno-unused-variable -o $@ $<
ifneq "$(SWIG)" ""
modperl/swigperlrun.h:
	@mkdir -p modperl
	$(Q)$(SWIG) -perl5 -c++ -shadow -external-runtime $@
modperl/ZNC.cpp: modperl/modperl.i Makefile modperl/module.h modperl/CString.i
	$(E) Generating ZNC API for Perl...
	@mkdir -p modperl
	@mkdir -p .depend
	$(Q)$(SWIG) -perl5 -c++ -shadow -outdir modperl -I$(srcdir) -MMD -MF .depend/modperl.swig.dep -w362,315,401,402 -o $@ $<
endif
modperl/functions.cpp: modperl/functions.in
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
