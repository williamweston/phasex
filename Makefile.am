## Process this file with automake to produce Makefile.in

SUBDIRS = src help pixmaps themes patches samples midimaps misc scripts

dist_doc_DATA = \
	README \
	LICENSE \
	GPL-3.0.txt \
	AUTHORS \
	ChangeLog \
	TODO \
	doc/ROADMAP \
	doc/signal-diagram.txt \
	doc/release-checklist.txt

#scriptsdir = scripts
#
#dist_scripts_SCRIPTS = \
#	scripts/version-sh

EXTRA_DIST = \
	aclocal.m4 \
	configure \
	ltmain.sh \
	phasex.spec


#install-data-local:
#	@$(NORMAL_INSTALL)


#uninstall-local:
#	@$(NORMAL_UNINSTALL)


uninstall-hook:
	test -d $(DESTDIR)$(pkgdatadir) && rmdir --ignore-fail-on-non-empty $(DESTDIR)$(pkgdatadir)
	test -d $(DESTDIR)$(docdir) && rmdir $(DESTDIR)$(docdir)


distclean-local:
	rm -f *~
	rm -rf autom4te.cache


phasex-$(PACKAGE_VERSION).tar.gz: dist


phasex-version:
	scripts/version-sh


phasex.spec: phasex-version phasex.spec.in


rpm: phasex-$(PACKAGE_VERSION).tar.gz phasex.spec
	rpmbuild=$$(mktemp -d -t rpmbuild-phasex.XXXXXXX); src=$$(pwd); \
	cp phasex-$(PACKAGE_VERSION).tar.gz "$$rpmbuild"; \
	cp phasex.spec $$rpmbuild/phasex.spec; \
	pushd "$$rpmbuild" && \
	rpmbuild \
		--define "_topdir $$PWD" \
		--define "_sourcedir $$PWD" \
		--define "_specdir $$PWD" \
		--define "_srcrpmdir $$PWD" \
		--define "_rpmdir $$PWD" \
		-ba phasex.spec && \
	popd; \
	mv "$$rpmbuild"/$$(arch)/*.rpm . &&  mv "$$rpmbuild"/*.src.rpm . && \
		rm -rf "$$rpmbuild" && ls -1 *.rpm

