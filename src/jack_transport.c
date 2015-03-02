/*****************************************************************************
 *
 * jack_transport.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2010 Anton Kormakov <assault64@gmail.com>
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
#include <jack/jack.h>
#include "phasex.h"
#include "jack_transport.h"
#include "jack.h"
#include "timekeeping.h"
#include "settings.h"
#include "buffer.h"
#include "engine.h"
#include "mididefs.h"
#include "midi_event.h"
#include "midi_process.h"
#include "debug.h"


jack_position_t         jack_pos;
jack_transport_state_t  jack_state         = JackTransportStopped;
jack_transport_state_t  jack_prev_state    = JackTransportStopped;

int                     current_frame      = 0;
jack_nframes_t          prev_frame         = 0;

//int                   frames_per_tick;
//int                   frames_per_beat;

int                     phase_correction;
int                     need_resync;

MIDI_EVENT              transport_events;


/*****************************************************************************
 * jack_process_transport()
 *
 * Run once per client per JACK process cycle, processes JACK Transport
 * changes, and queues pseudo MIDI events to shut notes off on stop, and to
 * resync oscillators, lfos, and delay/chorus buffer positions on start.
 *****************************************************************************/
void
jack_process_transport(jack_nframes_t nframes)
{
	MIDI_EVENT      *event              = & (transport_events);
	unsigned int    part_num;
	unsigned int    index               = get_midi_index();
	static int      bpm_adjust_counter  = 256;

	if (jack_audio_client == NULL) {
		return;
	}

	/* get current jack transport state */
	jack_prev_state = jack_state;
	jack_state = jack_transport_query(jack_audio_client, &jack_pos);

	/* if transport has stopped, queue up an all notes off. */
	if ((jack_state == JackTransportStopped) &&
	    (jack_prev_state != JackTransportStopped)) {
		PHASEX_DEBUG(DEBUG_CLASS_JACK_TRANSPORT, "+++ Transport Stopped! +++\n");
		queue_midi_realtime_event(ALL_PARTS, MIDI_EVENT_STOP, 0, index);
	}

	if (!pending_shutdown && (jack_audio_client != NULL) &&
	    (setting_jack_transport_mode != JACK_TRANSPORT_OFF)) {

		/* reinit sync vars if transport is just started */
		if ((jack_prev_state == JackTransportStopped) &&
		         (jack_state == JackTransportStarting)) {
			PHASEX_DEBUG(DEBUG_CLASS_JACK_TRANSPORT, "+++ Transport Starting! +++\n");
			need_resync = 1;
		}
		if (jack_state == JackTransportRolling) {
			if (need_resync) {
				prev_frame = jack_pos.frame - nframes;
				//frames_per_beat = sample_rate / global.bps;
				//frames_per_tick = sample_rate /
				//      (jack_pos.ticks_per_beat * global.bps);
				current_frame = 0;
				need_resync = 0;
			}

			/* Handle BPM change */
			if ((jack_pos.beats_per_minute > 1.0) && (global.bpm != jack_pos.beats_per_minute)) {
				event->type        = MIDI_EVENT_BPM_CHANGE;
				event->state       = 0;
				event->channel     = 0xFF;
				event->byte2       = 0;
				event->byte3       = 0;
				event->float_value = (sample_t) jack_pos.beats_per_minute;
				event->next        = NULL;

				if (!--bpm_adjust_counter) {
					PHASEX_DEBUG(DEBUG_CLASS_JACK_TRANSPORT,
					             "+++ Transport adjusting PHASEX tempo:  "
					             "old BPS = %lf  new BPM = %lf  "
					             "(last 999 msgs suppressed).\n",
					             global.bps, event->float_value);
					bpm_adjust_counter = 1000;
				}

				for (part_num = 0; part_num < MAX_PARTS; part_num++) {
					queue_midi_event(part_num, event, 0, index);
				}
			}

			/* frame-based sync */
			current_frame = (int)(jack_pos.frame - prev_frame);
			prev_frame = jack_pos.frame;

			/* whoooaaaa, we're traveling through time! */
			phase_correction = current_frame - (int) nframes;

			/* queue up a pseudo message to trigger the actual sync */
			if (phase_correction && (setting_jack_transport_mode == JACK_TRANSPORT_TNP)) {
				PHASEX_DEBUG(DEBUG_CLASS_JACK_TRANSPORT,
				             "+++ PHASEX out of sync with Transport.  "
				             "Phase correction:  %d +++\n",
				             phase_correction);

				event->type        = MIDI_EVENT_PHASE_SYNC;
				event->state       = 0;
				event->channel     = 0xFF;
				event->byte2       = (unsigned char)(phase_correction & 0xFF);
				event->byte3       = 0;
				event->float_value = 0.0;
				event->next        = NULL;

				/* queue for the end slot of this cycle in case */
				/* note on messages were already queued. */
				for (part_num = 0; part_num < MAX_PARTS; part_num++) {
					queue_midi_event(part_num, event, 0, index);
				}

				phase_correction = 0;
			}
		}
	}
}
