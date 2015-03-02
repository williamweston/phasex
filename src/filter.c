/*****************************************************************************
 *
 * filter.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2008,2012-2015 Willaim Weston <william.h.weston@gmail.com>
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
#include <math.h>
#include "wave.h"
#include "engine.h"
#include "patch.h"
#include "filter.h"
#include "midi_process.h"
#include "debug.h"


sample_t    filter_res[NUM_FILTER_TYPES][128];
sample_t    filter_table[TUNING_RESOLUTION * 648];
sample_t    filter_dist_1[32];
sample_t    filter_dist_2[32];
sample_t    filter_dist_3[32];
sample_t    filter_dist_4[32];
sample_t    filter_dist_5[32];
sample_t    filter_dist_6[32];

int         filter_limit = 1;


/*****************************************************************************
 * build_filter_tables()
 *
 * Builds tables with filter saturation, dampening, and resonance curves
 *****************************************************************************/
void
build_filter_tables(void)
{
	int         j;
	int         k;
	double      step;
	double      freq;
	double      x;
	sample_t    a;
	sample_t    b;
	sample_t    c;

	/* steps between halfstep set by F_TUNING_RESOLUTION */
	step = exp(log(2.0) / (12.0 * F_TUNING_RESOLUTION));

	/* saturation and resonance curves */

	/* Chamberlin filter uses a linear curve for resonance. */
	for (j = 0; j < 128; j++) {
		filter_res[FILTER_TYPE_DIST][j]  = 1.0 - ((sample_t)(j) / 128.0);
		filter_res[FILTER_TYPE_RETRO][j] = 1.0 - ((sample_t)(j) / 128.0);
	}

	/* TODO: Add Moog filter's resonance curve to filter_res[][] here. */

	/* build the high res table for fine filter adjustmentss */
	for (j = 0; j < 648; j++) {
		freq = freq_table[64][j];
		for (k = j * TUNING_RESOLUTION; k < ((j + 1) * TUNING_RESOLUTION); k++) {
			x = M_PI * 2.0 * freq / (f_sample_rate * F_FILTER_OVERSAMPLE);
			filter_table[k]   = (sample_t) sin(x);
			if (freq < nyquist_freq) {
				filter_limit = k;
			}
			freq *= step;
		}
	}

	/* build distortion value tables to be used in filters. */
	a = 1.0;
	b = 1.0;
	c = 0.0;
	for (j = 0; j < 32; j++) {
		a += 1.0;   /* 1/f harmonics */
		b += 2.0;   /* odd harmonics */
		c += 2.0;   /* even harmonics */
		filter_dist_1[j] = a;
		filter_dist_2[j] = 1.0 / a;
		filter_dist_3[j] = b;
		filter_dist_4[j] = 1.0 / b;
		filter_dist_5[j] = c;
		filter_dist_6[j] = 1.0 / c;
	}
}


/*****************************************************************************
 * filter_osc_table_12dB()
 *
 * Apply lowpass filter to wavetable for bandlimiting oscillators.
 *****************************************************************************/
#ifdef FILTER_WAVETABLE_12DB
void
filter_osc_table_12dB(int wave_num, int num_cycles, double octaves, sample_t scale)
{
	int         j;
	int         cycle;
	int         sample;
	int         oversample  = 37;
	sample_t    f;
	sample_t    q;
	sample_t    hp          = 0.0;
	sample_t    bp          = 0.0;
	sample_t    lp          = 0.0;

	/* set cutoff at N ocatves above the principal */
	f = (sample_t) sin(M_PI * 2.0 * (pow(2.0, octaves)) /
	                   (F_WAVEFORM_SIZE * (double) oversample));

	/* zero resonance for simple bandlimiting */
	q = 1.0;

	/* run seven cycles of the waveform through the filter. */
	/* dump the output of the filter into the osc table on seventh run */
	for (cycle = 1; cycle <= num_cycles; cycle++) {
		for (sample = 0; sample < F_WAVEFORM_SIZE; sample++) {

			/* oversample the filter */
			for (j = 0; j < oversample; j++) {

				/* highpass */
				hp = scale * (wave_lookup(wave_num, sample) - lp - (bp * q));

				/* bandpass */
				bp += (f * hp);

				/* lowpass */
				lp += (f * bp);
			}

			/* ignore filter output on all but last cycle */
			if (cycle == num_cycles) {
				/* take the lowpass tap */
				wave_set(wave_num, sample, lp);
			}
		}
	}
}
#endif /* FILTER_WAVETABLE_12DB */


