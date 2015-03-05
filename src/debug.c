/*****************************************************************************
 *
 * debug.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012-2015 Willaim Weston <william.h.weston@gmail.com>
 *
 * PHASEX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PHASEX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PHASEX.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "phasex.h"
#include "debug.h"


DEBUG_RINGBUFFER    main_debug_queue;

int                 debug       = 0;
unsigned long       debug_class = 0;

DEBUG_CLASS         debug_class_list[16] = {
	{ DEBUG_CLASS_NONE,           "none" },
	{ DEBUG_CLASS_INIT,           "init" },
	{ DEBUG_CLASS_GUI,            "gui" },
	{ DEBUG_CLASS_PARAM,          "param" },
	{ DEBUG_CLASS_RAW_MIDI,       "raw-midi" },
	{ DEBUG_CLASS_MIDI,           "midi" },
	{ DEBUG_CLASS_MIDI_NOTE,      "note" },
	{ DEBUG_CLASS_MIDI_EVENT,     "event" },
	{ DEBUG_CLASS_MIDI_TIMING,    "timing" },
	{ DEBUG_CLASS_AUDIO,          "audio" },
	{ DEBUG_CLASS_JACK_TRANSPORT, "jack-transport" },
	{ DEBUG_CLASS_ENGINE,         "engine" },
	{ DEBUG_CLASS_ENGINE_TIMING,  "engine-timing" },
	{ DEBUG_CLASS_SESSION,        "session" },
	{ DEBUG_CLASS_ALL,            "all" },
	{ (~0UL),                     NULL }
};


/*****************************************************************************
 * init_debug_buffers()
 *****************************************************************************/
void
init_debug_buffers(void)
{
	memset(& (main_debug_queue), 0, sizeof(DEBUG_RINGBUFFER));
	main_debug_queue.read_index = 0;
	g_atomic_int_set(& (main_debug_queue.write_index), 0);
	g_atomic_int_set(& (main_debug_queue.insert_index), DEBUG_BUFFER_MASK);
}


/*****************************************************************************
 * phasex_debug_thread()
 *****************************************************************************/
void *
phasex_debug_thread(void *UNUSED(arg))
{
	init_debug_buffers();

#ifdef DEBUG_STRUCT_SIZES
	fprintf(stderr,
	        "Struct sizes:  param=%d  param_info=%d  global=%d\n"
	        "               part=%d  voice=%d  patch=%d  patch_state=%d\n"
	        "               delay=%d (%u buf)  chorus=%d (%u buf)\n",
	        (int) sizeof(PARAM), (int) sizeof(PARAM_INFO), (int) sizeof(global),
	        (int) sizeof(PART), (int) sizeof(VOICE),
	        (int) sizeof(PATCH), (int) sizeof(PATCH_STATE),
	        (int) sizeof(DELAY), (unsigned int)(DELAY_MAX * 2 * sizeof(sample_t)),
	        (int) sizeof(CHORUS), (unsigned int)(CHORUS_MAX * 2 * sizeof(sample_t)));
#endif

	while (!pending_shutdown) {
		while (main_debug_queue.read_index !=
		       g_atomic_int_get(& (main_debug_queue.write_index))) {
			fprintf(stderr, "%s",
			        main_debug_queue.msgs[main_debug_queue.read_index].msg);
			main_debug_queue.read_index =
				(main_debug_queue.read_index + 1) & DEBUG_BUFFER_MASK;
		}
		usleep(16000 >> (PHASEX_CPU_POWER - 1));
	}

	pthread_exit(NULL);
	return NULL;
}
