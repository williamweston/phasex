## Process this file with automake to produce Makefile.in

miscdir = $(pkgdatadir)

dist_misc_DATA = patchbank

dist_bin_SCRIPTS = phasex-convert-patch

desktopdir = $(datadir)/applications
desktop_files = phasex.desktop
desktop_in_files = phasex.desktop.in

EXTRA_DIST = $(desktop_in_files:.desktop.in=.desktop)

desktop_base = Application AudioVideo Audio
desktop_extra = X-Jack X-MIDI X-Synthesis X-Digital_Processing


install-data-local:
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(datadir)/applications
	desktop-file-install \
		--dir $(DESTDIR)$(datadir)/applications \
		`for c in $(desktop_base) $(desktop_extra); do echo "--add-category $$c "; done` \
		$(desktop_files)


uninstall-hook:
	test -f $(DESTDIR)$(desktopdir)/phasex.desktop && \
		rm -f $(DESTDIR)$(desktopdir)/phasex.desktop


clean-local:


distclean-local:
	rm -f *~