/*****************************************************************************
 * filter_osc_table_24dB()
 *
 * Apply lowpass filter to wavetable for bandlimiting oscillators.
 *****************************************************************************/
void
filter_osc_table_24dB(int wave_num, int num_cycles, double octaves, sample_t scale)
{
	int         j;
	int         cycle;
	int         sample;
	int         oversample  = 37;
	sample_t    a;
	sample_t    d;
	sample_t    f;
	sample_t    k;
	sample_t    p;
	sample_t    x           = 0.0;
	sample_t    y1          = 0.0;
	sample_t    y2          = 0.0;
	sample_t    y3          = 0.0;
	sample_t    y4          = 0.0;
	sample_t    oldx        = 0.0;
	sample_t    oldy1       = 0.0;
	sample_t    oldy2       = 0.0;
	sample_t    oldy3       = 0.0;

	f = (sample_t) sin(M_PI * 2.0 * (pow(2.0, octaves)) /
	                   (F_WAVEFORM_SIZE * (double) oversample));
	k = (2.0 * f) - 1.0;
	p = (k + 1.0) * 0.5;

	for (cycle = 1; cycle <= num_cycles; cycle++) {
		for (sample = 0; sample < WAVEFORM_SIZE; sample++) {
			a = 0.0;
			for (j = 0; j < oversample; j++) {
				a += 2.0;
				d = (a - f) / a;
				x = scale * (wave_lookup(wave_num, sample) - (4.0 * p * y4));
				y1 = ((x  + oldx)  * p) - (k * y1);
				y2 = ((y1 + oldy1) * p) - (k * y2);
				y3 = ((y2 + oldy2) * p) - (k * y3);
				y4 = (((y3 + oldy3) * p) - (k * y4)) * d;
				y4 -= ((y4 * y4 * y4) / 6.0);
				oldx  = x;
				oldy1 = y1;
				oldy2 = y2;
				oldy3 = y3;
			}
			if (cycle == num_cycles) {
				wave_set(wave_num, sample, (4.0 * y4));
			}
		}
	}
}


/*****************************************************************************
 * run_experimental_filter()
 *
 * This was a Stilson/Smith (CCRMA) style 24dB/octave Moog filter with ideal
 * cutoff frequency tuning, near ideal decoupling of cutoff and resonance,
 * and the ability to self-oscillate.  An extra frequency curve is used in
 * the resonance calculation to acheive near-constant resonance across the
 * entire frequency range.  The experimental part involves chopping out one
 * of the filter poles without compensating in frequency and resonance
 * computations.
 *****************************************************************************/
