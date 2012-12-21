## Process this file with automake to produce Makefile.in

themesdir = $(datadir)/themes

miscdir = $(pkgdatadir)
dist_misc_DATA = gtkenginerc

theme_files = Phasex-Dark Phasex-Light

EXTRA_DIST = $(theme_files)


install-data-local:
	for theme in $(theme_files); do \
		$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/$$theme; \
		$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/$$theme/gtk-2.0; \
		if test -f $$theme; then \
			$(INSTALL_DATA) $$theme $(DESTDIR)$(datadir)/themes/$$theme/gtk-2.0/gtkrc; \
		fi \
	done


uninstall-hook:
	for theme in $(theme_files); do \
		rm -f $(DESTDIR)$(datadir)/themes/$$theme/gtk-2.0/gtkrc; \
		rmdir $(DESTDIR)$(datadir)/themes/$$theme/gtk-2.0; \
		rmdir $(DESTDIR)$(datadir)/themes/$$theme; \
	done
	test -d $(DESTDIR)$(datadir)/themes && rmdir --ignore-fail-on-non-empty $(DESTDIR)$(datadir)/themes


clean-local:


distclean-local:
	rm -f *~
