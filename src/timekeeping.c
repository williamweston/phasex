/*****************************************************************************
 *
 * timekeeping.c
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
#include "settings.h"
#include "debug.h"
#include "driver.h"


#if (ARCH_BITS == 32)

volatile ATOMIC_TIMESTAMP   midi_clock_time[8];
volatile ATOMIC_TIMESTAMP   active_sensing_timeout[8];
volatile gint               midi_clock_time_index;
volatile gint               active_sensing_timeout_index;

#endif /* (ARCH_BITS == 32) */


#if (ARCH_BITS == 64)

volatile ATOMIC_TIMESTAMP   midi_timeref;
volatile ATOMIC_TIMESTAMP   active_sensing_timeout;

#endif /* (ARCH_BITS == 64) */


clockid_t                   midi_clockid          = -1;

timecalc_t                  nsec_per_frame        = (1000000000.0 / 44100.0);
timecalc_t                  nsec_per_period       = (1000000000.0 / 44100.0) * 256.0;

timecalc_t                  f_buffer_period_size  = 256.0;

struct timespec             audio_start_time      = { 0, PHASEX_CLOCK_INIT };

volatile gint               need_increment        = 0;
volatile gint               last_cycle_frame      = 0;

timecalc_t                  audio_phase_lock      = 252.0;
timecalc_t                  audio_phase_min       = 1.0;
timecalc_t                  audio_phase_max       = 255.0;


/*****************************************************************************
 * set_audio_phase_lock()
 *****************************************************************************/
void
set_audio_phase_lock(void)
{
	if ((setting_audio_phase_lock <= 0.00390625) ||
	    (setting_audio_phase_lock >= 0.99609375)) {
		setting_audio_phase_lock = DEFAULT_AUDIO_PHASE_LOCK;
	}

	audio_phase_lock = (timecalc_t)(setting_audio_phase_lock * f_buffer_period_size);
	if (audio_phase_lock > 2.0) {
		audio_phase_min  = audio_phase_lock - 2.0;
	}
	else {
		audio_phase_min  = 0.0;
	}
	if (audio_phase_lock < (f_buffer_period_size - 2.0)) {
		audio_phase_max  = audio_phase_lock + 2.0;
	}
	else {
		audio_phase_max  = (f_buffer_period_size - 2.0);
	}
}


/*****************************************************************************
 * start_midi_clock()
 *
 * Initializes MIDI timing variables based on variables set during audio
 * initilization (buffer size, sample rate, etc.).
 *****************************************************************************/
void
start_midi_clock(void)
{
#ifdef HAVE_CLOCK_GETTIME
	struct timespec     now;
#else
	struct timeval      now;
#endif
#if (ARCH_BITS == 64)
	ATOMIC_TIMESTAMP    new_timeref;
#endif

	g_atomic_int_set(&need_increment, 0);

	audio_start_time.tv_sec  = 0;
	audio_start_time.tv_nsec = PHASEX_CLOCK_INIT;

	f_buffer_period_size = (timecalc_t) buffer_period_size;

	nsec_per_period  = f_buffer_period_size * 1000000000.0 / f_sample_rate;
	if ((setting_clock_constant > 0.8) && (setting_clock_constant < 1.25)) {
		nsec_per_period *= setting_clock_constant;
		PHASEX_DEBUG((DEBUG_CLASS_INIT | DEBUG_CLASS_MIDI_TIMING),
		             "Using previously calculated clock_constant=%0.23f.\n",
		             setting_clock_constant);
	}
	nsec_per_frame   = nsec_per_period / f_buffer_period_size;

	set_audio_phase_lock();

#ifdef HAVE_CLOCK_GETTIME

# ifdef CLOCK_MONOTONIC
	if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
		midi_clockid = CLOCK_MONOTONIC;
	}
# endif
# ifdef CLOCK_MONITONIC_HR
	if (clock_gettime(CLOCK_MONOTONIC_HR, &now) == 0) {
		midi_clockid = CLOCK_MONOTONIC_HR;
	}
