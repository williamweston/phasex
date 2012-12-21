## Process this file with automake to produce Makefile.in

patchesdir = $(pkgdatadir)/sys-patches

patches = default.phx

dist_patches_DATA = $(patches)


uninstall-hook:
	test -d $(DESTDIR)$(pkgdatadir)/sys-patches && rmdir $(DESTDIR)$(pkgdatadir)/sys-patches


clean-local:


distclean-local:
	rm -f *~
