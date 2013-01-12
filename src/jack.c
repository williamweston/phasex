/*****************************************************************************
 *
 * jack.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2013 William Weston <whw@linuxmail.org>
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
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#ifdef HAVE_JACK_WEAKJACK_H
# include <jack/weakjack.h>
#endif
#include <jack/jack.h>
#include <jack/midiport.h>
#include <glib.h>
#include "phasex.h"
#include "config.h"
#include "timekeeping.h"
#include "buffer.h"
#include "jack.h"
#include "jack_midi.h"
#include "jack_transport.h"
#include "midi_event.h"
#include "midi_process.h"
#include "engine.h"
#include "bank.h"
#include "session.h"
#include "settings.h"
#include "debug.h"
#include "driver.h"

#ifdef HAVE_JACK_SESSION_H
# include <jack/session.h>
#endif
#ifndef WITHOUT_LASH
# include "lash.h"
#endif


jack_client_t           *jack_audio_client         = NULL;
static char             jack_audio_client_name[64] = "phasex";

int                     num_output_pairs           = 0;

#ifdef ENABLE_INPUTS
jack_port_t             *input_port1;
jack_port_t             *input_port2;
#endif
jack_port_t             *output_port1[MAX_PARTS];
jack_port_t             *output_port2[MAX_PARTS];
jack_port_t             *dest_port1[MAX_PARTS];
jack_port_t             *dest_port2[MAX_PARTS];

jack_port_t             *midi_input_port            = NULL;

pthread_mutex_t         sample_rate_mutex;

pthread_cond_t          sample_rate_cond            = PTHREAD_COND_INITIALIZER;
pthread_cond_t          jack_client_cond            = PTHREAD_COND_INITIALIZER;

int                     jack_running                = 0;

JACK_PORT_INFO          *jack_midi_ports            = NULL;

int                     jack_midi_ports_changed     = 0;
int                     jack_rebuilding_port_list   = 0;

char                    *jack_session_uuid          = NULL;

#ifdef HAVE_JACK_SESSION_H
jack_session_event_t    *jack_session_event         = NULL;
#endif


/*****************************************************************************
 * jack_process_buffer_multi_out()
 *
 * Single jack client with one outpair per part.
 *****************************************************************************/
int
jack_process_buffer_multi_out(jack_nframes_t nframes, void *UNUSED(arg))
{
#ifdef ENABLE_INPUTS
	jack_default_audio_sample_t *in1;
	jack_default_audio_sample_t *in2;
#endif
	jack_default_audio_sample_t *out1;
	jack_default_audio_sample_t *out2;
	unsigned int                i;
	PART                        *part;
	unsigned int                a_index;
#ifdef MATH_64_BIT
	unsigned int                j;
#endif

	if (!jack_running || pending_shutdown || (jack_audio_client == NULL)) {
		return 0;
	}

	set_midi_cycle_time();
	if (midi_driver == MIDI_DRIVER_JACK) {
		jack_process_midi(nframes);
	}
	else if (check_active_sensing_timeout() > 0) {
		broadcast_notes_off();
	}

	a_index = get_audio_index();

	for (i = 0; i < MAX_PARTS; i++) {
		out1 = jack_port_get_buffer(output_port1[i], nframes);
		out2 = jack_port_get_buffer(output_port2[i], nframes);

		part = get_part(i);

#ifdef MATH_32_BIT
		memcpy((void *) out1, (void *) & (part->output_buffer1[a_index]),
		       sizeof(jack_default_audio_sample_t) * nframes);
		memcpy((void *) out2, (void *) & (part->output_buffer2[a_index]),
		       sizeof(jack_default_audio_sample_t) * nframes);
#endif
#ifdef MATH_64_BIT
		for (j = 0; j < nframes; j++) {
			out1[j] = (jack_default_audio_sample_t)part->output_buffer1[a_index + j];
			out2[j] = (jack_default_audio_sample_t)part->output_buffer2[a_index + j];
		}
#endif
	}

#ifdef ENABLE_INPUTS
	in1 = jack_port_get_buffer(input_port1, nframes);
	in2 = jack_port_get_buffer(input_port2, nframes);

# ifdef MATH_32_BIT
	memcpy((void *) & (input_buffer1[a_index]), (void *) in1,
	       sizeof(jack_default_audio_sample_t) * nframes);
	memcpy((void *) & (input_buffer2[a_index]), (void *) in2,
	       sizeof(jack_default_audio_sample_t) * nframes);
# endif
# ifdef MATH_64_BIT
	for (j = 0; j < nframes; j++) {
		input_buffer1[a_index + j] = (sample_t)in1[j];
		input_buffer2[a_index + j] = (sample_t)in2[j];
	}
# endif
#endif

	inc_audio_index(nframes);

	jack_process_transport(nframes);

	return 0;
}


/*****************************************************************************
 * jack_process_buffer_stereo_out()
 *
 * Single jack client with parts mixed down to a single output pair
 *****************************************************************************/
