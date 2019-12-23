EXTRA_DIST = index-1.html index-2.html index-3.html index-4.html index-5.html index-6.html index.html 

install-data-local:
	$(mkinstalldirs) $(prefix)/share/doc/cvoicecontrol/html/en
	$(INSTALL_DATA) index.html $(prefix)/share/doc/cvoicecontrol/html/en/index.html
	$(INSTALL_DATA) index-1.html $(prefix)/share/doc/cvoicecontrol/html/en/index-1.html
	$(INSTALL_DATA) index-2.html $(prefix)/share/doc/cvoicecontrol/html/en/index-2.html
	$(INSTALL_DATA) index-3.html $(prefix)/share/doc/cvoicecontrol/html/en/index-3.html
	$(INSTALL_DATA) index-4.html $(prefix)/share/doc/cvoicecontrol/html/en/index-4.html
	$(INSTALL_DATA) index-5.html $(prefix)/share/doc/cvoicecontrol/html/en/index-5.html
	$(INSTALL_DATA) index-6.html $(prefix)/share/doc/cvoicecontrol/html/en/index-6.html

uninstall-local:
	-rm -f $(prefix)/share/doc/cvoicecontrol/html/en/index.html
	-rm -f $(prefix)/share/doc/cvoicecontrol/html/en/index-1.html
	-rm -f $(prefix)/share/doc/cvoicecontrol/html/en/index-2.html
	-rm -f $(prefix)/share/doc/cvoicecontrol/html/en/index-3.html
	-rm -f $(prefix)/share/doc/cvoicecontrol/html/en/index-4.html
	-rm -f $(prefix)/share/doc/cvoicecontrol/html/en/index-5.html
	-rm -f $(prefix)/share/doc/cvoicecontrol/html/en/index-6.html
	rmdir $(prefix)/share/doc/cvoicecontrol/html/en
