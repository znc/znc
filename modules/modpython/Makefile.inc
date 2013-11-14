# vim: filetype=make

ifeq "$(PYTHON_ON)" "yes"
PYTHONCOMMON := $(PY_CFLAGS)
PYTHONCOMMON += -DSWIG_TYPE_TABLE=znc
# Could someone fix all of these in swig / python, please?
PYTHONCOMMON += -Wno-missing-field-initializers -Wno-unused -Wno-shadow
PYTHONCOMMON += -Wno-missing-declarations -Wno-uninitialized -Wno-switch-enum
PYTHONCOMMON += -Wno-redundant-decls
modpythonCXXFLAGS := $(PYTHONCOMMON) -I.
modpythonLDFLAGS  := $(PY_LDFLAGS)

ifeq "${ISCYGWIN}" "1"
PYCEXT_EXT := dll
PYDEPONMOD := ./modpython.so
else
PYCEXT_EXT := so
PYDEPONMOD :=
endif

PYTHONHOOK   := modpython_install
CLEAN    += modpython/_znc_core.$(PYCEXT_EXT)
CLEAN    += modpython/_znc_core.o modpython/compiler.o
ifneq "$(SWIG)" ""
# Only delete these files if we can regenerate them
CLEAN    += modpython/_znc_core.cpp modpython/znc_core.py
CLEAN    += modpython/swigpyrun.h modpython/functions.cpp
endif
ifneq "$(srcdir)" "."
# Copied from source for out-of-tree builds
CLEAN    += modpython/znc.py
endif

else
FILES    := $(shell echo $(FILES) | sed -e "s/modpython//")
endif

.PHONY: modpython_install modpython_all

install: $(PYTHONHOOK)

ifeq "$(PYTHON_ON)" "yes"
all: modpython_all
endif
modpython_all: modpython/_znc_core.$(PYCEXT_EXT)

modpython/_znc_core.o: modpython/_znc_core.cpp Makefile
	@mkdir -p modpython
	@mkdir -p .depend
	$(E) Building ZNC python bindings library...
	$(Q)$(CXX) $(MODFLAGS) -I$(srcdir) -MD -MF .depend/modpython.library.dep $(PYTHONCOMMON) -o $@ $< -c

modpython/_znc_core.$(PYCEXT_EXT): modpython/_znc_core.o Makefile modpython.so
	$(E) Linking ZNC python bindings library...
	$(Q)$(CXX) $(MODFLAGS) $(LDFLAGS) $(MODLINK) -o $@ $< $(PY_LDFLAGS) $(PYDEPONMOD)

ifneq "$(SWIG)" ""
include $(srcdir)/modpython/Makefile.gen
endif

modpython.o: modpython/functions.cpp modpython/swigpyrun.h

modpython/compiler.o: modpython/compiler.cpp Makefile
	@mkdir -p modpython
	@mkdir -p .depend
	$(E) Building optimizer for python files...
	$(Q)$(CXX) $(PYTHONCOMMON) -o $@ $< -c -MD -MF .depend/modpython.compiler.dep
modpython/compiler: modpython/compiler.o Makefile
	$(E) Linking optimizer for python files...
	$(Q)$(CXX) -o $@ $< $(PY_LDFLAGS)

modpython_install: install_datadir modpython_all
	-for i in $(srcdir)/*.py; do \
		$(INSTALL_DATA) $$i $(DESTDIR)$(MODDIR); \
	done
	mkdir -p $(DESTDIR)$(MODDIR)/modpython
	$(INSTALL_PROGRAM) modpython/_znc_core.$(PYCEXT_EXT) $(DESTDIR)$(MODDIR)/modpython
	if test -r modpython/znc_core.py;\
		then $(INSTALL_DATA) modpython/znc_core.py $(DESTDIR)$(MODDIR)/modpython;\
		else $(INSTALL_DATA) $(srcdir)/modpython/znc_core.py $(DESTDIR)$(MODDIR)/modpython;\
	fi
	$(INSTALL_DATA) $(srcdir)/modpython/znc.py $(DESTDIR)$(MODDIR)/modpython