int
jack_process_buffer_stereo_out(jack_nframes_t nframes, void *UNUSED(arg))
{
	PART                        *part;
	unsigned int                i;
	unsigned int                j;
	unsigned int                a_index;
# ifdef ENABLE_INPUTS
	jack_default_audio_sample_t *in1;
	jack_default_audio_sample_t *in2;
# endif
	jack_default_audio_sample_t *out1;
	jack_default_audio_sample_t *out2;

	if (!jack_running || pending_shutdown || (jack_audio_client == NULL)) {
		return 0;
	}

	set_midi_cycle_time();
	if (midi_driver == MIDI_DRIVER_JACK) {
		jack_process_midi(nframes);
	}
	else if (check_active_sensing_timeout() > 0) {
		broadcast_notes_off();
	}

	out1 = jack_port_get_buffer(output_port1[0], nframes);
	out2 = jack_port_get_buffer(output_port2[0], nframes);

	memset((void *) out1, 0, nframes * sizeof(jack_default_audio_sample_t));
	memset((void *) out2, 0, nframes * sizeof(jack_default_audio_sample_t));

	a_index = get_audio_index();

	for (i = 0; i < MAX_PARTS; i++) {
		part = get_part(i);

		for (j = 0; j < nframes; j++) {
			out1[j] += (jack_default_audio_sample_t) part->output_buffer1[a_index + j];
			out2[j] += (jack_default_audio_sample_t) part->output_buffer2[a_index + j];
		}
	}

# ifdef ENABLE_INPUTS
	in1 = jack_port_get_buffer(input_port1, nframes);
	in2 = jack_port_get_buffer(input_port2, nframes);

	memcpy((void *) & (input_buffer1[a_index]), (void *) in1,
	       sizeof(jack_default_audio_sample_t) * nframes);
	memcpy((void *) & (input_buffer2[a_index]), (void *) in2,
	       sizeof(jack_default_audio_sample_t) * nframes);
# endif

	inc_audio_index(nframes);

	jack_process_transport(nframes);

	return 0;
}


/*****************************************************************************
 * jack_port_info_free()
 *****************************************************************************/
void
jack_port_info_free(JACK_PORT_INFO *portinfo, int follow)
{
	JACK_PORT_INFO  *cur = portinfo;
	JACK_PORT_INFO  *next;

	while (cur != NULL) {
		if (cur->name != NULL) {
			free(cur->name);
		}
		if (cur->type != NULL) {
			free(cur->type);
		}
		next = cur->next;
		free(cur);
		cur = next;
		if (!follow) {
			break;
		}
	}
}


/*****************************************************************************
 * jack_get_midi_port_list()
 *****************************************************************************/
JACK_PORT_INFO *
jack_get_midi_port_list(void)
{
	JACK_PORT_INFO  *head = NULL;
	JACK_PORT_INFO  *cur  = NULL;
	JACK_PORT_INFO  *new;
	jack_port_t     *port;
	const char      **port_names;
	int             port_num;

	jack_rebuilding_port_list = 1;

	/* build list of available midi ports */
	port_names = jack_get_ports(jack_audio_client, NULL, NULL, JackPortIsOutput);
	for (port_num = 0; port_names[port_num] != NULL; port_num++) {
		port = jack_port_by_name(jack_audio_client, port_names[port_num]);
		if (port && (jack_port_flags(port) & JackPortIsOutput) &&
		    (strcmp(jack_port_type(port), JACK_DEFAULT_MIDI_TYPE) == 0)) {
			if ((new = malloc(sizeof(JACK_PORT_INFO))) == NULL) {
				phasex_shutdown("Out of Memory!\n");
			}
			new->name      = strdup(jack_port_name(port));
			new->type      = strdup(JACK_DEFAULT_MIDI_TYPE);
			new->connected =
				((midi_input_port != NULL) ?
				 (jack_port_connected_to(midi_input_port, jack_port_name(port)) ? 1 : 0) : 0);
			new->connect_request    = 0;
			new->disconnect_request = 0;
			new->next               = NULL;
			if (head == NULL) {
				head = cur = new;
			}
			else {
				cur->next = new;
				cur = cur->next;
			}
		}
	}
	free(port_names);

	jack_rebuilding_port_list = 0;

	return head;
}


/*****************************************************************************
 * jack_client_registration_handler()
 *
 * Called when a jack client is registered or unregistered.
 * Rebuilds the list of available MIDI ports.
 *****************************************************************************/
void
jack_client_registration_handler(const char *name, int reg, void *UNUSED(arg))
{
	JACK_PORT_INFO  *cur;
	JACK_PORT_INFO  *prev = NULL;
	JACK_PORT_INFO  *head;
	char            client_match[64];

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK %sregistered client %s.\n", (reg ? "" : "un"), name);

	while (jack_rebuilding_port_list) {
		usleep(1000);
	}

	/* when a client unregisters, we need to manually remove entries from the list */
	if ((reg == 0) && (jack_midi_ports != NULL)) {
		snprintf(client_match, sizeof(client_match), "%s:", name);
		head = cur = jack_midi_ports;
		while (cur != NULL) {
			if ((cur->name != NULL) &&
			    (strncmp(cur->name, client_match, strlen(client_match)) == 0)) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "  Removing port '%s' from list.", cur->name);
				if (prev == NULL) {
					head = cur->next;
				}
				else {
					prev->next = cur->next;
				}
				jack_port_info_free(cur, 0);
			}
			prev = cur;
			cur = cur->next;
		}
		jack_midi_ports = head;
		jack_midi_ports_changed = 1;
	}
}


/*****************************************************************************
 * jack_port_registration_handler()
 *
 * Called when a jack client registers or unregisters a port.
 * Inserts or deletes port from phasex internal midi port list
 * depending on the port's new registration status.
 *
 * TODO:  Make sure list functions are thread-safe!
 *****************************************************************************/
