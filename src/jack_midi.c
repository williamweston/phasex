/*****************************************************************************
 *
 * jack_midi.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012-2013 William Weston <whw@linuxmail.org>
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include "phasex.h"
#include "timekeeping.h"
#include "buffer.h"
#include "jack.h"
#include "jack_midi.h"
#include "midi_event.h"
#include "midi_process.h"
#include "engine.h"
#include "settings.h"
#include "debug.h"


MIDI_EVENT  output_events;


/*****************************************************************************
 * jack_process_midi()
 *
 * called by jack_process_buffer() or jack_engine_thread()
 *****************************************************************************/
void
jack_process_midi(jack_nframes_t nframes)
{
	PART                *part;
	MIDI_EVENT          *out_event  = & (output_events);
	void                *port_buf   = jack_port_get_buffer(midi_input_port, nframes);
	jack_midi_event_t   in_event;
	jack_nframes_t      num_events  = jack_midi_get_event_count(port_buf);
	unsigned char       type        = MIDI_EVENT_NO_EVENT;
	unsigned char       channel;
	unsigned short      e;
	unsigned short      part_num;
	unsigned int        m_index;

	out_event->state = EVENT_STATE_ALLOCATED;
	out_event->next  = NULL;

	/* JACK midi event cycles match audio buffer period processing cycles, so
	   update the midi index now instead of waiting for the midi clock.  JACK
	   midi does not need the clock per se, but currently the engine is
	   synchronized to the midi, so it's easier to work with the existing
	   system of synchronizing and updating buffer indices. */
	m_index = get_midi_index();

	/* handle all events for this process cycle */
	for (e = 0; e < num_events; e++) {
		jack_midi_event_get(&in_event, port_buf, e);
		/* handle messages with channel number embedded in the first byte */
		if (* (in_event.buffer) < 0xF0) {
			type    = * (in_event.buffer) & 0xF0;
			channel = * (in_event.buffer) & 0x0F;
			out_event->byte2   = (unsigned char) * (in_event.buffer + 1);
			out_event->type    = type;
			out_event->channel = channel;
			/* all channel specific messages except program change and
			   polypressure have 2 bytes following status byte */
			if ((type != 0xC0) && (type != 0xD0)) {
				out_event->byte3 = * (in_event.buffer + 2);
			}
			else {
				out_event->byte3 = 0x00;
			}
			/* queue event for all parts listening to the incoming channel. */
			for (part_num = 0; part_num < MAX_PARTS; part_num++) {
				part = get_part(part_num);
				if ((channel == part->midi_channel) || (part->midi_channel == 16)) {
					queue_midi_event(part_num, out_event, in_event.time, m_index);
				}
			}
		}
		/* handle other messages (sysex / clock / automation / etc) */
		else {
			type = * (in_event.buffer);
			PHASEX_DEBUG(DEBUG_CLASS_MIDI,
			             "+++ jack_process_midi() received midi system message "
			             "type 0x%02x\n", type);

			switch (type) {
			case MIDI_EVENT_SYSEX:          // 0xF0
				/* ignore sysex messages for now */
				break;
				/* 3 byte system messages */
			case MIDI_EVENT_SONGPOS:        // 0xF2
				break;
				/* 2 byte system messages */
			case MIDI_EVENT_MTC_QFRAME:     // 0xF1
			case MIDI_EVENT_SONG_SELECT:    // 0xF3
				//out_event->byte2 = *(in_event.buffer + 1);
				//out_event->byte3 = *(in_event.buffer + 2);
				//queue_midi_event (part_num, out_event, in_event.time);
				break;
				/* 1 byte realtime messages */
			case MIDI_EVENT_STOP:           // 0xFC
			case MIDI_EVENT_SYSTEM_RESET:   // 0xFF
				/* send stop and reset events to all queues */
				queue_midi_realtime_event(ALL_PARTS, type, in_event.time, m_index);
				break;
				/* ignored 1-byte system and realtime messages */
			case MIDI_EVENT_BUS_SELECT:     // 0xF5
			case MIDI_EVENT_TUNE_REQUEST:   // 0xF6
			case MIDI_EVENT_END_SYSEX:      // 0xF7
			case MIDI_EVENT_TICK:           // 0xF8
			case MIDI_EVENT_START:          // 0xFA
			case MIDI_EVENT_CONTINUE:       // 0xFB
				break;
			case MIDI_EVENT_ACTIVE_SENSING: // 0xFE
				set_active_sensing_timeout();
				break;
			default:
				break;
			}
		}
	}

	/* now that all events for this cycle are handled, check for an active
	   sensing timeout. */
	if (check_active_sensing_timeout() > 0) {
		/* a real timeout has occurred when there are _no_ midi events. */
		if (num_events == 0) {
			queue_midi_realtime_event(ALL_PARTS, MIDI_EVENT_STOP, (nframes - 1), m_index);
		}
	}

	/* All events are processed. Engine can start now. */
	inc_midi_index();
}
