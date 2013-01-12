## Process this file with automake to produce Makefile.in

pixmapsdir = $(pkgdatadir)/pixmaps

theme_dirs = Dark Light

knob_files = \
	Dark/knob-16x16.png \
	Dark/knob-20x20.png \
	Dark/knob-24x24.png \
	Dark/knob-28x28.png \
	Dark/knob-32x32.png \
	Dark/knob-36x36.png \
	Dark/knob-40x40.png \
	Dark/knob-44x44.png \
	Dark/knob-48x48.png \
	Dark/knob-52x52.png \
	Dark/knob-56x56.png \
	Dark/knob-60x60.png \
	Dark/knob-64x64.png \
	Dark/detent-knob-16x16.png \
	Dark/detent-knob-20x20.png \
	Dark/detent-knob-24x24.png \
	Dark/detent-knob-28x28.png \
	Dark/detent-knob-32x32.png \
	Dark/detent-knob-36x36.png \
	Dark/detent-knob-40x40.png \
	Dark/detent-knob-44x44.png \
	Dark/detent-knob-48x48.png \
	Dark/detent-knob-52x52.png \
	Dark/detent-knob-56x56.png \
	Dark/detent-knob-60x60.png \
	Dark/detent-knob-64x64.png \
	Light/knob-16x16.png \
	Light/knob-20x20.png \
	Light/knob-24x24.png \
	Light/knob-28x28.png \
	Light/knob-32x32.png \
	Light/knob-36x36.png \
	Light/knob-40x40.png \
	Light/knob-44x44.png \
	Light/knob-48x48.png \
	Light/knob-52x52.png \
	Light/knob-56x56.png \
	Light/knob-60x60.png \
	Light/knob-64x64.png \
	Light/detent-knob-16x16.png \
	Light/detent-knob-20x20.png \
	Light/detent-knob-24x24.png \
	Light/detent-knob-28x28.png \
	Light/detent-knob-32x32.png \
	Light/detent-knob-36x36.png \
	Light/detent-knob-40x40.png \
	Light/detent-knob-44x44.png \
	Light/detent-knob-48x48.png \
	Light/detent-knob-52x52.png \
	Light/detent-knob-56x56.png \
	Light/detent-knob-60x60.png \
	Light/detent-knob-64x64.png

icon_files = \
	phasex-icon-16x16.png \
	phasex-icon-22x22.png \
	phasex-icon-24x24.png \
	phasex-icon-32x32.png \
	phasex-icon-36x36.png \
	phasex-icon-48x48.png \
	phasex-icon-64x64.png \
	phasex-icon-72x72.png \
	phasex-icon-96x96.png \
	phasex-icon-128x128.png \
	phasex-icon-192x192.png \
	phasex-icon-256x256.png \
	phasex-icon-512x512.png \
	phasex-icon-scalable.svg \
	phasex-icon-tiny.svg

splash_image_file = phasex-splash.png

default_icon_file = phasex-icon.png

nobase_dist_pixmaps_DATA = $(knob_files) $(icon_files) $(splash_image_file)


gtk_update_icon_cache = gtk-update-icon-cache -f -t


update-icon-cache:
	@if test -z "$(DESTDIR)"; then \
		if echo "${datadir}" | grep -q "_inst"; then \
			rm -f $(DESTDIR)$(datadir)/icons/hicolor/icon-theme.cache; \
		else \
			echo "Updating Gtk icon cache."; \
			$(gtk_update_icon_cache) $(datadir)/icons/hicolor; \
		fi; \
	else \
		echo "*** Icon cache not updated.  After (un)install, please run:"; \
		echo "***   $(gtk_update_icon_cache) $(DESTDIR)$(datadir)/icons/hicolor"; \
		rm -f $(DESTDIR)$(datadir)/icons/hicolor/icon-theme.cache; \
	fi


install-icons:
	@for icon in $(icon_files); do \
		NAME=`echo $$icon | cut -f 1-2 -d '-'` \
		EXTENSION=`echo $$icon | cut -f 2 -d '.'` \
		SIZE=`echo $$icon | cut -f 3 -d '-' | cut -f 1 -d '.'`; \
		mkdir -p $(DESTDIR)$(datadir)/icons/hicolor/$$SIZE/apps; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/icons/hicolor/$$SIZE/apps/$$NAME.$$EXTENSION; \
	done; \
	$(INSTALL_DATA) $(srcdir)/phasex-icon-256x256.png $(DESTDIR)$(datadir)/pixmaps/$(default_icon_file); \
	$(INSTALL_DATA) $(srcdir)/phasex-icon-256x256.png $(DESTDIR)$(pkgdatadir)/pixmaps/$(default_icon_file)


uninstall-icons:
	@for icon in $(icon_files); do \
		NAME=`echo $$icon | cut -f 1-2 -d '-'` \
		EXTENSION=`echo $$icon | cut -f 2 -d '.'` \
		SIZE=`echo $$icon | cut -f 3 -d '-' | cut -f 1 -d '.'`; \
		rm -f $(DESTDIR)$(datadir)/icons/hicolor/$$SIZE/apps/$$NAME.$$EXTENSION; \
	done;


install-pixmaps:
	@mkdir -p $(DESTDIR)$(datadir)/pixmaps; \
	for icon in $(icon_files); do \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/pixmaps/$$icon; \
	done;


uninstall-pixmaps:
	@for icon in $(icon_files); do \
		rm -f $(DESTDIR)$(datadir)/pixmaps/$$icon; \
	done; \
	rm -f $(DESTDIR)$(datadir)/pixmaps/$(default_icon_file); \
	rm -f $(DESTDIR)$(pkgdatadir)/pixmaps/$(default_icon_file);


install-splash:
	@mkdir -p $(DESTDIR)$(pkgdatadir)/pixmaps; \
	$(INSTALL_DATA) $(srcdir)/$(splash_image_file) $(DESTDIR)$(pkgdatadir)/pixmaps/$(splash_image_file)


uninstall-splash:
	@rm -f $(DESTDIR)$(pkgdatadir)/pixmaps/$(splash_image_file)


install-data-local: install-pixmaps install-splash install-icons update-icon-cache


uninstall-local: uninstall-pixmaps uninstall-splash uninstall-icons update-icon-cache


install-data-hook:
	@mkdir -p $(DESTDIR)$(pkgdatadir)/pixmaps


uninstall-hook:
	@for themedir in $(theme_dirs); do \
		test -d $(DESTDIR)$(pixmapsdir) && rmdir $(DESTDIR)$(pixmapsdir)/$$themedir; \
	done; \
	test -d $(DESTDIR)$(pixmapsdir) && rmdir $(DESTDIR)$(pixmapsdir)


distuninstallcheck:
	@:


clean-local:


distclean-local:
	rm -f *~