void
jack_port_registration_handler(jack_port_id_t port_id, int reg, void *UNUSED(arg))
{
	JACK_PORT_INFO  *head      = NULL;
	JACK_PORT_INFO  *cur       = NULL;
	JACK_PORT_INFO  *prev      = NULL;
	JACK_PORT_INFO  *new;
	jack_port_t     *port      = jack_port_by_id(jack_audio_client, port_id);
	const char      *port_name = jack_port_name(port);
	int             flags      = jack_port_flags(port);

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK %sregistered port %d (%s)\n",
	             (reg ? "" : "un"), port_id, port_name);

	while (jack_rebuilding_port_list) {
		usleep(1000);
	}

	/* when a port unregisters, remove it from the list */
	if (reg == 0) {
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "  Port '%s' unregistered.\n", port_name);
		head = cur = jack_midi_ports;
		while (cur != NULL) {
			if (strcmp(cur->name, port_name) == 0) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "    Removing port '%s' from list.\n", port_name);
				if (prev == NULL) {
					head = cur->next;
				}
				else {
					prev->next = cur->next;
				}
				jack_port_info_free(cur, 0);
				break;
			}
			prev = cur;
			cur = cur->next;
		}
		jack_midi_ports = head;
		jack_midi_ports_changed = 1;
	}

	/* when a port registers, add it to the list */
	else if (!jack_port_is_mine(jack_audio_client, port) &&
	         (flags & JackPortIsOutput) &&
	         (strcmp(jack_port_type(port), JACK_DEFAULT_MIDI_TYPE) == 0)) {

		PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "  Port '%s' registered.  Adding to list.\n", port_name);
		if ((new = malloc(sizeof(JACK_PORT_INFO))) == NULL) {
			phasex_shutdown("Out of Memory!\n");
		}
		new->name      = strdup(port_name);
		new->type      = strdup(JACK_DEFAULT_MIDI_TYPE);
		new->connected = ((midi_input_port != NULL) ?
		                  (jack_port_connected_to(midi_input_port, port_name) ? 1 : 0) : 0);
		new->connect_request    = 0;
		new->disconnect_request = 0;
		new->next               = NULL;
		head = cur = jack_midi_ports;
		while (cur != NULL) {
			if (strcmp(cur->name, port_name) > 0) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "    Adding port '%s' to list before '%s'.\n",
				             port_name, cur->name);
				new->next = cur;
				if (prev == NULL) {
					head = new;
				}
				else {
					prev->next = new;
				}
				break;
			}
			prev = cur;
			cur = cur->next;
		}
		if (cur == NULL) {
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "    Adding port '%s' to end of list.\n", port_name);
			if (prev == NULL) {
				head = new;
			}
			else {
				prev->next = new;
			}
		}
		jack_midi_ports = head;
		jack_midi_ports_changed = 1;
	}
}


/*****************************************************************************
 * jack_port_connection_handler()
 *
 * Called when a jack client port connects or disconnects.
 * Sets connection status in the phasex internal jack port list.
 *****************************************************************************/
void
jack_port_connection_handler(jack_port_id_t a, jack_port_id_t b, int connect, void *UNUSED(arg))
{
	JACK_PORT_INFO  *cur    = NULL;
	jack_port_t     *port_a = jack_port_by_id(jack_audio_client, a);
	jack_port_t     *port_b = jack_port_by_id(jack_audio_client, b);

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK port %d (%s) %s port %d (%s)  connect=%d\n",
	             a, jack_port_name(port_a),
	             (connect ? "connected to" : "disconnected from"),
	             b, jack_port_name(port_b),
	             connect);

	while (jack_rebuilding_port_list) {
		usleep(1000);
	}

	if ((strcmp(jack_port_type(port_a), JACK_DEFAULT_MIDI_TYPE) == 0) &&
	    (jack_port_is_mine(jack_audio_client, port_a) ||
	     jack_port_is_mine(jack_audio_client, port_b))) {
		cur = jack_midi_ports;
		while (cur != NULL) {
			if ((strcmp(cur->name, jack_port_name(port_a)) == 0) ||
			    (strcmp(cur->name, jack_port_name(port_b)) == 0)) {
				cur->connected = connect;
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
				             "  setting connection status in list.  a=%d  b=%d  connect=%d\n",
				             a, b, connect);
			}
			cur = cur->next;
		}
	}
}


/*****************************************************************************
 * jack_port_rename_handler()
 *
 * Called when a jack client port is renamed.
 * Resets port's name in the phasex internal jack port list.
 *****************************************************************************/
int
jack_port_rename_handler(jack_port_id_t UNUSED(port),
                         const char     *old_name,
                         const char     *new_name,
                         void           *UNUSED(arg))
{
	JACK_PORT_INFO  *cur  = NULL;
	char            *old;

	while (jack_rebuilding_port_list) {
		usleep(1000);
	}

	cur = jack_midi_ports;
	while (cur != NULL) {
		if (strcmp(cur->name, old_name) == 0) {
			old = cur->name;
			cur->name = strdup(new_name);
			free(old);
		}
		cur = cur->next;
	}

	return 0;
}


/*****************************************************************************
 * jack_bufsize_handler()
 *
 * Called when jack sets or changes buffer size.
 *****************************************************************************/
int
jack_bufsize_handler(jack_nframes_t nframes, void *UNUSED(arg))
{
	/* Make sure buffer doesn't get overrun */
	if (nframes > PHASEX_MAX_BUFSIZE) {
		PHASEX_ERROR("JACK requested buffer size:  "
		             "%d (%d * %d periods).  Max is:  %d.\n",
		             (nframes * DEFAULT_BUFFER_PERIODS),
		             nframes,
		             DEFAULT_BUFFER_PERIODS,
		             PHASEX_MAX_BUFSIZE);
		phasex_shutdown("Buffer size exceeded.  Exiting...\n");
	}
	if ((unsigned int) buffer_period_size != nframes) {
		sample_rate_changed = 1;
	}
	buffer_periods     = DEFAULT_BUFFER_PERIODS;
	buffer_period_size = nframes;
	buffer_size        = nframes * buffer_periods;
	buffer_latency     = setting_buffer_latency * buffer_period_size;
	buffer_size_mask   = buffer_size - 1;
	buffer_period_mask = buffer_period_size - 1;

	init_buffer_indices(1);
	start_midi_clock();

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
	             "JACK requested buffer size:  %d (%d * %d periods, mask=0x%x)\n",
	             buffer_size, buffer_period_size, buffer_periods, buffer_size_mask);

	return 0;
}


/*****************************************************************************
 * jack_samplerate_handler()
 *
 * Called when jack sets or changes sample rate.
 *****************************************************************************/
