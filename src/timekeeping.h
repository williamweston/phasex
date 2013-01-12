/*****************************************************************************
 *
 * timekeeping.h
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
#ifndef _TIMEKEEPING_H_
#define _TIMEKEEPING_H_

#include <time.h>
#include <glib.h>


#define PHASEX_CLOCK_INIT           1111111111


#if (ARCH_BITS == 32)

typedef float timecalc_t;


struct phasex_timestamp {
	int                         sec;
	int                         nsec;
} __attribute__((packed));
typedef struct phasex_timestamp PHASEX_TIMESTAMP;


typedef union atomic_timestamp {
	PHASEX_TIMESTAMP            timestamp;
} ATOMIC_TIMESTAMP;


extern volatile ATOMIC_TIMESTAMP    midi_clock_time[8];
extern volatile ATOMIC_TIMESTAMP    active_sensing_timeout[8];
extern volatile gint                midi_clock_time_index;
extern volatile gint                active_sensing_timeout_index;


#endif /* (ARCH_BITS == 32) */


#if (ARCH_BITS == 64)


typedef double timecalc_t;


struct phasex_timestamp {
	int                         sec;
	int                         nsec;
} __attribute__((packed));
typedef struct phasex_timestamp PHASEX_TIMESTAMP;


typedef union atomic_timestamp {
	gpointer                    gptr;
	PHASEX_TIMESTAMP            timestamp;
} ATOMIC_TIMESTAMP __attribute__((__transparent_union__));


extern volatile ATOMIC_TIMESTAMP    midi_ref_marker;
extern volatile ATOMIC_TIMESTAMP    active_sensing_timeout;


#endif /* (ARCH_BITS == 64) */

extern clockid_t        midi_clockid;

extern timecalc_t       nsec_per_frame;
extern timecalc_t       nsec_per_period;
extern timecalc_t       f_buffer_period_size;

extern volatile gint    need_increment;

extern timecalc_t       audio_phase_lock;
extern timecalc_t       audio_phase_min;
extern timecalc_t       audio_phase_max;


void start_midi_clock(void);
timecalc_t get_time_delta(struct timespec *now);
guint inc_midi_index(void);
void set_midi_cycle_time(void);
unsigned int get_midi_cycle_frame(timecalc_t delta_nsec);
void set_active_sensing_timeout(void);
int check_active_sensing_timeout(void);


#endif /* _TIMEKEEPING_H_ */
