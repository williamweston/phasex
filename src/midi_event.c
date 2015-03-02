/*****************************************************************************
 *
 * midi_event.c
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


MIDI_EVENT              realtime_events[MAX_PARTS];

volatile gint           bulk_event_index = 0;


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
		event->state   = EVENT_STATE_FREE;
	}
	for (e = 0; e < MIDI_EVENT_POOL_SIZE; e++) {
		event = & (part->event_queue[e]);
		event->type    = MIDI_EVENT_NO_EVENT;
		event->channel = 0x7F;
		event->byte2   = 0;
		event->byte3   = 0;
		event->next    = NULL;
		event->state   = EVENT_STATE_FREE;
	}
	for (e = 0; e < MIDI_EVENT_POOL_SIZE; e++) {
		event          = & (part->bulk_queue[e]);
		event->type    = MIDI_EVENT_NO_EVENT;
		event->channel = 0x7F;
		event->byte2   = 0;
		event->byte3   = 0;
		event->next    = NULL;
		event->state   = EVENT_STATE_FREE;
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
	guint           old_bulk_index;
	guint           new_bulk_index;

	if (cycle_frame >= buffer_period_size) {
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT, "%%%%%%%%%%  Timing Error:  "
		             "cycle_frame=%d >= buffer_period_size=%d  (adjusting)  %%%%%%%%%%\n",
		             cycle_frame, buffer_period_size);
		cycle_frame = buffer_period_size - 1;
	}

	/* check main queue for free event and copy the event */
	queue_event = & (part->event_queue[index + cycle_frame]);
	if (g_atomic_int_compare_and_exchange(&(queue_event->state),
	                                      EVENT_STATE_FREE, EVENT_STATE_ALLOCATED)) {
		memcpy(queue_event, event, sizeof(MIDI_EVENT));
		queue_event->frame = (int) cycle_frame;
		queue_event->next  = NULL;
		queued = 1;
	}

	/* if main queue is full for this cycle, link in event from the bulk pool */
	if (!queued) {
		bulk_event = queue_event->next;
		while (bulk_event != NULL) {
			queue_event = bulk_event;
			bulk_event  = bulk_event->next;
		}
		do {
			old_bulk_index = (guint)bulk_event_index;
			new_bulk_index = (old_bulk_index + 1) & MIDI_EVENT_POOL_MASK;
		} while (!g_atomic_int_compare_and_exchange(&bulk_event_index,
		                                            (gint)old_bulk_index, (gint)new_bulk_index));
		bulk_event = & (part->bulk_queue[old_bulk_index]);
		if (g_atomic_int_compare_and_exchange(&(bulk_event->state),
		                                      EVENT_STATE_FREE, EVENT_STATE_ALLOCATED)) {
			memcpy(bulk_event, event, sizeof(MIDI_EVENT));
			bulk_event->frame = (int)cycle_frame;
			bulk_event->next  = NULL;
			while (!g_atomic_pointer_compare_and_exchange(&(queue_event->gnext),
			                                              NULL, bulk_event)) {
				queue_event = queue_event->next;
			}
			queued = 1;
		}
	}

	if (!queued) {
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT, "*** queue_midi_event():  Queue full!  "
		             "Dropping event state=%d type=0x%02x byte2=0x%02x byte3=0x%02x\n",
		             event->state, event->type, event->byte2, event->byte3);
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


/*****************************************************************************
 * queue_midi_param_event()
 *****************************************************************************/
void
queue_midi_param_event(unsigned int part_num, unsigned int id, int cc_val)
{
	MIDI_EVENT          queue_event;
	struct timespec     now;
	timecalc_t          delta_nsec;
	unsigned int        cycle_frame  = 0;
	unsigned int        m_index;

	/* timestamp and queue event for engine */
	delta_nsec  = get_time_delta(&now);
	cycle_frame = get_midi_cycle_frame(delta_nsec);
	m_index     = get_midi_index();
	if (id == PARAM_BPM) {
		queue_event.type        = MIDI_EVENT_BPM_CHANGE;
		queue_event.float_value = (sample_t)(cc_val + 64);
	}
	else {
		queue_event.type        = MIDI_EVENT_PARAMETER;
		queue_event.state       = EVENT_STATE_ALLOCATED;
		queue_event.parameter   = (unsigned char)id;
		queue_event.value       = (unsigned char)cc_val;
	}
	queue_midi_event(part_num, &queue_event, cycle_frame, m_index);
	PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
	             DEBUG_COLOR_CYAN "[%d] " DEBUG_COLOR_DEFAULT,
	             (m_index / buffer_period_size));
}