int
jack_samplerate_handler(jack_nframes_t nframes, void *UNUSED(arg))
{
	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK requested sample rate:  %d\n", nframes);

	/* if jack requests a zero value, just use what we already have */
	if (nframes == 0) {
		return 0;
	}

	/* scale sample rate depending on mode */
	switch (setting_sample_rate_mode) {
	case SAMPLE_RATE_UNDERSAMPLE:
		nframes /= 2;
		break;
	case SAMPLE_RATE_NORMAL:
		break;
	case SAMPLE_RATE_OVERSAMPLE:
		nframes *= 2;
		break;
	}

	pthread_mutex_lock(&sample_rate_mutex);
	if (nframes == (unsigned int) sample_rate) {
		pthread_mutex_unlock(&sample_rate_mutex);
		return 0;
	}

	/* Changing JACK sample rate midstream not tested */
	if ((sample_rate > 0) && ((unsigned int) sample_rate != nframes)) {
		stop_audio();
	}

	/* First time setting sample rate */
	if (sample_rate == 0) {
		sample_rate    = (int) nframes;
		f_sample_rate  = (sample_t) sample_rate;
		nyquist_freq   = (sample_t)(sample_rate / 2);
		wave_period    = (sample_t)(F_WAVEFORM_SIZE / f_sample_rate);

		PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Internal sample rate:  %d\n", nframes);

		/* now that we have the sample rate, signal anyone else who
		   needs to know */
		pthread_cond_broadcast(&sample_rate_cond);
	}

	pthread_mutex_unlock(&sample_rate_mutex);
	return 0;
}


/*****************************************************************************
 * jack_shutdown_handler()
 *
 * Called when JACK shuts down or closes client.
 *****************************************************************************/
void
jack_shutdown_handler(void *UNUSED(arg))
{
	/* set state so client can be restarted */
	jack_running        = 0;
	jack_thread_p       = 0;
	jack_audio_client   = NULL;
	midi_input_port     = NULL;

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK shutdown handler called in client thread 0x%lx\n",
	             pthread_self());
}


/*****************************************************************************
 * jack_xrun_handler()
 *
 * Called when jack detects an xrun event.
 *****************************************************************************/
int
jack_xrun_handler(void *UNUSED(arg))
{
	init_buffer_indices(1);

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK xrun detected.\n");
	return 0;
}


/*****************************************************************************
 * jack_latency_handler()
 *
 * Called when jack sets or changes port latencies.  This should only be
 * necessary when inputs are enabled.
 *****************************************************************************/
#ifdef ENABLE_JACK_LATENCY_CALLBACK
void
jack_latency_handler(jack_latency_callback_mode_t mode, void *UNUSED(arg))
{
	jack_latency_range_t    range;
	jack_nframes_t          min_adj;
	jack_nframes_t          max_adj;
	int                     i;

	if (mode == JackCaptureLatency) {
#ifdef ENABLE_INPUTS
		min_adj = 0;
		max_adj = 0;
		jack_port_get_latency_range(input_port1, JackCaptureLatency, &range);
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
		             "Latency handler:  Input Channel 1:     "
		             "old:  range.min = %5d  range.max = %5d\n",
		             range.min, range.max);
		range.min += min_adj;
		range.max += max_adj;
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
		             "                                       "
		             "new:  range.min = %5d  range.max = %5d\n",
		             range.min, range.max);
		jack_port_set_latency_range(input_port1, JackCaptureLatency, &range);

		jack_port_get_latency_range(input_port2, JackCaptureLatency, &range);
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
		             "                  Input Channel 2:     "
		             "old:  range.min = %5d  range.max = %5d\n",
		             range.min, range.max);
		range.min += min_adj;
		range.max += max_adj;
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
		             "                                       "
		             "new:  range.min = %5d  range.max = %5d\n",
		             range.min, range.max);
		jack_port_set_latency_range(input_port2, JackCaptureLatency, &range);
#endif
	}

	else if (mode == JackPlaybackLatency) {
		min_adj = setting_buffer_latency * buffer_period_size;
		max_adj = setting_buffer_latency * buffer_period_size;
		for (i = 0; i < num_output_pairs; i++) {
			jack_port_get_latency_range(dest_port1[i], JackPlaybackLatency, &range);
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
			             "Latency handler:  Part %d:  Channel 1:  "
			             "old:  range.min = %5d  range.max = %5d\n",
			             i, range.min, range.max);
			range.min += min_adj;
			range.max += max_adj;
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
			             "                                       "
			             "new:  range.min = %5d  range.max = %5d\n",
			             range.min, range.max);
			jack_port_set_latency_range(output_port1[i], JackPlaybackLatency, &range);

			jack_port_get_latency_range(dest_port2[i], JackPlaybackLatency, &range);
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
			             "                           Channel 2:  "
			             "old:  range.min = %5d  range.max = %5d\n",
			             range.min, range.max);
			range.min += min_adj;
			range.max += max_adj;
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
			             "                                       "
			             "new:  range.min = %5d  range.max = %5d\n",
			             range.min, range.max);
			jack_port_set_latency_range(output_port2[i], JackPlaybackLatency, &range);
		}
	}
}
#endif /* ENABLE_JACK_LATENCY_CALLBACK */


/*****************************************************************************
 * jack_graph_order_handler()
 *
 * Called when jack sets or changes the graph order.  Rebuilds the list of
 * available MIDI ports.
 *****************************************************************************/
#ifdef ENABLE_JACK_GRAPH_ORDER_CALLBACK
int
jack_graph_order_handler(void *arg)
{
	JACK_PORT_INFO  *old_midi_ports = jack_midi_ports;
	JACK_PORT_INFO  *new_midi_ports;

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK graph order changed!\n");

	return 0;
}
#endif /* ENABLE_JACK_GRAPH_ORDER_CALLBACK */


/*****************************************************************************
 * jack_session_handler()
 *
 * Called when jack needs to save the session.
 *****************************************************************************/
#ifdef HAVE_JACK_SESSION_H
void
jack_session_handler(jack_session_event_t *event, void *UNUSED(arg))
{
	char                    cmd[256];

	snprintf(cmd, sizeof(cmd), "phasex -u %s -D %s",
	         event->client_uuid, event->session_dir);
	event->command_line = strdup(cmd);
	jack_session_reply(jack_audio_client, event);

	/* keep session event and let watchdog do the real work */
	jack_session_event = event;
}
#endif /* HAVE_JACK_SESSION_H */


