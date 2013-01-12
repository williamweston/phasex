/*****************************************************************************
 *
 * wave.h
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
#ifndef _PHASEX_WAVE_H_
#define _PHASEX_WAVE_H_

#include "phasex.h"


/* hardcoded waveform numbers (should match the functions) */
#define WAVE_SINE           0
#define WAVE_TRIANGLE       1
#define WAVE_SAW            2
#define WAVE_REVSAW         3
#define WAVE_SQUARE         4
#define WAVE_STAIR          5
#define WAVE_SAW_S          6
#define WAVE_REVSAW_S       7
#define WAVE_SQUARE_S       8
#define WAVE_STAIR_S        9
#define WAVE_JUNO_OSC       10
#define WAVE_JUNO_SAW       11
#define WAVE_JUNO_SQUARE    12
#define WAVE_JUNO_POLY      13
#define WAVE_ANALOG_SQUARE  14
#define WAVE_VOX_1          15
#define WAVE_VOX_2          16
#define WAVE_POLY_SINE      17
#define WAVE_POLY_SAW       18
#define WAVE_POLY_REVSAW    19
#define WAVE_POLY_SQUARE_1  20
#define WAVE_POLY_SQUARE_2  21
#define WAVE_POLY_1         22
#define WAVE_POLY_2         23
#define WAVE_POLY_3         24
#define WAVE_POLY_4         25
#define WAVE_NULL           26
#define WAVE_IDENTITY       27


/* lookup macro for exponential frequency scaling */
#define halfsteps_to_freq_mult(val)                                     \
	(freq_shift_table[(int)(((val)*F_TUNING_RESOLUTION)+FREQ_SHIFT_ZERO_OFFSET)])


/* lookup tables */
extern sample_t freq_shift_table[FREQ_SHIFT_TABLE_SIZE];
extern sample_t osc_table[NUM_WAVEFORMS][WAVEFORM_SIZE + 4];
extern sample_t freq_table[128][648];
extern sample_t keyfollow_table[128][128];
extern sample_t mix_table[128];
extern sample_t pan_table[128];
extern sample_t gain_table[128];
extern sample_t velocity_gain_table[128][128];
extern sample_t env_curve[ENV_CURVE_SIZE + 1];
extern int      env_interval_dur[6][128];
extern int      env_table[128];

/* global base tuning frequency */
extern double   a4freq;


void build_freq_table(void);
void build_freq_shift_table(void);
void build_waveform_tables(void);
void build_env_tables(void);
void build_mix_table(void);
void build_pan_table(void);
void build_gain_table(void);
void build_velocity_gain_table(void);
void build_keyfollow_table(void);

#ifdef ENABLE_SAMPLE_LOADING
int load_waveform_sample(int wavenum, int num_cycles, double octaves, sample_t scale);
#endif /* ENABLE_SAMPLE_LOADING */

#ifdef NEED_GENERIC_HERMITE
sample_t hermite(sample_t *buf, unsigned int max, sample_t sample_index);
#endif
sample_t chorus_hermite(sample_t *buf, sample_t sample_index);
sample_t osc_table_hermite(int wave_num, sample_t sample_index);
sample_t osc_table_linear(int wave_num, sample_t sample_index);


/* these are the functions for building the initial waveforms */
/* these should _only_ be used in the startup code */
sample_t func_square(unsigned int sample_num);
sample_t func_saw(unsigned int sample_num);
sample_t func_revsaw(unsigned int sample_num);
sample_t func_saw_s(unsigned int sample_num);
sample_t func_revsaw_s(unsigned int sample_num);
sample_t func_stair(unsigned int sample_num);
sample_t func_triangle(unsigned int sample_num);


#endif /* _PHASEX_WAVE_H_ */
