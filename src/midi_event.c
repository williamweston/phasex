/*****************************************************************************
 *
 * midi_event.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012 William Weston <whw@linuxmail.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <asoundlib.h>
#include <glib.h>
#include "phasex.h"
#include "timekeeping.h"
#include "buffer.h"
#include "mididefs.h"
#include "midi_event.h"
#include "engine.h"
#include "patch.h"
#include "param.h"
#include "filter.h"
#include "bank.h"
#include "settings.h"
#include "debug.h"
#include "driver.h"


MIDI_EVENT      realtime_events[MAX_PARTS];

unsigned int    bulk_event_index = 0;


/*****************************************************************************
 * get_midi_event_state()
 *****************************************************************************/
int
get_midi_event_state(MIDI_EVENT *event)
{
	return event->state;
}


/*****************************************************************************
 * set_midi_event_state()
 *****************************************************************************/
void
set_midi_event_state(MIDI_EVENT *event, int state)
{
	event->state = state;
}


/*****************************************************************************
 * init_midi_event_queue()
 *****************************************************************************/
void
init_midi_event_queue(unsigned int part_num)
{
	PART            *part = get_part(part_num);
	MIDI_EVENT      *event;
	unsigned int    e;

	memset(& (part->event_queue[0]), 0, sizeof(MIDI_EVENT) * MIDI_EVENT_POOL_SIZE);
	memset(& (part->bulk_queue[0]),  0, sizeof(MIDI_EVENT) * MIDI_EVENT_POOL_SIZE);
	memset(& (realtime_events[0]),   0, sizeof(MIDI_EVENT) * MAX_PARTS);

	for (e = 0; e < MAX_PARTS; e++) {
		event          = & (realtime_events[e]);
		event->type    = MIDI_EVENT_NO_EVENT;
		event->channel = 0x7F;
		event->byte2   = 0;
		event->byte3   = 0;
		event->next    = NULL;
		set_midi_event_state(event, EVENT_STATE_FREE);
	}
	for (e = 0; e < MIDI_EVENT_POOL_SIZE; e++) {
		event = & (part->event_queue[e]);
		event->type    = MIDI_EVENT_NO_EVENT;
		event->channel = 0x7F;
		event->byte2   = 0;
		event->byte3   = 0;
		event->next    = NULL;
		set_midi_event_state(event, EVENT_STATE_FREE);
	}
	for (e = 0; e < MIDI_EVENT_POOL_SIZE; e++) {
		event          = & (part->bulk_queue[e]);
		event->type    = MIDI_EVENT_NO_EVENT;
		event->channel = 0x7F;
		event->byte2   = 0;
		event->byte3   = 0;
		event->next    = NULL;
		set_midi_event_state(event, EVENT_STATE_FREE);
	}
	bulk_event_index = 0;
}


/*****************************************************************************
 * queue_midi_event()
 *****************************************************************************/
void
queue_midi_event(unsigned int part_num,
                 MIDI_EVENT   *event,
                 unsigned int cycle_frame,
                 unsigned int index)
{
	MIDI_EVENT      *queue_event = NULL;
	MIDI_EVENT      *bulk_event  = NULL;
	PART            *part        = get_part(part_num);
	int             queued       = 0;
	unsigned int    j;

	if (cycle_frame >= buffer_period_size) {
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT, "%%%%%%%%%%  Timing Error:  "
		             "cycle_frame=%d >= buffer_period_size=%d  (adjusting)  %%%%%%%%%%\n",
		             cycle_frame, buffer_period_size);
		cycle_frame = buffer_period_size - 1;
	}

	/* check main queue for free event and copy the event */
	for (j = 0; j < buffer_period_size; j++) {
		queue_event = & (part->event_queue[index + j]);
		if ((get_midi_event_state(queue_event)) == EVENT_STATE_FREE) {
			memcpy(queue_event, event, sizeof(MIDI_EVENT));
			queue_event->frame = (int) cycle_frame;
			queue_event->next  = NULL;
			set_midi_event_state(queue_event, (int) cycle_frame);
			queued = 1;
			break;
		}
	}

	/* if main queue is full for this cycle, link in event from the bulk pool */
	if (!queued) {
		bulk_event = queue_event->next;
		while (bulk_event != NULL) {
			queue_event = bulk_event;
			bulk_event  = bulk_event->next;
		}
		bulk_event = & (part->bulk_queue[bulk_event_index]);
		bulk_event_index = (bulk_event_index + 1) & MIDI_EVENT_POOL_MASK;
		if ((get_midi_event_state(bulk_event)) == EVENT_STATE_FREE) {
			memcpy(bulk_event, event, sizeof(MIDI_EVENT));
			bulk_event->frame = (int) cycle_frame;
			bulk_event->next  = NULL;
			set_midi_event_state(bulk_event, (int) cycle_frame);
			queue_event->next = bulk_event;
			queued = 1;
		}
	}

	if (!queued) {
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT, "*** queue_midi_event():  Queue full!  "
		             "Dropping event state=%d type=0x%02x byte2=0x%02x byte3=0x%02x\n",
		             get_midi_event_state(event), event->type, event->byte2, event->byte3);
	}

	if (check_active_sensing_timeout() != 0) {
		set_active_sensing_timeout();
	}
}


/*****************************************************************************
 * queue_midi_realtime_event()
 *
 * Queues a realtime MIDI event based on event type.  Called when processing
 * MIDI data streams with interleaved MIDI realtime messages.
 *****************************************************************************/
void
queue_midi_realtime_event(unsigned int  part_num,
                          unsigned char type,
                          unsigned int  cycle_frame,
                          unsigned int  index)
{
	MIDI_EVENT      *event = & (realtime_events[part_num]);
	unsigned int    i;

	if (type == MIDI_EVENT_ACTIVE_SENSING) {
		set_active_sensing_timeout();
	}
	else {
		event->state   = EVENT_STATE_ALLOCATED;
		event->type    = type;
		event->channel = 0x7F;
		event->byte2   = 0;
		event->byte3   = 0;

		if (part_num == ALL_PARTS) {
			for (i = 0; i < MAX_PARTS; i++) {
				queue_midi_event(i, event, cycle_frame, index);
			}
		}
		else {
			queue_midi_event(part_num, event, cycle_frame, index);
		}
	}
}