/*****************************************************************************
 * jack_error_handler()
 *****************************************************************************/
void
jack_error_handler(const char *msg)
{
	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK Error:  %s\n", msg);
}


/*****************************************************************************
 * jack_audio_init()
 *
 * Initialize the jack client, register ports, and set callbacks.  All that
 * needs to be done before time to call jack_activate() should be done here.
 *****************************************************************************/
int
jack_audio_init(void)
{
	char            port_name[16];
	JACK_PORT_INFO  *cur;
	//const char      *server_name        = "default";
	jack_options_t  options             = JackNoStartServer | JackUseExactName;
	jack_status_t   client_status;
	int             pair_num;
	int             new_sample_rate;
	unsigned int    new_period_size;

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Initializing JACK client from thread 0x%lx\n", pthread_self());

	jack_set_error_function(jack_error_handler);

	if (setting_jack_multi_out) {
		num_output_pairs = MAX_PARTS;
	}
	else {
		num_output_pairs = 1;
	}
	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Number of JACK output pairs = %d\n", num_output_pairs);

	if (jack_audio_client != NULL) {
		PHASEX_WARN("Warning: closing stale JACK client...\n");
		jack_client_close(jack_audio_client);
		jack_audio_client = NULL;
	}

	/* open a client connection to the JACK server */
	PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
	             //"JACK server:  '%s':  "
	             "Opening JACK client '%s'...\n",
	             //server_name,
	             jack_audio_client_name);
	jack_audio_client = jack_client_open(jack_audio_client_name,
	                                     options,
	                                     &client_status); //, server_name);

	/* deal with non-unique client name */
	if (client_status & JackNameNotUnique) {
		PHASEX_ERROR("Unable to start JACK client '%s'!\n", jack_audio_client_name);
		return 1;
	}

	/* deal with jack server problems */
	if (client_status & (JackServerFailed | JackServerError)) {
		PHASEX_ERROR("Unable to connect to JACK server.  Status = 0x%2.0x\n", client_status);
		return 1;
	}

	/* deal with missing client */
	if (jack_audio_client == NULL) {
		PHASEX_ERROR("Unable to open JACK client.  Status = 0x%2.0x\n", client_status);
		return 1;
	}

	/* callback for when jack shuts down (needs to be set as early as possible) */
	jack_on_shutdown(jack_audio_client, jack_shutdown_handler, 0);

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Unique JACK client name '%s' assigned.\n",
	             jack_audio_client_name);

	/* notify once if we started jack server */
	if (client_status & JackServerStarted) {
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK server started.\n");
	}

	/* report realtime scheduling in JACK */
	if (debug) {
		if (jack_is_realtime(jack_audio_client)) {
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK is running with realtime scheduling.\n");
		}
		else {
			PHASEX_WARN("WARNING:  JACK is running without realtime scheduling.\n");
		}
	}

	/* get sample rate from jack */
	new_sample_rate = (int) jack_get_sample_rate(jack_audio_client);

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK sample rate:  %d\n", new_sample_rate);

	/* scale sample rate depending on mode */
	switch (setting_sample_rate_mode) {
	case SAMPLE_RATE_UNDERSAMPLE:
		new_sample_rate /= 2;
		break;
	case SAMPLE_RATE_OVERSAMPLE:
		new_sample_rate *= 2;
		break;
	}

	/* keep track of sample rate changes */
	if (sample_rate != new_sample_rate) {
		sample_rate = new_sample_rate;
		sample_rate_changed = 1;
	}

	/* set samplerate related vars */
	f_sample_rate = (sample_t) sample_rate;
	nyquist_freq  = (sample_t)(sample_rate / 2);
	wave_period   = (sample_t)(F_WAVEFORM_SIZE / (double) sample_rate);

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Internal sample rate:  %d\n", sample_rate);

	/* callback for setting our sample rate when jack tells us to */
	jack_set_sample_rate_callback(jack_audio_client, jack_samplerate_handler, 0);

	/* now that we have the sample rate, signal anyone else who needs to know */
	pthread_mutex_lock(&sample_rate_mutex);
	pthread_cond_broadcast(&sample_rate_cond);
	pthread_mutex_unlock(&sample_rate_mutex);

	/* get buffer size */
	new_period_size = jack_get_buffer_size(jack_audio_client);
	if (buffer_period_size != new_period_size) {
		buffer_period_size = new_period_size;
		sample_rate_changed = 1;
	}
	buffer_periods     = DEFAULT_BUFFER_PERIODS;
	buffer_size        = buffer_period_size * buffer_periods;
	buffer_latency     = setting_buffer_latency * buffer_period_size;
	buffer_size_mask   = buffer_size - 1;
	buffer_period_mask = buffer_period_size - 1;
	if (buffer_size > PHASEX_MAX_BUFSIZE) {
		PHASEX_WARN("JACK requested buffer size:  %d (%d * %d periods).  Max is:  %d.\n",
		            buffer_size, buffer_period_size, buffer_periods, PHASEX_MAX_BUFSIZE);
		PHASEX_ERROR("JACK buffer size exceeded.  Closing client...\n");
		jack_client_close(jack_audio_client);
		jack_audio_client  = NULL;
		jack_running = 0;
		return 1;
	}

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
	             "JACK audio buffer size:  %d (%d * %d periods, mask = 0x%x)\n",
	             buffer_size, buffer_period_size, buffer_periods, buffer_size_mask);

	/* create ports */
#ifdef ENABLE_INPUTS
	input_port1 = jack_port_register(jack_audio_client, "in_1",
	                                 JACK_DEFAULT_AUDIO_TYPE,
	                                 JackPortIsInput, 0);
	input_port2 = jack_port_register(jack_audio_client, "in_2",
	                                 JACK_DEFAULT_AUDIO_TYPE,
	                                 JackPortIsInput, 0);
