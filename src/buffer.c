/*****************************************************************************
 *
 * buffers.c
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
#include <errno.h>
#include <glib.h>
#include "phasex.h"
#include "buffer.h"
#include "timekeeping.h"
#include "driver.h"
#include "debug.h"


unsigned int    buffer_size         = PHASEX_MAX_BUFSIZE;
unsigned int    buffer_size_mask    = (PHASEX_MAX_BUFSIZE - 1);
unsigned int    buffer_periods      = DEFAULT_BUFFER_PERIODS;
unsigned int    buffer_period_size  = DEFAULT_BUFFER_PERIOD_SIZE;
unsigned int    buffer_period_mask  = DEFAULT_BUFFER_PERIOD_SIZE - 1;
unsigned int    buffer_latency      = DEFAULT_BUFFER_PERIOD_SIZE;
unsigned int    buffer_size_bits;
unsigned int    buffer_period_size_bits;

volatile gint   audio_index;
volatile gint   midi_index;
volatile gint   engine_index;

int             need_index_resync[MAX_PARTS];


/*****************************************************************************
 * init_buffer_indices()
 *****************************************************************************/
void
init_buffer_indices(int resync)
{
	int             part_num;

	g_atomic_int_set(&need_increment, 0);
	set_audio_index(buffer_size - buffer_latency);
	set_engine_index(0);
	set_midi_index(0);
	if (resync) {
		for (part_num = 0; part_num < MAX_PARTS; part_num++) {
			need_index_resync[part_num] = 1;
		}
	}
}


/*****************************************************************************
 *
 * MIDI Buffer Index Synchronization:
 *
 *****************************************************************************/

/*****************************************************************************
 * test_midi_index()
 *
 * Atomically reads midi_write_index and tests against supplied
 * value for write_index.
 *****************************************************************************/
unsigned int
test_midi_index(unsigned int val)
{
	volatile gint   *addr = &midi_index;
	return (g_atomic_int_get(addr) == (gint) val);
}


/*****************************************************************************
 * get_midi_index()
 *
 * Atomically reads midi_write_index.
 *****************************************************************************/
unsigned int
get_midi_index(void)
{
	volatile gint   *addr = &midi_index;
	return (unsigned int) g_atomic_int_get(addr);
}


/*****************************************************************************
 * set_midi_index()
 *  unsigned int    val
 *
 * Atomically sets midi_write_index to <val>.
 *****************************************************************************/
void
set_midi_index(unsigned int val)
{
	volatile gint   *addr = &midi_index;
	g_atomic_int_set(addr, (gint) val);
}


/*****************************************************************************
 *
 * Engine Buffer Index Synchronization:
 *
 *****************************************************************************/

/*****************************************************************************
 * get_engine_index()
 *
 * Atomically reads engine_index.
 *****************************************************************************/
unsigned int
get_engine_index(void)
{
	volatile gint   *addr = &engine_index;
	return (unsigned int) g_atomic_int_get(addr);
}


/*****************************************************************************
 * set_engine_index()
 *  unsigned int    val
 *
 * Atomically sets engine_index to <val>.
 *****************************************************************************/
void
set_engine_index(unsigned int val)
{
	volatile gint   *addr = &engine_index;
	g_atomic_int_set(addr, (gint) val);
}


/*****************************************************************************
 *
 * Audio Buffer Index Synchronization:
 *
 *****************************************************************************/

/*****************************************************************************
 * test_audio_index()
 *
 * Atomically reads audio_index and tests against supplied
 * value for write_index.  (currently not used).
 *****************************************************************************/
unsigned int
test_audio_index(unsigned int val)
{
	volatile gint   *addr = &audio_index;
	return ((((unsigned int) g_atomic_int_get(addr) + buffer_latency) & buffer_size_mask) == val);
}

/*****************************************************************************
 * get_audio_index()
 *
 * Atomically reads audio_index.
 *****************************************************************************/
unsigned int
get_audio_index(void)
{
	volatile gint   *addr = &audio_index;
	return (unsigned int) g_atomic_int_get(addr);
}

/*****************************************************************************
 * get_audio_index()
 *
 * Atomically increments audio_index by the supplied number of frames.
 *****************************************************************************/
void
inc_audio_index(unsigned int nframes)
{
	volatile gint   *addr = &audio_index;
	guint           old_read_index;
	guint           new_read_index;
	old_read_index = (guint) g_atomic_int_get(addr);
	new_read_index = ((old_read_index + nframes) & buffer_size_mask);
	g_atomic_int_set(addr, (gint) new_read_index);
}

/*****************************************************************************
 * set_engine_index()
 *  unsigned int    val
 *
 * Atomically sets audio_index to <val>.
 *****************************************************************************/
void
set_audio_index(unsigned int val)
{
	volatile gint   *addr = &audio_index;
	g_atomic_int_set(addr, (gint) val);
}
