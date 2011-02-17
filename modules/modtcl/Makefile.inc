ifeq "$(TCL_FLAGS)" ""
FILES       := $(shell echo $(FILES) | sed -e "s:modtcl::")
else
TCLHOOK     := modtcl_install
endif
modtclFLAGS := $(TCL_FLAGS)

.PHONY: modtcl_install

install: $(TCLHOOK)

modtcl_install:
	mkdir -p $(DESTDIR)$(DATADIR)/modtcl/
	$(INSTALL_DATA) $(srcdir)/modtcl/modtcl.tcl $(srcdir)/modtcl/binds.tcl $(DESTDIR)$(DATADIR)/modtcl/