# endif
# ifdef CLOCK_MONOTONIC_RAW
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &now) == 0) {
		midi_clockid = CLOCK_MONOTONIC_RAW;
	}
# endif

	/* now set the reference timestamp itself. */
	if (clock_gettime(midi_clockid, &now) == 0) {
# if (ARCH_BITS == 32)
		midi_clock_time[0].timestamp.sec  = now.tv_sec;
		midi_clock_time[0].timestamp.nsec = now.tv_nsec;
		g_atomic_int_set(&midi_clock_time_index, 0);
# endif
# if (ARCH_BITS == 64)
		new_timeref.timestamp.sec  = (int) now.tv_sec;
		new_timeref.timestamp.nsec = (int) now.tv_nsec;
		g_atomic_pointer_set(& (midi_timeref.gptr), new_timeref.gptr);
# endif
	}

#else /* !HAVE_CLOCK_GETTIME */

	if (gettimeofday(&now, NULL) == 0) {
# if (ARCH_BITS == 32)
		midi_clock_time[0].timestamp.sec  = now.tv_sec;
		midi_clock_time[0].timestamp.nsec = now.tv_usec * 1000;
		g_atomic_int_set(&midi_clock_time_index, 0);
# endif
# if (ARCH_BITS == 64)
		new_timeref.timestamp.sec  = now.tv_sec;
		new_timeref.timestamp.nsec = now.tv_nsec * 1000;
		g_atomic_pointer_set(& (midi_timeref.gptr), new_timeref.gptr);
# endif
	}

#endif /* !HAVE_CLOCK_GETTIME */

	/* initialize the active sensing timeout to zero (active sensing off). */
#if (ARCH_BITS == 32)
	active_sensing_timeout[0].timestamp.sec  = 0;
	active_sensing_timeout[0].timestamp.nsec = 0;
	g_atomic_int_set(&active_sensing_timeout_index, 0);
# endif
# if (ARCH_BITS == 64)
	new_timeref.timestamp.sec  = 0;
	new_timeref.timestamp.nsec = 0;
	g_atomic_pointer_set(& (active_sensing_timeout.gptr), new_timeref.gptr);
# endif

}


/*****************************************************************************
 * get_time_delta()
 *
 * The timestamp delta is time elapsed since the reference time if positive,
 * or time until the reference time if negative.  This delta is used by
 * audio thread to calculate wakeup time, and by the MIDI thread to calculate
 * frame position.
 *****************************************************************************/
timecalc_t
get_time_delta(struct timespec *now)
{
	ATOMIC_TIMESTAMP    last_timeref;
#ifndef HAVE_CLOCK_GETTIME
	struct timeval      walltime;
#endif
#if (ARCH_BITS == 32)
	int                 c_index;
#endif

	if (
#ifdef HAVE_CLOCK_GETTIME
	    clock_gettime(midi_clockid, now)
#else
	    gettimeofday(&walltime, NULL)
#endif
	    == 0) {
#ifndef HAVE_CLOCK_GETTIME
		now->tv_sec  = walltime.tv_sec;
		now->tv_nsec = walltime.tv_usec * 1000;
#endif
#if (ARCH_BITS == 32)
		c_index = g_atomic_int_get(&midi_clock_time_index);
		last_timeref.timestamp.sec  = midi_clock_time[c_index].timestamp.sec;
		last_timeref.timestamp.nsec = midi_clock_time[c_index].timestamp.nsec;
#endif
#if (ARCH_BITS == 64)
		last_timeref.gptr = g_atomic_pointer_get(& (midi_timeref.gptr));
#endif
		return (timecalc_t)(((now->tv_sec -
		                      last_timeref.timestamp.sec) * 1000000000) +
		                    (now->tv_nsec - last_timeref.timestamp.nsec));
	}

	PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "clock_gettime() failed!\n");
	return 0.0;
}


