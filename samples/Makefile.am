## Process this file with automake to produce Makefile.in

samplesdir = $(pkgdatadir)/sys-samples

sample_files = \
	juno_osc.raw \
	juno_poly.raw \
	juno_saw.raw \
	juno_square.raw \
	vox_1.raw \
	vox_2.raw \
	vox_3.raw \
	analog_square.raw

dist_samples_DATA = $(sample_files)


uninstall-hook:
	test -d $(DESTDIR)$(samplesdir) && rmdir $(DESTDIR)$(samplesdir)


clean-local:


distclean-local:
	rm -f *~
