dist-hook:

SUBDIRS = src docs

EXTRA_DIST = AUTHORS COPYING ChangeLog INSTALL README BUGS FAQ

AUTOMAKE_OPTIONS = foreign

install-data-local:
	$(mkinstalldirs) $(prefix)/share/doc/cvoicecontrol
	$(INSTALL_DATA) AUTHORS $(prefix)/share/doc/cvoicecontrol/AUTHORS
	$(INSTALL_DATA) FAQ     $(prefix)/share/doc/cvoicecontrol/FAQ
	$(INSTALL_DATA) BUGS    $(prefix)/share/doc/cvoicecontrol/BUGS
	$(INSTALL_DATA) README  $(prefix)/share/doc/cvoicecontrol/README
	$(INSTALL_DATA) COPYING $(prefix)/share/doc/cvoicecontrol/COPYING

uninstall-local:
	-rm -f $(prefix)/share/doc/cvoicecontrol/AUTHORS
	-rm -f $(prefix)/share/doc/cvoicecontrol/FAQ
	-rm -f $(prefix)/share/doc/cvoicecontrol/BUGS
	-rm -f $(prefix)/share/doc/cvoicecontrol/README
	-rm -f $(prefix)/share/doc/cvoicecontrol/COPYING
	rmdir $(prefix)/share/doc/cvoicecontrol