/*****************************************************************************
 * inc_midi_index ()
 *
 * Increments the midi index the first time this fuction is called within the
 * current midi period.  This guarantees that all midi events for a single
 * MIDI period use the same queue index.  For midi periods with no midi events
 * occurring before the audio wakes up, the audio thread will be the first
 * caller.  This guarantees that the midi index is incremented exactly once
 * per midi period.
 *****************************************************************************/
guint
inc_midi_index(void)
{
	guint       old_midi_index;
	guint       new_midi_index  = buffer_periods;

	if (g_atomic_int_compare_and_exchange(&need_increment, 1, 0)) {
		do {
			old_midi_index = (guint) g_atomic_int_get(&midi_index);
			new_midi_index = (old_midi_index + buffer_period_size) & buffer_size_mask;
		}
		while (!g_atomic_int_compare_and_exchange(&midi_index,
		                                          (gint) old_midi_index,
		                                          (gint) new_midi_index));
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
		             DEBUG_COLOR_ORANGE ": " DEBUG_COLOR_DEFAULT);
		return new_midi_index;
	}

	return (guint) g_atomic_int_get(&midi_index);
}


/*****************************************************************************
 * set_midi_cycle_time()
 *
 * This function sets the midi period time reference to a regular cycle, using
 * the timing of when this function is called as an incoming clock pulse to
 * generate an absolutely steady phase locked time reference for determining
 * the frame position within the audio buffer of incoming MIDI events.  This
 * function is to be called exactly once for every audio buffer period, and
 * can be called at any time within the processing period.  Basically, this is
 * a fancy software PLL with the incoming clock pulse handled by one thread
 * while another thread checks to see when the stable output clock pulse
 * _would_ have fired, times its events, and updates its buffer index
 * accordingly.  Timing jitter of when this function is called is not a
 * problem as long as the average period time remains relatively stable.  We
 * just need to remember that in the absense of xruns, we assume that an audio
 * processing period is never actually late, just later than we expected.  It
 * can only be early (and always less than 1 full period early).
 *****************************************************************************/
void
set_midi_cycle_time(void)
{
	ATOMIC_TIMESTAMP        next_timeref;
	ATOMIC_TIMESTAMP        timeref;
	PHASEX_TIMESTAMP        last;
	timecalc_t              delta_nsec;
	timecalc_t              avg_period_nsec     = nsec_per_period;
	static int              cycle_frame;
	static int              last_cycle_frame;
#if (ARCH_BITS == 32)
	int                     c_index;
#endif

	last.sec  = (int) audio_start_time.tv_sec;
	last.nsec = (int) audio_start_time.tv_nsec;

	delta_nsec = get_time_delta(&audio_start_time);

	inc_midi_index();

	/* Delay between start_midi_clock() and first call to this
	   function is not always determinate, so check for clock init
	   and set timestamp here. */
	if (last.nsec == PHASEX_CLOCK_INIT) {
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
		             DEBUG_COLOR_YELLOW "!!! Clock Start !!! " DEBUG_COLOR_DEFAULT);
		/* set initial timeref to match target audio wakeup phase. */
		delta_nsec = nsec_per_frame * (timecalc_t)(audio_phase_lock);
#if (ARCH_BITS == 32)
		midi_clock_time[0].timestamp.sec   = (int) audio_start_time.tv_sec;
		midi_clock_time[0].timestamp.nsec  = (int) audio_start_time.tv_nsec;
		midi_clock_time[0].timestamp.nsec -= (int) delta_nsec;
		g_atomic_int_set(&midi_clock_time_index, 0);
#endif
#if (ARCH_BITS == 64)
		next_timeref.timestamp.sec   = (int) audio_start_time.tv_sec;
		next_timeref.timestamp.nsec  = (int) audio_start_time.tv_nsec;
		next_timeref.timestamp.nsec -= (int)(delta_nsec);
		g_atomic_pointer_set(& (midi_timeref.gptr), next_timeref.gptr);
#endif
	}

	/* handle the normal case. */
	else {
		avg_period_nsec  -= (avg_period_nsec / 2048.0);
		/* handle system clock wrapping around. */
		if ((audio_start_time.tv_sec == 0) && (last.sec != 0)) {
			avg_period_nsec += ((timecalc_t)(1000000000 +
			                                 audio_start_time.tv_nsec -
			                                 last.nsec) / 2048.0);
		}
		else {
			avg_period_nsec += ((timecalc_t)(((audio_start_time.tv_sec -
			                                   last.sec) * 1000000000) +
			                                 (audio_start_time.tv_nsec -
			                                  last.nsec)) / 2048.0);
		}
		nsec_per_period = avg_period_nsec;
		nsec_per_frame  = (nsec_per_period / f_buffer_period_size);
	}
