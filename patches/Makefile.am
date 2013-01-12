## Process this file with automake to produce Makefile.in

patchesdir = $(pkgdatadir)/sys-patches

patches = phasex-default.phx \
	alien-breeze.phx \
	almost-perfect-pad.phx \
	arp-factory.phx \
	basic-square-arp.phx \
	culvert.phx \
	cyclic-noise.phx \
	filter-pipes.phx \
	fm-polysynth.phx \
	glitch.phx \
	go-crazy-on-it.phx \
	mellow-pad.phx \
	octave-arpeggiator.phx \
	ooohaah.phx \
	out-of-the-mist.phx \
	pm-detune.phx \
	pulsed-bass.phx \
	robot-whales.phx \
	sci-fi.phx \
	sharp-square.phx \
	simple-arp.phx \
	smooth-squares.phx \
	soft-lead.phx \
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
