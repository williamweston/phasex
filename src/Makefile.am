## Process this file with automake to produce Makefile.in

bin_PROGRAMS    = phasex

phasex_SOURCES  = \
	alsa_pcm.c alsa_pcm.h \
	alsa_seq.c alsa_seq.h \
	bank.c bank.h \
	bpm.c bpm.h \
	buffer.c buffer.h \
	debug.c debug.h \
	driver.c driver.h \
	engine.c engine.h \
	filter.c filter.h \
	gtkknob.c gtkknob.h \
	gui_alsa.c gui_alsa.h \
	gui_bank.c gui_bank.h \
	gui_jack.c gui_jack.h \
	gui_layout.c gui_layout.h \
	gui_main.c gui_main.h \
	gui_menubar.c gui_menubar.h \
	gui_midimap.c gui_midimap.h \
	gui_navbar.c gui_navbar.h \
	gui_param.c gui_param.h \
	gui_patch.c gui_patch.h \
	gui_session.c gui_session.h \
	help.c help.h \
	jack.c jack.h \
	jack_midi.c jack_midi.h \
	jack_transport.c jack_transport.h \
	mididefs.h \
	midi_event.c midi_event.h \
	midimap.c midimap.h \
	midi_process.c midi_process.h \
	param.c param.h \
	param_cb.c param_cb.h \
	param_parse.c param_parse.h \
	param_strings.c param_strings.h \
	patch.c patch.h \
	phasex.c phasex.h \
	rawmidi.c rawmidi.h \
	session.c session.h \
	settings.c settings.h \
	string_util.c string_util.h \
	timekeeping.c timekeeping.h \
	wave.c wave.h

if WITH_LASH
    phasex_SOURCES  += lash.c lash.h
endif


AM_CFLAGS       = @PHASEX_CFLAGS@
AM_CPPFLAGS     = $(EXTRA_CPPFLAGS) @PHASEX_CPPFLAGS@
phasex_LDADD    = $(INTLLIBS) @PHASEX_LIBS@


clean-local:


distclean-local:
	rm -f *~