#if (ARCH_BITS == 32)
	c_index = g_atomic_int_get(&midi_clock_time_index);
	timeref.timestamp.sec   = midi_clock_time[c_index].timestamp.sec;
	timeref.timestamp.nsec  = midi_clock_time[c_index].timestamp.nsec;
	next_timeref.timestamp.sec  = timeref.timestamp.sec;
	next_timeref.timestamp.nsec = timeref.timestamp.nsec;
#endif
#if (ARCH_BITS == 64)
	timeref.gptr      = g_atomic_pointer_get(& (midi_timeref.gptr));
	next_timeref.gptr = timeref.gptr;
#endif
	last_cycle_frame = cycle_frame;
	cycle_frame = (int)(delta_nsec / nsec_per_frame);
	/* Latch the clock when audio wakes up before the calculated
	   midi period start. coming in one frame too early is
	   unfortunately common with non-rt kernels or missing
	   realtime priveleges.  allow for an extra 2 frames. */
	if (delta_nsec < (nsec_per_frame + nsec_per_frame)) {
		next_timeref.timestamp.sec  = (int) audio_start_time.tv_sec;
		next_timeref.timestamp.nsec = (int) audio_start_time.tv_nsec;
		next_timeref.timestamp.nsec -= ((int)(nsec_per_period -
		                                      (nsec_per_frame * (f_buffer_period_size -
		                                                         audio_phase_lock))));
		/* Nudge nsec_per_period in the right direction to
		   speed up clock settling time. */
		nsec_per_period -= (0.015625 * nsec_per_frame);
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
		             DEBUG_COLOR_YELLOW "---|||%d||| " DEBUG_COLOR_DEFAULT,
		             cycle_frame);
	}
	/* Half frame jitter correction for audio waking up too early,
	   but within the current midi period. */
	else if (delta_nsec < (nsec_per_frame * audio_phase_min)) {
		next_timeref.timestamp.nsec -= ((int)(nsec_per_frame * 0.5));
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
		             DEBUG_COLOR_CYAN "-<<" DEBUG_COLOR_LTBLUE "%d"
		             DEBUG_COLOR_CYAN "<< " DEBUG_COLOR_DEFAULT,
		             cycle_frame);
	}
	/* This condition is reached when the phase is locked. */
	else if (delta_nsec < (nsec_per_frame * audio_phase_max)) {
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
		             DEBUG_COLOR_LTBLUE "%d " DEBUG_COLOR_DEFAULT,
		             cycle_frame);
	}
	/* Half frame jitter correction for audio waking up too late,
	   but within the current midi period. coming in one frame too
	   early is unfortunately common with non-rt kernels or
	   missing realtime priveleges.  Allow for an extra frame as
	   well as single isolated late wakeups. */
	else if ((delta_nsec < (nsec_per_period + nsec_per_frame)) &&
	         (last_cycle_frame < f_buffer_period_size)) {
		next_timeref.timestamp.nsec += ((int)(nsec_per_frame * 0.5));
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
		             DEBUG_COLOR_CYAN "+>>" DEBUG_COLOR_LTBLUE "%d"
		             DEBUG_COLOR_CYAN ">> " DEBUG_COLOR_DEFAULT,
		             cycle_frame);
	}
	/* Latch the clock when audio wakes up after the calculated
	   midi period end. */
	else {
		next_timeref.timestamp.nsec += (int)(delta_nsec - (nsec_per_frame * audio_phase_lock));
		/* Nudge nsec_per_period in the right direction to
		   speed up clock settling time. */
		nsec_per_period += (0.015625 * nsec_per_frame);
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
		             DEBUG_COLOR_YELLOW "+++|||%d||| " DEBUG_COLOR_DEFAULT,
		             cycle_frame);
	}
	/* Advance the timeref by one period */
	next_timeref.timestamp.nsec += (int)(nsec_per_period);
	if (next_timeref.timestamp.nsec >= 1000000000) {
		next_timeref.timestamp.nsec -= 1000000000;
		next_timeref.timestamp.sec  = (next_timeref.timestamp.sec + 1);
	}
	else if (next_timeref.timestamp.nsec < 0) {
		next_timeref.timestamp.nsec += 1000000000;
		next_timeref.timestamp.sec  = (next_timeref.timestamp.sec - 1);
	}