void
run_experimental_filter(VOICE *voice, PART *part, PATCH_STATE *state)
{
	int         j;
	int         filter_index;
	sample_t    tmp;
	sample_t    filter_f;
	sample_t    filter_k;
	sample_t    filter_q;
	sample_t    filter_r;
	sample_t    filter_d;

	tmp = (state->filter_gain * keyfollow_table[state->keyfollow_vol][voice->vol_key]) * 0.5;
	voice->out1 *= tmp;
	voice->out2 *= tmp;

	/* assignable lfo/velocity controls */
	tmp = (state->filter_lfo == LFO_VELOCITY) ?
		voice->velocity_coef_linear : part->lfo_out[state->filter_lfo];
	filter_q = state->filter_resonance + (tmp * state->filter_lfo_resonance);

	if (filter_q < 0.0) {
		filter_q = 0.0;
	}
	else if (filter_q > 0.9921875) {
		filter_q = 0.9921875;
	}

	/* assignable lfo/velocity controls with dedicated lfo cutoff */
	filter_index = ((int)(((tmp * state->filter_lfo_cutoff) +
	                       (part->lfo_out[2] * state->lfo_3_cutoff) +
	                       (state->filter_env_amount * voice->filter_env_raw) -
	                       part->filter_env_offset +
	                       state->filter_cutoff +
	                       voice->filter_key_adj + 256.0) *
	                      F_TUNING_RESOLUTION)) +
		state->patch_tune;

	if (filter_index < 0) {
		filter_f = filter_table[0];
		PHASEX_DEBUG(DEBUG_CLASS_ENGINE, "Filter Index = %d\n", filter_index);
	}
	else if (filter_index > (filter_limit - (24 * TUNING_RESOLUTION) - 0)) {
		filter_f = filter_table[filter_limit - (24 * TUNING_RESOLUTION) - 0];
	}
	else {
		filter_f = filter_table[filter_index];
	}

	filter_k = (2.0 * filter_f) - 1.0;
	filter_r = (sample_t)(((1.0 + (sample_t) MATH_SIN(filter_q * M_PI_2)) *
	                       (1.0 - filter_f)) - 1.0) * 4.0;

	switch (state->filter_type) {
	case FILTER_TYPE_EXPERIMENTAL_DIST:
		/* waveshaper saturation/distortion (fixed at a = 0.9) */
		tmp = (sample_t) MATH_ABS(voice->out1);
		voice->out1 *= (tmp + 0.9) / ((tmp * tmp) + (0.9 - 1.0) * tmp + 1.0);
		tmp = (sample_t) MATH_ABS(voice->out2);
		voice->out2 *= (tmp + 0.9) / ((tmp * tmp) + (0.9 - 1.0) * tmp + 1.0);

		for (j = 0; j < FILTER_OVERSAMPLE; j++) {
			filter_d = (filter_dist_5[j] - filter_f) * filter_dist_6[j];

			voice->filter_x_1 = (voice->out1 - filter_r * voice->filter_y4_1);
			voice->filter_x_2 = (voice->out2 - filter_r * voice->filter_y4_2);

			voice->filter_y1_1 = ((voice->filter_x_1  + voice->filter_oldx_1) *
			                      filter_f)  - (filter_k * voice->filter_y1_1);
			voice->filter_y1_2 = ((voice->filter_x_2  + voice->filter_oldx_2) *
			                      filter_f)  - (filter_k * voice->filter_y1_2);

			voice->filter_y3_1 = ((voice->filter_y1_1 + voice->filter_oldy1_1) *
			                      filter_f) - (filter_k * voice->filter_y3_1);
			voice->filter_y3_2 = ((voice->filter_y1_2 + voice->filter_oldy1_2) *
			                      filter_f) - (filter_k * voice->filter_y3_2);

			voice->filter_y4_1 = (((voice->filter_y3_1 + voice->filter_oldy3_1) *
			                       filter_f) - (filter_k * voice->filter_y4_1)) * filter_d;
			voice->filter_y4_2 = (((voice->filter_y3_2 + voice->filter_oldy3_2) *
			                       filter_f) - (filter_k * voice->filter_y4_2)) * filter_d;

			voice->filter_y4_1 -= ((voice->filter_y4_1 * voice->filter_y4_1 *
			                        voice->filter_y4_1) * 0.1666666666666666);
			voice->filter_y4_2 -= ((voice->filter_y4_2 * voice->filter_y4_2 *
			                        voice->filter_y4_2) * 0.1666666666666666);

			voice->filter_oldx_1  = voice->filter_x_1  + part->denormal_offset;
			voice->filter_oldx_2  = voice->filter_x_2  + part->denormal_offset;
			voice->filter_oldy1_1 = voice->filter_y1_1 - part->denormal_offset;
			voice->filter_oldy1_2 = voice->filter_y1_2 - part->denormal_offset;
			voice->filter_oldy3_1 = voice->filter_y3_1 + part->denormal_offset;
			voice->filter_oldy3_2 = voice->filter_y3_2 + part->denormal_offset;
		}
		break;
	case FILTER_TYPE_EXPERIMENTAL_CLEAN:
		for (j = 0; j < FILTER_OVERSAMPLE; j++) {
			voice->filter_x_1 = (voice->out1 - filter_r * voice->filter_y4_1);
			voice->filter_x_2 = (voice->out2 - filter_r * voice->filter_y4_2);

			voice->filter_y1_1 = ((voice->filter_x_1  + voice->filter_oldx_1)  *
			                      filter_f) - (filter_k * voice->filter_y1_1);
			voice->filter_y1_2 = ((voice->filter_x_2  + voice->filter_oldx_2)  *
			                      filter_f) - (filter_k * voice->filter_y1_2);

			voice->filter_y3_1 = ((voice->filter_y1_1 + voice->filter_oldy1_1) *
			                      filter_f) - (filter_k * voice->filter_y3_1);
			voice->filter_y3_2 = ((voice->filter_y1_2 + voice->filter_oldy1_2) *
			                      filter_f) - (filter_k * voice->filter_y3_2);

			voice->filter_y4_1 = ((voice->filter_y3_1 + voice->filter_oldy3_1) *
			                      filter_f) - (filter_k * voice->filter_y4_1);
			voice->filter_y4_2 = ((voice->filter_y3_2 + voice->filter_oldy3_2) *
			                      filter_f) - (filter_k * voice->filter_y4_2);

			voice->filter_y4_1 -= ((voice->filter_y4_1 * voice->filter_y4_1 *
			                        voice->filter_y4_1) * 0.166666666666666);
			voice->filter_y4_2 -= ((voice->filter_y4_2 * voice->filter_y4_2 *
			                        voice->filter_y4_2) * 0.166666666666666);

			voice->filter_oldx_1  = voice->filter_x_1  + part->denormal_offset;
			voice->filter_oldx_2  = voice->filter_x_2  + part->denormal_offset;
			voice->filter_oldy1_1 = voice->filter_y1_1 - part->denormal_offset;
			voice->filter_oldy1_2 = voice->filter_y1_2 - part->denormal_offset;
			voice->filter_oldy3_1 = voice->filter_y3_1 + part->denormal_offset;
			voice->filter_oldy3_2 = voice->filter_y3_2 + part->denormal_offset;
		}
		break;
	}

	switch (state->filter_mode) {
	case FILTER_MODE_LP:
		voice->out1 = 3.0 * voice->filter_y4_1;
		voice->out2 = 3.0 * voice->filter_y4_2;
		break;
	case FILTER_MODE_HP:  /* empirical tuning */
		voice->out1 = (2.093 * voice->filter_x_1) +
			(2.0 * (voice->filter_y4_1 - voice->filter_y3_1 - voice->filter_y1_1));
		voice->out2 = (2.093 * voice->filter_x_2) +
			(2.0 * (voice->filter_y4_2 - voice->filter_y3_2 - voice->filter_y1_2));
		break;
	case FILTER_MODE_BP:
		voice->out1 = 3.0 * (voice->filter_y3_1 - voice->filter_y4_1);
		voice->out2 = 3.0 * (voice->filter_y3_2 - voice->filter_y4_2);
		break;
	case FILTER_MODE_BS:
		voice->out1 -= 3.0 * (voice->filter_y3_1 - voice->filter_y4_1);
		voice->out2 -= 3.0 * (voice->filter_y3_2 - voice->filter_y4_2);
		break;
	case FILTER_MODE_LP_PLUS_BP:
		voice->out1 = 2.0 * (voice->filter_y3_1 + voice->filter_y4_1);
		voice->out2 = 2.0 * (voice->filter_y3_2 + voice->filter_y4_2);
		break;
	case FILTER_MODE_HP_PLUS_BP:
		voice->out1 = (2.093 * voice->filter_x_1) - (2.0 * voice->filter_y1_1) -
			(3.0 * voice->filter_y3_1) + (3.0 * voice->filter_y4_1);
		voice->out2 = (2.093 * voice->filter_x_2) - (2.0 * voice->filter_y1_2) -
			(3.0 * voice->filter_y3_2) + (3.0 * voice->filter_y4_2);
		break;
	case FILTER_MODE_LP_PLUS_HP:
		voice->out1 = (2.093 * voice->filter_x_1) - (2.0 * voice->filter_y1_1) +
			(3.0 * voice->filter_y4_1);
		voice->out2 = (2.093 * voice->filter_x_2) - (2.0 * voice->filter_y1_2) +
			(3.0 * voice->filter_y4_2);
		break;
	case FILTER_MODE_BS_PLUS_BP:
		voice->out1 = ((4.0 * voice->out1) - voice->filter_y1_1 + voice->filter_y3_1 +
		               ((1.0 - (4.0 * filter_r)) * voice->filter_y4_1) +
		               (2.0 * voice->filter_x_1)) * 0.5;
		voice->out2 = ((4.0 * voice->out2) - voice->filter_y1_2 + voice->filter_y3_2 +
		               ((1.0 - (4.0 * filter_r)) * voice->filter_y4_2) +
		               (2.0 * voice->filter_x_2)) * 0.5;
		break;
	}

	voice->out1 *= 0.25;
	voice->out2 *= 0.25;
}



