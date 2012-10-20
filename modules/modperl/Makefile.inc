# vim: filetype=make

ifeq "$(PERL_ON)" "yes"
# We execute this now so that we see the 'beauty' of these flags in make's output
PERL_CXX   := $(shell $(PERL) -MExtUtils::Embed -e perl_inc)
PERL_LD    := $(shell $(PERL) -MExtUtils::Embed -e ldopts)
# Perl API is ugly, casting string literals to char* and redeclaring functions :(
PERL_CXX   += -Wno-write-strings -Wno-redundant-decls -Wno-missing-declarations
PERL_CXX   += -Wno-type-limits -Wno-sign-compare -Wno-strict-overflow -Wno-unused-value
# This is for SWIG
PERL_CXX   += -DSWIG_TYPE_TABLE=znc
modperlCXXFLAGS := $(PERL_CXX) -Wno-unused-function
modperlLDFLAGS  := $(PERL_LD)
# Find additional headers for out-of-tree build
modperlCXXFLAGS += -I.

ifeq "$(ISCYGWIN)" "1"
PERLCEXT_EXT := dll
PERLDEPONMOD := modperl.so
else
PERLCEXT_EXT := so
PERLDEPONMOD :=
endif

PERLHOOK     := modperl_install
CLEAN    += modperl/ZNC.$(PERLCEXT_EXT) modperl/ZNC.pm modperl/ZNC.o
CLEAN    += modperl/swigperlrun.h modperl/ZNC.cpp modperl/functions.cpp
CLEAN    += modperl/gen
all: modperl_all

else
FILES    := $(shell echo $(FILES) | sed -e "s/modperl//")
endif

.PHONY: modperl_install modperl_all

install: $(PERLHOOK)

modperl_all: modperl/ZNC.$(PERLCEXT_EXT) modperl/swigperlrun.h modperl/functions.cpp

modperl/ZNC.$(PERLCEXT_EXT): modperl/ZNC.o Makefile modperl.so
	$(E) Linking ZNC Perl bindings library...
	$(Q)$(CXX) $(MODFLAGS) $(LDFLAGS) $(MODLINK) -o $@ $< $(PERL_LD) $(PERLDEPONMOD)

modperl/ZNC.o: modperl/ZNC.cpp Makefile
	@mkdir -p modperl
	@mkdir -p .depend
	$(E) Building ZNC Perl bindings library...
	$(Q)$(CXX) $(MODFLAGS) -I$(srcdir) -MD -MF .depend/modperl.library.dep $(PERL_CXX) -Wno-unused-variable -Wno-shadow -o $@ $< -c

ifneq "$(SWIG)" ""
modperl/swigperlrun.h modperl/functions.cpp modperl/ZNC.cpp modperl/ZNC.pm: modperl/codegen.pl modperl/functions.in Makefile
	@mkdir -p modperl .depend
	$(Q)$(MAKE) -f $(srcdir)/modperl/Makefile.gen $(C) SWIG="$(SWIG)" srcdir="$(srcdir)" PERL="$(PERL)" VERBOSE="$(VERBOSE)" SED="$(SED)"
endif

modperl_install: install_datadir modperl_all
	for i in $(wildcard $(srcdir)/*.pm); do \
		$(INSTALL_DATA) $$i $(DESTDIR)$(MODDIR); \
	done
	mkdir -p $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_PROGRAM) modperl/ZNC.$(PERLCEXT_EXT) $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_DATA) modperl/ZNC.pm $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_DATA) $(srcdir)/modperl/startup.pl $(DESTDIR)$(MODDIR)/modperl