#if (ARCH_BITS == 32)
	c_index = (c_index + 1) & 0x7;
	midi_clock_time[c_index].timestamp.sec  = next_timeref.timestamp.sec;
	midi_clock_time[c_index].timestamp.nsec = next_timeref.timestamp.nsec;
	g_atomic_int_add(&need_increment, 1);
	g_atomic_int_set(&midi_clock_time_index, c_index);
#endif
#if (ARCH_BITS == 64)
	g_atomic_int_add(&need_increment, 1);
	g_atomic_pointer_set(& (midi_timeref.gptr), next_timeref.gptr);
#endif
}


/*****************************************************************************
 * get_midi_cycle_frame()
 *
 * Returns the number of MIDI frames into the current audio/MIDI processing
 * period.
 *****************************************************************************/
unsigned int
get_midi_cycle_frame(timecalc_t delta_nsec)
{
	int         cycle_frame = 0;

	if (delta_nsec >= 0.0) {
		inc_midi_index();
		cycle_frame = (int)(((delta_nsec * f_buffer_period_size) -
		                     (nsec_per_frame)) / (nsec_per_period + nsec_per_frame));
		if (cycle_frame <= 1) {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
			             DEBUG_COLOR_YELLOW "-(%d)- " DEBUG_COLOR_DEFAULT,
			             cycle_frame);
		}
		else {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
			             DEBUG_COLOR_CYAN "-(%d)- " DEBUG_COLOR_DEFAULT,
			             cycle_frame);
		}
	}
	else {
		cycle_frame = (int) buffer_period_size + (int)(delta_nsec / nsec_per_frame);
		if (cycle_frame >= (int)(buffer_period_size - 1)) {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
			             DEBUG_COLOR_YELLOW "={%d}= " DEBUG_COLOR_DEFAULT,
			             cycle_frame);
		}
		else {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
			             DEBUG_COLOR_CYAN "={%d}= " DEBUG_COLOR_DEFAULT,
			             cycle_frame);
		}
	}

	/* calculated frame position is beyond current period. */
	if (cycle_frame < 0) {
		phasex_shutdown("*********** cycle_frame < 0 ***********");
	}
	if (cycle_frame >= (int) buffer_period_size) {
		cycle_frame = (int) buffer_period_size - 1;
	}
	return (unsigned int) cycle_frame;
}


/*****************************************************************************
 * set_active_sensing_timeout()
 *
 * Called whenever an active sensing message is received (or when any other
 * message is received less than 300ms after an active sensing message).
 *****************************************************************************/
