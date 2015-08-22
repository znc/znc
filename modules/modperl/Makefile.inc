# vim: filetype=make

ifeq "$(PERL_ON)" "yes"
# We execute this now so that we see the 'beauty' of these flags in make's output
PERL_CXX   := $(shell $(PERL) -MExtUtils::Embed -e perl_inc)
PERL_LD    := $(shell $(PERL) -MExtUtils::Embed -e ldopts)
# Perl API is ugly, casting string literals to char* and redeclaring functions :(
PERL_CXX   += -Wno-write-strings -Wno-redundant-decls -Wno-missing-declarations
PERL_CXX   += -Wno-type-limits -Wno-sign-compare -Wno-strict-overflow -Wno-unused-value
# perl 5.20 will fix this warning: https://rt.perl.org/Public/Bug/Display.html?id=120670
PERL_CXX   += -Wno-reserved-user-defined-literal -Wno-literal-suffix
# This is for SWIG
PERL_CXX   += -DSWIG_TYPE_TABLE=znc
PERLCEXT_EXT    := $(shell $(PERL) -MConfig -e'print $$Config::Config{dlext}')
modperlCXXFLAGS := $(PERL_CXX) -Wno-unused-function
modperlLDFLAGS  := $(PERL_LD)
# Find additional headers for out-of-tree build
modperlCXXFLAGS += -I.

ifeq "$(ISCYGWIN)" "1"
PERLDEPONMOD := modperl.so
else
PERLDEPONMOD :=
endif

PERLHOOK     := modperl_install
CLEAN    += modperl/ZNC.$(PERLCEXT_EXT) modperl/ZNC.o modperl/gen
ifneq "$(SWIG)" ""
# Only delete these files if we can regenerate them
CLEAN    += modperl/ZNC.pm modperl/swigperlrun.h modperl/ZNC.cpp modperl/functions.cpp
endif

all: modperl_all

else
FILES    := $(shell echo $(FILES) | sed -e "s/modperl//")
endif

.PHONY: modperl_install modperl_all

install: $(PERLHOOK)

modperl_all: modperl/ZNC.$(PERLCEXT_EXT) modperl/swigperlrun.h modperl/functions.cpp

modperl/ZNC.$(PERLCEXT_EXT): modperl/ZNC.o Makefile modperl.so
	$(E) Linking ZNC Perl bindings library...
	$(Q)$(CXX) $(MODFLAGS) $(LDFLAGS) $(MODLINK) -o $@ $< $(PERL_LD) $(PERLDEPONMOD) $(LIBS)

modperl/ZNC.o: modperl/ZNC.cpp Makefile
	@mkdir -p modperl
	@mkdir -p .depend
	$(E) Building ZNC Perl bindings library...
	$(Q)$(CXX) $(MODFLAGS) -I$(srcdir) -MD -MF .depend/modperl.library.dep $(PERL_CXX) -Wno-unused-variable -Wno-shadow -o $@ $< -c

ifneq "$(SWIG)" ""
include $(srcdir)/modperl/Makefile.gen
endif

modperl.o: modperl/functions.cpp modperl/swigperlrun.h

modperl_install: install_datadir modperl_all
	for i in $(wildcard $(srcdir)/*.pm); do \
		$(INSTALL_DATA) $$i $(DESTDIR)$(MODDIR); \
	done
	mkdir -p $(DESTDIR)$(MODDIR)/modperl
	$(INSTALL_PROGRAM) modperl/ZNC.$(PERLCEXT_EXT) $(DESTDIR)$(MODDIR)/modperl
	if test -e modperl/ZNC.pm ; then \
		$(INSTALL_DATA) modperl/ZNC.pm $(DESTDIR)$(MODDIR)/modperl || exit 1 ; \
	else \
		$(INSTALL_DATA) $(srcdir)/modperl/ZNC.pm $(DESTDIR)$(MODDIR)/modperl || exit 1 ; \
	fi
	$(INSTALL_DATA) $(srcdir)/modperl/startup.pl $(DESTDIR)$(MODDIR)/modperl
