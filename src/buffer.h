/*****************************************************************************
 *
 * buffers.h
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
#ifndef _PHASEX_BUFFER_H_
#define _PHASEX_BUFFER_H_

#include <glib.h>
#include "phasex.h"


extern unsigned int     buffer_size;
extern unsigned int     buffer_size_mask;
extern unsigned int     buffer_latency;
extern unsigned int     buffer_periods;
extern unsigned int     buffer_period_size;
extern unsigned int     buffer_period_mask;

extern volatile gint    new_buffer_period_size;
extern volatile gint    audio_index;
extern volatile gint    midi_index;

extern int              need_index_resync[MAX_PARTS];


void init_buffer_indices(int resync);

unsigned int test_midi_index(unsigned int val);
unsigned int get_midi_index(void);
void set_midi_index(unsigned int val);

unsigned int test_engine_index(unsigned int val);
unsigned int get_engine_index(void);

unsigned int test_midi_rx_index(unsigned int val);
unsigned int get_midi_rx_index(void);

unsigned int get_audio_index(void);
void inc_audio_index(unsigned int nframes);
void set_audio_index(unsigned int val);


#endif /* _PHASEX_BUFFER_H_ */