#endif

	/* multi out mode uses part num in port name. */
	if (num_output_pairs > 1) {
		/* connect one pair of ports per part on a single client */
		for (pair_num = 0; pair_num < num_output_pairs; pair_num++) {
			snprintf(port_name, 9, "out_%d_L", (pair_num + 1));
			output_port1[pair_num] = jack_port_register(jack_audio_client, port_name,
			                                            JACK_DEFAULT_AUDIO_TYPE,
			                                            JackPortIsOutput | JackPortIsTerminal, 0);
			snprintf(port_name, 9, "out_%d_R", (pair_num + 1));
			output_port2[pair_num] = jack_port_register(jack_audio_client, port_name,
			                                            JACK_DEFAULT_AUDIO_TYPE,
			                                            JackPortIsOutput | JackPortIsTerminal, 0);
		}
	}
	/* Stereo out mode uses simple naming.  Different naming schemes for
	   stereo- and multi- out keep jack from getting them mixed up. */
	else {
		/* connect one pair of ports to one part (1:1 mapping) */
		output_port1[0] = jack_port_register(jack_audio_client, "out_L",
		                                     JACK_DEFAULT_AUDIO_TYPE,
		                                     JackPortIsOutput | JackPortIsTerminal, 0);
		output_port2[0] = jack_port_register(jack_audio_client, "out_R",
		                                     JACK_DEFAULT_AUDIO_TYPE,
		                                     JackPortIsOutput | JackPortIsTerminal, 0);
	}

	if ((output_port1[0] == NULL) || (output_port2[0] == NULL)) {
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK has no output ports available.\n");
		jack_client_close(jack_audio_client);
		jack_audio_client  = NULL;
		jack_running = 0;
		return 1;
	}

	/* register midi input port */
	if (midi_driver == MIDI_DRIVER_JACK) {
		midi_input_port = jack_port_register(jack_audio_client, "midi_in",
		                                     JACK_DEFAULT_MIDI_TYPE,
		                                     JackPortIsInput, 0);
	}

	/* build list of available midi ports */
	if (jack_midi_ports != NULL) {
		jack_port_info_free(jack_midi_ports, 1);
	}
	jack_midi_ports = jack_get_midi_port_list();

	if ((debug & DEBUG_CLASS_AUDIO) && (jack_midi_ports != NULL)) {
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Found JACK MIDI Ports:\n");
		cur = jack_midi_ports;
		while (cur != NULL) {
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "  %s\n", cur->name);
			cur = cur->next;
		}
	}

	/* set all callbacks needed for jack */
	if (setting_jack_multi_out) {
		jack_set_process_callback
			(jack_audio_client, jack_process_buffer_multi_out, (void *) NULL);
	}
	else {
		jack_set_process_callback
			(jack_audio_client, jack_process_buffer_stereo_out, (void *) NULL);
	}
	jack_set_buffer_size_callback
		(jack_audio_client, jack_bufsize_handler, 0);
	jack_set_xrun_callback
		(jack_audio_client, jack_xrun_handler, 0);
	jack_set_client_registration_callback
		(jack_audio_client, jack_client_registration_handler, (void *) NULL);
	jack_set_port_registration_callback
		(jack_audio_client, jack_port_registration_handler, (void *) NULL);
	jack_set_port_connect_callback
		(jack_audio_client, jack_port_connection_handler, (void *) NULL);
#ifdef HAVE_JACK_SET_PORT_RENAME_CALLBACK
	jack_set_port_rename_callback
		(jack_audio_client, jack_port_rename_handler, (void *) NULL);
#endif /* HAVE_JACK_SET_PORT_RENAME_CALLBACK */
#ifdef HAVE_JACK_SET_SESSION_CALLBACK
	if (jack_set_session_callback) {
		jack_set_session_callback
			(jack_audio_client, jack_session_handler, (void *) NULL);
	}
#endif /* HAVE_JACK_SET_SESSION_CALLBACK */
#if defined(HAVE_JACK_LATENCY_CALLBACK) && defined(ENABLE_JACK_LATENCY_CALLBACK)
	if (jack_set_latency_callback) {
		jack_set_latency_callback
			(jack_audio_client, jack_latency_handler, (void *) NULL);
	}
#endif /* HAVE_JACK_LATENCY_CALLBACK && ENABLE_JACK_LATENCY_CALLBACK */
#ifdef ENABLE_JACK_GRAPH_ORDER_CALLBACK
	jack_set_graph_order_callback(jack_audio_client,
	                              jack_graph_order_handler,
	                              (void *) NULL);
#endif /* ENABLE_JACK_GRAPH_ORDER_CALLBACK */

#ifndef WITHOUT_LASH
	if (!lash_disabled) {
		lash_client_set_jack_name(jack_audio_client);
	}
#endif

	return 0;
}


/*****************************************************************************
 * jack_start()
 *
 * Start jack client and attach to playback ports.
 *****************************************************************************/
