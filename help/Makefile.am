## Process this file with automake to produce Makefile.in

helpdir = $(pkgdatadir)/help

help_files = parameters.help

dist_help_DATA = $(help_files)


uninstall-hook:
	test -d $(DESTDIR) && test -d $(DESTDIR)$(helpdir) && rmdir --ignore-fail-on-non-empty $(DESTDIR)$(helpdir)


clean-local:


distclean-local:
	rm -f *~
