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
	$(E) Building ZNC Perl bindings library...
	$(Q)$(CXX) $(MODFLAGS) $(LDFLAGS) $(MODLINK) -I$(srcdir) $(PERLCOMMON) -o $@ $<
ifneq "$(SWIG)" ""
modperl/swigperlrun.h:
	@mkdir -p modperl
	$(Q)$(SWIG) -perl5 -c++ -shadow -external-runtime $@
modperl/ZNC.cpp: modperl/modperl.i Makefile modperl/module.h modperl/CString.i
	$(E) Generating ZNC API for Perl...
	@mkdir -p modperl
	$(Q)$(SWIG) -perl5 -c++ -shadow -outdir modperl -I$(srcdir) -w362,315,401,402 -o $@.warn $<
	$(Q)$(PERL) -pe '$$x==2 and print "items+=0;" and $$x=3;/^XS\(SWIG_init\)/ and $$x=1;$$x==1 and /dXSARGS/ and $$x=2' $@.warn > $@
	$(Q)rm -rf $@.warn
endif
modperl/functions.cpp: modperl/functions.in
	@mkdir -p modperl
	$(Q)$(PERL) $(srcdir)/modperl/codegen.pl $< $@

modperl_install: create_install_dir install_metadirs modperl.so
	for i in $(wildcard $(srcdir)/*.pm); do \
		$(INSTALL_DATA) $$i $(DESTDIR)$(MODDIR); \
	done
	for a in $(srcdir)/*; do \
		if [ -d $$a ] && [ -f $${a}.pm ]; then \
			cp -Rp $$a $(DESTDIR)$(DATADIR); \
		fi \
	done
	mkdir -p $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_PROGRAM) modperl/ZNC.so $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_DATA) modperl/ZNC.pm $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_DATA) $(srcdir)/modperl/startup.pl $(DESTDIR)$(MODDIR)/modperl