int
jack_start(void)
{
	const char      **port_names;
	char            *p;
	char            *q;
	int             j;
	int             k;

	init_buffer_indices(0);
	start_midi_clock();

	/* activate client (callbacks start, so everything needs to be ready) */
	if (jack_activate(jack_audio_client)) {
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Unable to activate JACK client.\n");
		jack_client_close(jack_audio_client);
		jack_running        = 0;
		jack_thread_p       = 0;
		jack_audio_client   = NULL;
		midi_input_port     = NULL;
		return 1;
	}

	/* all up and running now */
	jack_running = 1;
	jack_thread_p = jack_client_thread_id(jack_audio_client);

	/* broadcast the audio ready condition */
	pthread_mutex_lock(&audio_ready_mutex);
	audio_ready = 1;
	pthread_cond_broadcast(&audio_ready_cond);
	pthread_mutex_unlock(&audio_ready_mutex);

	/* connect ports.  in/out is from server perspective */

	/* By default, connect PHASEX output to first two JACK hardware
	   playback ports found */
	if (setting_jack_autoconnect) {
		port_names = jack_get_ports(jack_audio_client, NULL, NULL,
		                            JackPortIsPhysical | JackPortIsInput);
		if ((port_names == NULL) || (port_names[0] == NULL)) {
			PHASEX_WARN("Warning:  PHASEX output not connected!\n"
			            "          (No physical JACK playback ports available.)\n");
		}
		else {
			for (k = 0; k < num_output_pairs; k++) {
				if (jack_connect(jack_audio_client,
				                 jack_port_name(output_port1[k]), port_names[0])) {
					PHASEX_WARN("Unable to connect %s output ports to JACK input.\n",
					            jack_audio_client_name);
				}
				else {
					dest_port1[k] = jack_port_by_name(jack_audio_client, port_names[0]);
					PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK connected %s to %s.\n",
					             jack_port_name(output_port1[k]), port_names[0]);
				}
				if (jack_connect(jack_audio_client,
				                 jack_port_name(output_port2[k]), port_names[1])) {
					PHASEX_WARN("Unable to connect %s output ports to JACK input.\n",
					            jack_audio_client_name);
				}
				else {
					dest_port2[k] = jack_port_by_name(jack_audio_client,
					                                  port_names[1]);
					PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK connected %s to %s.\n",
					             jack_port_name(output_port2[k]), port_names[1]);
				}
			}
		}
	}

	/* otherwise, connect to ports that match -o command line flag */
	else {
		port_names = jack_get_ports(jack_audio_client, NULL, NULL, JackPortIsInput);
		if ((port_names == NULL) || (port_names[0] == NULL)) {
			PHASEX_WARN("Warning:  PHASEX output not connected!\n"
			            "          (No JACK playback ports available.)\n");
		}
		else {
			p = audio_output_ports;
			if (p != NULL) {
				if ((q = index(p, ',')) != NULL) {
					*q = '\0';
					q++;
				}
				else {
					q = p;
				}
				for (j = 0; port_names[j] != NULL; j++) {
					if (strstr(port_names[j], p) != NULL) {
						for (k = 0; k < num_output_pairs; k++) {
							if (jack_connect(jack_audio_client,
							                 jack_port_name(output_port1[k]), port_names[j])) {
								PHASEX_WARN("Unable to connect %s output ports to JACK input.\n",
								            jack_audio_client_name);
							}
							else {
								dest_port1[0] =
									jack_port_by_name(jack_audio_client, port_names[j]);
								PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
								             "JACK connected %s:out_1 to %s.\n",
								             jack_audio_client_name, port_names[j]);
							}
						}
						p = "_no_port_";
					}
					if (strstr(port_names[j], q) != NULL) {
						for (k = 0; k < num_output_pairs; k++) {
							if (jack_connect(jack_audio_client,
							                 jack_port_name(output_port2[k]), port_names[j])) {
								PHASEX_WARN("Unable to connect %s output ports to JACK input.\n",
								            jack_audio_client_name);
							}
							else {
								dest_port2[0] =
									jack_port_by_name(jack_audio_client, port_names[j]);
								PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK connected %s:out_2 to %s.\n",
								             jack_audio_client_name, port_names[j]);
							}
						}
						q = "_no_port_";
					}
				}
				if (p != q) {
					q--;
					*q = ',';
				}
			}
		}
	}
	free(port_names);

	/* By default, connect PHASEX input to first two JACK hardware capture ports found */
#ifdef ENABLE_INPUTS
	if (setting_jack_autoconnect) {
		port_names = jack_get_ports(jack_audio_client, NULL, NULL,
		                            JackPortIsPhysical | JackPortIsOutput);
		if ((port_names == NULL) || (port_names[0] == NULL) || (port_names[1] == NULL)) {
			PHASEX_WARN("Warning:  PHASEX input not connected!\n"
			            "          (No physical JACK capture ports available.)\n");
		}
		else {
			if (jack_connect(jack_audio_client, port_names[0], jack_port_name(input_port1))) {
				PHASEX_WARN("Unable to connect %s input ports to JACK output.\n",
				            jack_audio_client_name);
			}
			if (jack_connect(jack_audio_client, port_names[1], jack_port_name(input_port2))) {
				PHASEX_WARN("Unable to connect %s input ports to JACK output.\n",
				            jack_audio_client_name);
			}
		}
	}

	/* otherwise, connect to ports that match -i command line flag */
	else {
		port_names = jack_get_ports(jack_audio_client, NULL, NULL, JackPortIsOutput);
		if ((port_names == NULL) ||
		    (port_names[0] == NULL) ||
		    (port_names[1] == NULL)) {
			PHASEX_WARN("Warning:  PHASEX input not connected!\n"
			            "          (No JACK capture ports available.)\n");
		}
		else {
			p = audio_input_ports;
			if (p != NULL) {
				if ((q = index(p, ',')) != NULL) {
					*q = '\0';
					q++;
				}
				else {
					q = p;
				}
				for (j = 0; port_names[j] != NULL; j++) {
					if (strstr(port_names[j], p) != NULL) {
						if (jack_connect(jack_audio_client,
						                 port_names[j], jack_port_name(input_port1))) {
							PHASEX_WARN("Unable to connect %s input ports to JACK output.\n",
							            jack_audio_client_name);
						}
						else if (debug) {
							PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK connected %s to %s:in_1.\n",
							             port_names[j], jack_audio_client_name);
						}
						p = "_no_port_";
					}
					if (strstr(port_names[j], q) != NULL) {
						if (jack_connect(jack_audio_client,
						                 port_names[j], jack_port_name(input_port2))) {
							PHASEX_WARN("Unable to connect %s input ports to JACK output.\n",
							            jack_audio_client_name);
						}
						else if (debug) {
							PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK connected %s to %s in_2.\n",
							             port_names[j], jack_audio_client_name);
						}
						q = "_no_port_";
					}
				}
			}
		}
	}
	free(port_names);