/*****************************************************************************
 * run_moog_filter()
 *
 * This is a Stilson/Smith (CCRMA) style 24dB/octave Moog filter with ideal
 * cutoff frequency tuning, near ideal decoupling of cutoff and resonance,
 * and the ability to self-oscillate.  An extra frequency curve is used in
 * the resonance calculation to acheive near-constant resonance across the
 * entire frequency range.
 *****************************************************************************/
void
run_moog_filter(VOICE *voice, PART *part, PATCH_STATE *state)
{
	int         j;
	int         filter_index;
	sample_t    tmp;
	sample_t    filter_f;
	sample_t    filter_k;
	sample_t    filter_q;
	sample_t    filter_r;
	sample_t    filter_d;

	tmp = (state->filter_gain * keyfollow_table[state->keyfollow_vol][voice->vol_key]) * 0.5;
	voice->out1 *= tmp;
	voice->out2 *= tmp;

	/* waveshaper saturation/distortion */
	tmp = (sample_t) MATH_ABS(voice->out1);
	voice->out1 *= (tmp + 0.9) / ((tmp * tmp) + (0.9 - 1.0) * tmp + 1.0);
	tmp = (sample_t) MATH_ABS(voice->out2);
	voice->out2 *= (tmp + 0.9) / ((tmp * tmp) + (0.9 - 1.0) * tmp + 1.0);

	/* assignable lfo/velocity controls */
	tmp = (state->filter_lfo == LFO_VELOCITY) ?
		voice->velocity_coef_linear : part->lfo_out[state->filter_lfo];
	filter_q = (state->filter_resonance + (tmp * state->filter_lfo_resonance));

	if (filter_q < 0.0) {
		filter_q = 0.0;
	}
	else if (filter_q > 0.9921875) {
		filter_q = 0.9921875;
	}

	/* assignable lfo/velocity controls with dedicated lfo cutoff */
	filter_index = ((int)(((tmp * state->filter_lfo_cutoff) +
	                       (part->lfo_out[2] * state->lfo_3_cutoff) +
	                       (state->filter_env_amount * voice->filter_env_raw) -
	                       part->filter_env_offset +
	                       state->filter_cutoff +
	                       voice->filter_key_adj + 256.0) *
	                      F_TUNING_RESOLUTION)) +
		state->patch_tune;

	if (filter_index < 0) {
		filter_f = filter_table[0];
		PHASEX_DEBUG(DEBUG_CLASS_ENGINE, "Filter Index = %d\n", filter_index);
	}
	else if (filter_index > (filter_limit - (24 * TUNING_RESOLUTION) - 0)) {
		filter_f = filter_table[filter_limit - (24 * TUNING_RESOLUTION) - 0];
	}
	else {
		filter_f = filter_table[filter_index];
	}

	filter_k = (2.0 * filter_f) - 1.0;
	filter_r = (sample_t)(((1.0 + (sample_t) MATH_SIN(filter_q * M_PI_2)) *
	                       (1.0 - filter_f)) - 1.0) * 4.0;

	switch (state->filter_type) {
	case FILTER_TYPE_MOOG_DIST:
		for (j = 0; j < FILTER_OVERSAMPLE; j++) {
			filter_d = (filter_dist_5[j] - filter_f) * filter_dist_6[j];

			voice->filter_x_1 = (voice->out1 - filter_r * voice->filter_y4_1);
			voice->filter_x_2 = (voice->out2 - filter_r * voice->filter_y4_2);

			voice->filter_y1_1 = ((voice->filter_x_1  + voice->filter_oldx_1)  *
			                      filter_f) - (filter_k * voice->filter_y1_1);
			voice->filter_y1_2 = ((voice->filter_x_2  + voice->filter_oldx_2)  *
			                      filter_f) - (filter_k * voice->filter_y1_2);

			voice->filter_y2_1 = ((voice->filter_y1_1 + voice->filter_oldy1_1) *
			                      filter_f) - (filter_k * voice->filter_y2_1);
			voice->filter_y2_2 = ((voice->filter_y1_2 + voice->filter_oldy1_2) *
			                      filter_f) - (filter_k * voice->filter_y2_2);

			voice->filter_y3_1 = ((voice->filter_y2_1 + voice->filter_oldy2_1) *
			                      filter_f) - (filter_k * voice->filter_y3_1);
			voice->filter_y3_2 = ((voice->filter_y2_2 + voice->filter_oldy2_2) *
			                      filter_f) - (filter_k * voice->filter_y3_2);

			voice->filter_y4_1 = (((voice->filter_y3_1 + voice->filter_oldy3_1) *
			                       filter_f) - (filter_k * voice->filter_y4_1)) * filter_d;
			voice->filter_y4_2 = (((voice->filter_y3_2 + voice->filter_oldy3_2) *
			                       filter_f) - (filter_k * voice->filter_y4_2)) * filter_d;

			voice->filter_y4_1 -= ((voice->filter_y4_1 * voice->filter_y4_1 *
			                        voice->filter_y4_1) * 0.1666666666666666);
			voice->filter_y4_2 -= ((voice->filter_y4_2 * voice->filter_y4_2 *
			                        voice->filter_y4_2) * 0.1666666666666666);

			voice->filter_oldx_1  = voice->filter_x_1  + part->denormal_offset;
			voice->filter_oldx_2  = voice->filter_x_2  + part->denormal_offset;
			voice->filter_oldy1_1 = voice->filter_y1_1 - part->denormal_offset;
			voice->filter_oldy1_2 = voice->filter_y1_2 - part->denormal_offset;
			voice->filter_oldy2_1 = voice->filter_y2_1 + part->denormal_offset;
			voice->filter_oldy2_2 = voice->filter_y2_2 + part->denormal_offset;
			voice->filter_oldy3_1 = voice->filter_y3_1 - part->denormal_offset;
			voice->filter_oldy3_2 = voice->filter_y3_2 - part->denormal_offset;
		}
		break;
	case FILTER_TYPE_MOOG_CLEAN:
		for (j = 0; j < FILTER_OVERSAMPLE; j++) {
			voice->filter_x_1 = (voice->out1 - filter_r * voice->filter_y4_1);
			voice->filter_x_2 = (voice->out2 - filter_r * voice->filter_y4_2);

			voice->filter_y1_1 = ((voice->filter_x_1  + voice->filter_oldx_1)  *
			                      filter_f) - (filter_k * voice->filter_y1_1);
			voice->filter_y1_2 = ((voice->filter_x_2  + voice->filter_oldx_2)  *
			                      filter_f) - (filter_k * voice->filter_y1_2);

			voice->filter_y2_1 = ((voice->filter_y1_1 + voice->filter_oldy1_1) *
			                      filter_f) - (filter_k * voice->filter_y2_1);
			voice->filter_y2_2 = ((voice->filter_y1_2 + voice->filter_oldy1_2) *
			                      filter_f) - (filter_k * voice->filter_y2_2);

			voice->filter_y3_1 = ((voice->filter_y2_1 + voice->filter_oldy2_1) *
			                      filter_f) - (filter_k * voice->filter_y3_1);
			voice->filter_y3_2 = ((voice->filter_y2_2 + voice->filter_oldy2_2) *
			                      filter_f) - (filter_k * voice->filter_y3_2);

			voice->filter_y4_1 = ((voice->filter_y3_1 + voice->filter_oldy3_1) *
			                      filter_f) - (filter_k * voice->filter_y4_1);
			voice->filter_y4_2 = ((voice->filter_y3_2 + voice->filter_oldy3_2) *
			                      filter_f) - (filter_k * voice->filter_y4_2);

			voice->filter_y4_1 -= ((voice->filter_y4_1 * voice->filter_y4_1 *
			                        voice->filter_y4_1) * 0.166666666666666);
			voice->filter_y4_2 -= ((voice->filter_y4_2 * voice->filter_y4_2 *
			                        voice->filter_y4_2) * 0.166666666666666);

			voice->filter_oldx_1  = voice->filter_x_1  + part->denormal_offset;
			voice->filter_oldx_2  = voice->filter_x_2  + part->denormal_offset;
			voice->filter_oldy1_1 = voice->filter_y1_1 - part->denormal_offset;
			voice->filter_oldy1_2 = voice->filter_y1_2 - part->denormal_offset;
			voice->filter_oldy2_1 = voice->filter_y2_1 + part->denormal_offset;
			voice->filter_oldy2_2 = voice->filter_y2_2 + part->denormal_offset;
			voice->filter_oldy3_1 = voice->filter_y3_1 - part->denormal_offset;
			voice->filter_oldy3_2 = voice->filter_y3_2 - part->denormal_offset;
		}
		break;
	}

	switch (state->filter_mode) {
	case FILTER_MODE_LP:
		voice->out1 = 3.0 * voice->filter_y4_1;
		voice->out2 = 3.0 * voice->filter_y4_2;
		break;
	case FILTER_MODE_HP:
		voice->out1 = (2.093 * voice->filter_x_1) +
			(2.0 * (voice->filter_y4_1 - voice->filter_y3_1 - voice->filter_y1_1));
		voice->out2 = (2.093 * voice->filter_x_2) +
			(2.0 * (voice->filter_y4_2 - voice->filter_y3_2 - voice->filter_y1_2));
		break;
	case FILTER_MODE_BP:
		voice->out1 = 3.0 * (voice->filter_y3_1 - voice->filter_y4_1);
		voice->out2 = 3.0 * (voice->filter_y3_2 - voice->filter_y4_2);
		break;
	case FILTER_MODE_BS:
		voice->out1 -= 3.0 * (voice->filter_y3_1 - voice->filter_y4_1);
		voice->out2 -= 3.0 * (voice->filter_y3_2 - voice->filter_y4_2);
		break;
	case FILTER_MODE_LP_PLUS_BP:
		voice->out1 = 2.0 * (voice->filter_y3_1 + voice->filter_y4_1);
		voice->out2 = 2.0 * (voice->filter_y3_2 + voice->filter_y4_2);
		break;
	case FILTER_MODE_HP_PLUS_BP:
		voice->out1 = (2.093 * voice->filter_x_1) - (2.0 * voice->filter_y1_1) -
			(3.0 * voice->filter_y3_1) + (3.0 * voice->filter_y4_1);
		voice->out2 = (2.093 * voice->filter_x_2) - (2.0 * voice->filter_y1_2) -
			(3.0 * voice->filter_y3_2) + (3.0 * voice->filter_y4_2);
		break;
	case FILTER_MODE_LP_PLUS_HP:
		voice->out1 = (2.093 * voice->filter_x_1) - (2.0 * voice->filter_y1_1) +
			(3.0 * voice->filter_y4_1);
		voice->out2 = (2.093 * voice->filter_x_2) - (2.0 * voice->filter_y1_2) +
			(3.0 * voice->filter_y4_2);
		break;
	case FILTER_MODE_BS_PLUS_BP:
		voice->out1 = ((4.0 * voice->out1) - voice->filter_y1_1 -
		               voice->filter_y2_1 + voice->filter_y3_1 +
		               ((1.0 - (4.0 * filter_r)) * voice->filter_y4_1) +
		               (2.0 * voice->filter_x_1)) * 0.5;
		voice->out2 = ((4.0 * voice->out2) - voice->filter_y1_2 -
		               voice->filter_y2_2 + voice->filter_y3_2 +
		               ((1.0 - (4.0 * filter_r)) * voice->filter_y4_2) +
		               (2.0 * voice->filter_x_2)) * 0.5;
		break;
	}

	voice->out1 *= 0.25;
	voice->out2 *= 0.25;
}


