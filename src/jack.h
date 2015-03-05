/*****************************************************************************
 *
 * jack.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2015 Willaim Weston <william.h.weston@gmail.com>
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
#ifndef _PHASEX_JACK_H_
#define _PHASEX_JACK_H_

#include <jack/jack.h>
#include <glib.h>
#include "phasex.h"


typedef struct jack_port_info {
	jack_port_t             *port;
	char                    *name;
	char                    *type;
	int                     connected;
	short                   connect_request;
	short                   disconnect_request;
	struct jack_port_info   *next;
} JACK_PORT_INFO;


extern jack_client_t        *jack_audio_client;

extern jack_port_t          *midi_input_port;

extern JACK_PORT_INFO       *jack_midi_ports;

extern pthread_mutex_t      sample_rate_mutex;
extern pthread_cond_t       sample_rate_cond;

extern volatile gint        new_sample_rate;
extern volatile gint        new_buffer_period_size;

extern int                  buffer_size_changed;
extern int                  jack_midi_ports_changed;
extern int                  jack_running;

extern char                 *jack_session_uuid;


int jack_process_buffer_multi_out(jack_nframes_t nframes, void *UNUSED(arg));
int jack_process_buffer_stereo_out(jack_nframes_t nframes, void *UNUSED(arg));

void jack_port_info_free(JACK_PORT_INFO *portinfo, int follow);
JACK_PORT_INFO *jack_get_midi_port_list(void);

void jack_shutdown(void *UNUSED(arg));
int  jack_audio_init(void);
int  jack_start(void);
int  jack_stop(void);
void jack_restart(void);
void jack_watchdog_cycle(void);
void *jack_audio_thread(void *UNUSED(arg));

char *jack_get_session_name_from_directory(const char *directory);


#endif /* _PHASEX_JACK_H_ */