#endif

	return 0;
}


/*****************************************************************************
 * jack_stop()
 *
 * Closes the JACK client and cleans up internal state.
 * Unless a shutdown condition has been specified, this will cause the
 * watchdog loop to restart it.  To be called from other threads.
 *****************************************************************************/
int
jack_stop(void)
{
	jack_client_t   *tmp_client;

	if ((jack_audio_client != NULL) && jack_running && (jack_thread_p) != 0) {
		tmp_client          = jack_audio_client;
		jack_audio_client   = NULL;
		jack_thread_p       = 0;
		midi_input_port     = NULL;

#ifdef JACK_DEACTIVATE_BEFORE_CLOSE
		jack_deactivate(tmp_client);
#endif
		jack_client_close(tmp_client);
	}

	init_engine_buffers();
	init_buffer_indices(1);

	jack_running = 0;
	audio_stopped = 1;

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK stopped.  Client closed.\n");

	return 0;
}


/*****************************************************************************
 * jack_restart()
 *
 * Calls jack_stop() and hopes the audio watchdog will restart jack.
 *****************************************************************************/
void
jack_restart(void)
{
	jack_stop();
}


/*****************************************************************************
 * jack_get_session_name_from_directory()
 *****************************************************************************/
char *
jack_get_session_name_from_directory(const char *directory)
{
	SESSION *session = get_current_session();
	char    *dir;
	char    *p;
	char    *q;
	int     slashes;

	dir = strdup(directory);
	p = dir;
	while (*p != '\0') {
		p++;
	}
	p--;
	if (*p == '/') {
		*p-- = '\0';
	}
	slashes = 2;
	while (slashes > 0) {
		if ((*p == '/') && (* (p + 1) != '.')) {
			slashes--;
		}
		p--;
		if (p < dir) {
			break;
		}
	}
	p++;
	if (slashes == 0) {
		q = ++p;
		while (*p != '/') {
			p++;
		}
		*p = '\0';
		if (session->name == NULL) {
			session->name = strdup(q);
		}
		else {
			p = session->name;
			session->name = strdup(q);
			free(p);
		}
		session_name_changed = 1;
	}
	free(dir);

	return session->name;
}


/*****************************************************************************
 * jack_watchdog_cycle()
 *
 * Called by the audio watchdog to handle (dis)connect requests.
 *****************************************************************************/
void
jack_watchdog_cycle(void)
{
	JACK_PORT_INFO  *cur;
	char            *name;
	int             save_and_quit = 0;

	if (buffer_latency != (setting_buffer_latency * buffer_period_size)) {
		buffer_latency = setting_buffer_latency * buffer_period_size;
		jack_recompute_total_latencies(jack_audio_client);
	}
#ifdef HAVE_JACK_SESSION_H
	if (jack_session_event != NULL) {
		switch(jack_session_event->type) {
		case JackSessionSaveAndQuit:
			save_and_quit = 1;
		case JackSessionSave:
		case JackSessionSaveTemplate:
			save_session((char *)(jack_session_event->session_dir), visible_sess_num, 1);
			name = jack_get_session_name_from_directory(jack_session_event->session_dir);
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "JACK Saved session '%s'\n", name);
			jack_session_event_free(jack_session_event);
			jack_session_event = NULL;
		}
		if (save_and_quit) {
			phasex_shutdown("Saved JACK Session.  Goodbye!\n");
		}
	}
#endif /* HAVE_JACK_SESSION_H */
	if ((midi_driver == MIDI_DRIVER_JACK) && (jack_midi_ports != NULL)) {
		cur = jack_midi_ports;
		while (cur != NULL) {
			if (cur->connect_request) {
				jack_connect(jack_audio_client, cur->name, jack_port_name(midi_input_port));
				if (jack_port_connected_to(midi_input_port, cur->name)) {
					cur->connected          = 1;
					cur->connect_request    = 0;
					cur->disconnect_request = 0;
				}
				else {
					cur->connected          = 0;
					cur->connect_request    = 0;
					cur->disconnect_request = 0;
				}
			}
			else if (cur->disconnect_request) {
				jack_disconnect(jack_audio_client, cur->name, jack_port_name(midi_input_port));
				if (jack_port_connected_to(midi_input_port, cur->name)) {
					cur->connected          = 1;
					cur->connect_request    = 0;
					cur->disconnect_request = 1;
				}
				else {
					cur->connected = 0;
					cur->connect_request    = 0;
					cur->disconnect_request = 0;
				}
			}
			cur = cur->next;
		}
	}
}


/*****************************************************************************
 * jack_audio_thread()
 *****************************************************************************/
void *
jack_audio_thread(void *UNUSED(arg))
{
	struct sched_param  schedparam;
	pthread_t           thread_id;

	/* set realtime scheduling and priority */
	thread_id = pthread_self();
	memset(&schedparam, 0, sizeof(struct sched_param));
	schedparam.sched_priority = setting_audio_priority;
	pthread_setschedparam(thread_id, setting_sched_policy, &schedparam);

	/* setup thread cleanup handler */
	//pthread_cleanup_push (&alsa_pcm_cleanup, NULL);

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Starting JACK AUDIO thread...\n");

	/* broadcast the audio ready condition */
	pthread_mutex_lock(&audio_ready_mutex);
	audio_ready = 1;
	pthread_cond_broadcast(&audio_ready_cond);
	pthread_mutex_unlock(&audio_ready_mutex);

	/* initialize buffer indices and set reference clock. */
	init_buffer_indices(1);
	start_midi_clock();

	jack_start();

	while (!audio_stopped && !pending_shutdown) {
		usleep(125000);
	}

	/* execute cleanup handler and remove it */
	//pthread_cleanup_pop (1);

	/* end of MIDI thread */
	pthread_exit(NULL);
	return NULL;
}