/*****************************************************************************
 * run_filter()
 *
 * This filter is an oversampled Chamberlin 12dB/octave filter.
 * Table lookups are used for ideal cutoff frequency tuning and
 * harmonic waveshaping.  Cutoff and resonance controls are fully
 * independent.  Filter does not self-oscillate.
 *****************************************************************************/
void
run_filter(VOICE *voice, PART *part, PATCH_STATE *state)
{
	int         j;
	sample_t    tmp;
	sample_t    filter_f;
	sample_t    filter_q;
	int         filter_index;

	/* apply keyfollow volume and filter gain at filter input */
	tmp = (state->filter_gain * keyfollow_table[state->keyfollow_vol][voice->vol_key]) * 0.25;
	voice->out1 *= tmp;
	voice->out2 *= tmp;

	/* selectable lfo */
	tmp = (state->filter_lfo == LFO_VELOCITY)
		? voice->velocity_coef_linear : part->lfo_out[state->filter_lfo];
	filter_q = 1.0 - (state->filter_resonance + (tmp * state->filter_lfo_resonance));

	if (filter_q < 0.00390625) {  // 1/256
		filter_q = 0.00390625;
	}
	else if (filter_q > 1.0) {
		filter_q = 1.0;
	}

	/* assignable lfo/velocity controls with dedicated lfo cutoff */
	filter_index = ((int)(((tmp * state->filter_lfo_cutoff) +
	                       (part->lfo_out[2] * state->lfo_3_cutoff) +
	                       (state->filter_env_amount * voice->filter_env_raw) -
	                       part->filter_env_offset +
	                       state->filter_cutoff +
	                       voice->filter_key_adj + 256.0) *
	                      F_TUNING_RESOLUTION)) +
		state->patch_tune;

	/* now look up the f coefficient from the table */
	/* use hard clipping (top midi note + 2 octaves) for filter cutoff */
	if (filter_index < 0) {
		filter_f = filter_table[0];
		PHASEX_DEBUG(DEBUG_CLASS_ENGINE, "Filter Index = %d\n", filter_index);
	}
	else if (filter_index > (filter_limit - 11)) {
		filter_f = filter_table[filter_limit - 11];
	}
	else {
		filter_f = filter_table[filter_index];
	}

	/* Two variations of the Chamberlin filter */
	switch (state->filter_type) {

	case FILTER_TYPE_DIST:
		/* "Dist" - LP distortion */
		for (j = 0; j < FILTER_OVERSAMPLE; j++) {
			/* highpass */
			voice->filter_hp1 = voice->out1 - voice->filter_lp1 -
				(voice->filter_bp1 * filter_q);
			voice->filter_hp2 = voice->out2 - voice->filter_lp2 -
				(voice->filter_bp2 * filter_q);

			/* bandpass */
			voice->filter_bp1 += filter_f * voice->filter_hp1;
			voice->filter_bp2 += filter_f * voice->filter_hp2;

			/* lowpass */
			voice->filter_lp1 += filter_f * voice->filter_bp1;
			voice->filter_lp2 += filter_f * voice->filter_bp2;

			/* lowpass distortion */
			tmp = (filter_dist_1[j] - filter_f) * filter_dist_2[j];
			voice->filter_lp1 *= tmp;
			voice->filter_lp2 *= tmp;

			/* soft clipping */
			voice->filter_lp1 -= ((voice->filter_lp1 * voice->filter_lp1 *
			                       voice->filter_lp1) * 0.1666666666666666);
			voice->filter_lp2 -= ((voice->filter_lp2 * voice->filter_lp2 *
			                       voice->filter_lp2) * 0.1666666666666666);
		}
		break;

	case FILTER_TYPE_RETRO:
		/* "Retro" - No distortion */
		for (j = 0; j < FILTER_OVERSAMPLE; j++) {
			/* highpass */
			voice->filter_hp1 = voice->out1 - voice->filter_lp1 -
				(voice->filter_bp1 * filter_q);
			voice->filter_hp2 = voice->out2 - voice->filter_lp2 -
				(voice->filter_bp2 * filter_q);

			/* bandpass */
			voice->filter_bp1 += filter_f * voice->filter_hp1;
			voice->filter_bp2 += filter_f * voice->filter_hp2;

			/* lowpass */
			voice->filter_lp1 += filter_f * voice->filter_bp1;
			voice->filter_lp2 += filter_f * voice->filter_bp2;
		}
		break;
	}

	voice->filter_hp1 += part->denormal_offset;
	voice->filter_hp1 += part->denormal_offset;
	voice->filter_bp1 -= part->denormal_offset;
	voice->filter_bp1 -= part->denormal_offset;
	voice->filter_lp1 -= part->denormal_offset;
	voice->filter_lp1 -= part->denormal_offset;

	/* select the filter output we want */
	switch (state->filter_mode) {
	case FILTER_MODE_LP:
		voice->out1 = voice->filter_lp1;
		voice->out2 = voice->filter_lp2;
		break;
	case FILTER_MODE_HP:
		voice->out1 = voice->filter_hp1;
		voice->out2 = voice->filter_hp2;
		break;
	case FILTER_MODE_BP:
		voice->out1 = voice->filter_bp1;
		voice->out2 = voice->filter_bp2;
		break;
	case FILTER_MODE_BS:
		voice->out1 = (voice->filter_lp1 - voice->filter_bp1 + voice->filter_hp1);
		voice->out2 = (voice->filter_lp2 - voice->filter_bp2 + voice->filter_hp2);
		break;
	case FILTER_MODE_LP_PLUS_BP:
		voice->out1 = (voice->filter_lp1 + voice->filter_bp1);
		voice->out2 = (voice->filter_lp2 + voice->filter_bp2);
		break;
	case FILTER_MODE_HP_PLUS_BP:
		voice->out1 = (voice->filter_bp1 + voice->filter_hp1);
		voice->out2 = (voice->filter_bp2 + voice->filter_hp2);
		break;
	case FILTER_MODE_LP_PLUS_HP:
		voice->out1 = (voice->filter_lp1 - voice->filter_hp1);
		voice->out2 = (voice->filter_lp2 - voice->filter_hp2);
		break;
	case FILTER_MODE_BS_PLUS_BP:
		voice->out1 = (voice->filter_lp1 + voice->filter_bp1 + voice->filter_hp1);
		voice->out2 = (voice->filter_lp2 + voice->filter_bp2 + voice->filter_hp2);
		break;
	}
}
