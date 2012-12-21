## Process this file with automake to produce Makefile.in

patchesdir = $(pkgdatadir)/sys-patches

patches = phasex-default.phx \
	alien-breeze.phx \
	almost-perfect-pad.phx \
	basic-square-arp.phx \
	cyclic-noise.phx \
	filter-pipes.phx \
	fm-polysynth.phx \
	go-crazy-on-it.phx \
	mellow-pad.phx \
	ooohaah.phx \
	out-of-the-mist.phx \
	pulsed-bass.phx \
	robot-whales.phx \
	spinlock.phx \
	synth-bass-drum.phx \
	thick-pad.phx \
	vibrato-organ.phx


dist_patches_DATA = $(patches)


uninstall-hook:
	test -d $(DESTDIR)$(pkgdatadir)/sys-patches && rmdir $(DESTDIR)$(pkgdatadir)/sys-patches


clean-local:


distclean-local:
	rm -f *~