void
set_active_sensing_timeout(void)
{
	ATOMIC_TIMESTAMP    new_timeout;
#ifndef HAVE_CLOCK_GETTIME
	struct timeval      walltime;
#endif
	struct timespec     now;
#if (ARCH_BITS == 32)
	int                 t_index     = g_atomic_int_get(&active_sensing_timeout_index);
	int                 next_index  = (t_index + 1) & 0x7;
#endif

	if (
#ifdef HAVE_CLOCK_GETTIME
	    clock_gettime(midi_clockid, &now)
#else
	    gettimeofday(&walltime, NULL)
#endif
	    == 0) {
#ifndef HAVE_CLOCK_GETTIME
		now.tv_sec  = walltime.tv_sec;
		now.tv_nsec = walltime.tv_usec * 1000;
#endif
		new_timeout.timestamp.sec  = (int) now.tv_sec;
		new_timeout.timestamp.nsec = (int) now.tv_nsec;
		new_timeout.timestamp.nsec += 300000000;
		if (new_timeout.timestamp.nsec >= 1000000000) {
			new_timeout.timestamp.sec  += 1;
			new_timeout.timestamp.nsec -= 1000000000;
		}
#if (ARCH_BITS == 32)
		active_sensing_timeout[next_index].timestamp.sec  = new_timeout.timestamp.sec;
		active_sensing_timeout[next_index].timestamp.nsec = new_timeout.timestamp.nsec;
		g_atomic_int_set(&active_sensing_timeout_index, next_index);
#endif
#if (ARCH_BITS == 64)
		g_atomic_pointer_set(& (active_sensing_timeout.gptr), new_timeout.gptr);
#endif
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
		             DEBUG_COLOR_MAGENTA "---AAAAA--- " DEBUG_COLOR_DEFAULT);
	}
}


/*****************************************************************************
 * check_active_sensing_timeout()
 *****************************************************************************/
int
check_active_sensing_timeout(void)
{
	ATOMIC_TIMESTAMP    timeout;
	ATOMIC_TIMESTAMP    midi_clock;
#if (ARCH_BITS == 32)
	int                 t_index = g_atomic_int_get(&active_sensing_timeout_index);
	int                 next_index = (t_index + 1) & 0x7;

	timeout.timestamp.sec  = active_sensing_timeout[t_index].timestamp.sec;
	timeout.timestamp.nsec = active_sensing_timeout[t_index].timestamp.nsec;
#endif
#if (ARCH_BITS == 64)
	timeout.gptr = g_atomic_pointer_get(& (active_sensing_timeout.gptr));
#endif
	if ((timeout.timestamp.sec != 0) && (timeout.timestamp.nsec != 0)) {
#if (ARCH_BITS == 32)
		t_index = g_atomic_int_get(&midi_clock_time_index);
		midi_clock.timestamp.sec  = midi_clock_time[t_index].timestamp.sec;
		midi_clock.timestamp.nsec = midi_clock_time[t_index].timestamp.nsec;
#endif
#if (ARCH_BITS == 64)
		midi_clock.gptr = g_atomic_pointer_get(& (midi_timeref.gptr));
#endif
		if ((timeout.timestamp.sec < midi_clock.timestamp.sec) ||
		    ((timeout.timestamp.sec == midi_clock.timestamp.sec) &&
		     (timeout.timestamp.nsec < midi_clock.timestamp.nsec))) {
#if (ARCH_BITS == 32)
			active_sensing_timeout[next_index].timestamp.sec  = 0;
			active_sensing_timeout[next_index].timestamp.nsec = 0;
			g_atomic_int_set(&active_sensing_timeout_index, next_index);
#endif
#if (ARCH_BITS == 64)
			timeout.timestamp.sec  = 0;
			timeout.timestamp.nsec = 0;
			g_atomic_pointer_set(& (active_sensing_timeout.gptr), timeout.gptr);
#endif
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
			             DEBUG_COLOR_MAGENTA "---ZZZZZ--- " DEBUG_COLOR_DEFAULT);
			return 1;
		}
		return -1;
	}
	return 0;
}
