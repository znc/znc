# vim: filetype=make

ifneq "$(PYCFG)" ""
PYTHONCOMMON := $(shell $(PYCFG) --includes) $(shell $(PYCFG) --ldflags)
PYTHONCOMMON += -DSWIG_TYPE_TABLE=znc
PYTHONCOMMON += -Wno-missing-field-initializers -Wno-unused -Wno-shadow
PYTHONCOMMON += -Wno-missing-declarations -Wno-uninitialized
modpythonFLAGS := $(PYTHONCOMMON) -I.

PYTHONHOOK   := modpython_install
CLEAN    += modpython/_znc_core.so modpython/_znc_core.cpp modpython/znc_core.py modpython/znc_core.pyc
CLEAN    += modpython/swigpyrun.h modpython/znc.pyc modpython/functions.cpp modpython/compiler *.pyc
ifneq "$(srcdir)" "."
# Copied from source for out-of-tree builds
CLEAN    += modpython/znc.py
endif

else
FILES    := $(shell echo $(FILES) | sed -e "s/modpython//")
endif

.PHONY: modpython_install modpython_all

install: $(PYTHONHOOK)

# This will run: modpython/compiler blah.py blah.pyc
%.pyc: modpython/compiler %.py
	$(E) Compiling $@...
	$(Q)$^ $@

ifneq "$(PYCFG)" ""
all: modpython_all
endif
modpython_all: modpython/_znc_core.so modpython/swigpyrun.h modpython/znc.pyc modpython/znc_core.pyc
modpython_all: $(addsuffix c, $(notdir $(wildcard $(srcdir)/*.py)))

modpython/znc_core.py: modpython/_znc_core.so
modpython/_znc_core.so: modpython/_znc_core.cpp Makefile modpython/functions.cpp
	@mkdir -p modpython
	@mkdir -p .depend
	$(E) Building ZNC python bindings library...
	$(Q)$(CXX) $(MODFLAGS) $(LDFLAGS) $(MODLINK) -I$(srcdir) -MMD -MF .depend/modpython.library.dep $(PYTHONCOMMON) -o $@ $<
ifneq "$(SWIG)" ""
modpython/swigpyrun.h:
	@mkdir -p modpython
	$(Q)$(SWIG) -python -py3 -c++ -shadow -external-runtime $@
modpython/_znc_core.cpp: modpython/modpython.i Makefile
	$(E) Generating ZNC API for python...
	@mkdir -p modpython
	@mkdir -p .depend
	$(Q)$(SWIG) -python -py3 -c++ -shadow -outdir modpython -I$(srcdir) -MMD -MF .depend/modpython.swig.dep -w362,315,401 -o $@ $<
endif
modpython/functions.cpp: modpython/functions.in
	@mkdir -p modpython
	$(Q)$(PERL) $(srcdir)/modpython/codegen.pl $< $@

modpython/compiler: modpython/compiler.cpp Makefile
	$(E) Building optimizator for python files...
	$(Q)$(CXX) $(PYTHONCOMMON) -o $@ $<

modpython_install: create_install_dir install_metadirs modpython_all
	for i in $(wildcard *.pyc); do \
		$(INSTALL_DATA) $$i $(DESTDIR)$(MODDIR); \
	done
	for a in $(srcdir)/*; do \
		if [ -d $$a ]; then \
			if [ -f $${a}.py ]; then \
				cp -Rp $$a $(DESTDIR)$(DATADIR); \
			fi \
		fi \
	done
	mkdir -p $(DESTDIR)$(MODDIR)/modpython
	$(INSTALL_PROGRAM) modpython/_znc_core.so $(DESTDIR)$(MODDIR)/modpython
	$(INSTALL_DATA) modpython/znc_core.pyc $(DESTDIR)$(MODDIR)/modpython
	$(INSTALL_DATA) modpython/znc.pyc $(DESTDIR)$(MODDIR)/modpython
