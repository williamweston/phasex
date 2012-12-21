## Process this file with automake to produce Makefile.in

midimapsdir = $(pkgdatadir)/sys-midimaps

midimaps_files = default.map

dist_midimaps_DATA = $(midimaps_files)


install-data-local:
	mkdir -p $(DESTDIR)$(midimapsdir); \
	for midimap in $(midimaps_files); do \
		$(INSTALL_DATA) $(srcdir)/$$midimap $(DESTDIR)$(midimapsdir)/$$midimap; \
	done


uninstall-local:
	for midimap in $(midimaps_files); do \
		rm -f $(DESTDIR)$(midimapsdir)/$$midimap; \
	done
	test -d $(DESTDIR)$(midimapsdir) && rmdir $(DESTDIR)$(midimapsdir)


clean-local:


distclean-local:
	rm -f *~
