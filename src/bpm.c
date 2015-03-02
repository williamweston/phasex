/*****************************************************************************
 *
 * bpm.c
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
#include <unistd.h>
#include <math.h>
#include "phasex.h"
#include "settings.h"
#include "wave.h"
#include "engine.h"
#include "patch.h"
#include "param.h"
#include "bank.h"
#include "session.h"
#include "debug.h"


/*****************************************************************************
 * override_bpm()
 *****************************************************************************/
void
override_bpm(unsigned int new_bpm)
{
	PARAM           *param;
	unsigned int    sess_num;
	unsigned int    part_num;
	unsigned int    prog_num;

	if (new_bpm > 0) {
		for (part_num = 0; part_num < MAX_PARTS; part_num++) {
			get_param_info_by_id(PARAM_BPM)->locked = 1;
			for (prog_num = 0; prog_num < PATCH_BANK_SIZE; prog_num++) {
				param = & (patch_bank[part_num]
				           [prog_num].param[PARAM_BPM]);
				param->value.cc_prev = param->value.cc_val;
				param->value.cc_val  = (int) new_bpm - 64;
				param->value.int_val = (int) new_bpm;
			}
			for (sess_num = 0; sess_num < SESSION_BANK_SIZE; sess_num++) {
				param = & (session_bank[sess_num].patch[part_num].param[PARAM_BPM]);
				param->value.cc_prev = param->value.cc_val;
				param->value.cc_val  = (int) new_bpm - 64;
				param->value.int_val = (int) new_bpm;
			}
		}
	}
}


/*****************************************************************************
 * set_bpm()
 *****************************************************************************/
void
set_bpm(PARAM *param, sample_t float_bpm)
{
	PATCH_STATE     *state   = param->patch->state;
	PART            *part    = param->patch->part;
	int             cc_val   = param->value.cc_val;
	int             int_val  = param->value.int_val;
	unsigned int    id       = param->info->id;
	unsigned int    part_num = param->patch->part_num;
	DELAY           *delay   = get_delay(part_num);
	CHORUS          *chorus  = get_chorus(part_num);
	VOICE           *voice;
	unsigned int    lfo;
	unsigned int    osc;
	unsigned int    voice_num;

	/* use supplied floating point value, if non-zero. */
	if (float_bpm > 0.0) {
		int_val = (int) float_bpm;
		cc_val  = int_val - 64;
	}

	/* initialize all variables based on bpm */
	state->bpm_cc = cc_val & 0x7F;
	state->bpm    = (sample_t) int_val;

	global.bpm    = (sample_t)(state->bpm);
	global.bps    = (sample_t)(state->bpm) / 60.0;

	/* For now, keep bpm per part, and update all parts at once to make it
	   appear as global.  This way, bpm can still be saved with patches. */
	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		param  = get_param(part_num, id);
		state  = get_active_state(part_num);
		part   = get_part(part_num);
		delay  = get_delay(part_num);
		chorus = get_chorus(part_num);

		if (param->value.cc_val != cc_val) {
			param->value.cc_prev = param->value.cc_val;
			param->value.cc_val  = cc_val;
			param->value.int_val = int_val;
			param->updated = 1;
		}

		/* initialize all variables based on bpm */
		state->bpm_cc  = cc_val & 0x7F;
		state->bpm     = (sample_t) int_val;

		/* re-initialize delay size */
		delay->size   = state->delay_time * f_sample_rate / global.bps;
		delay->length = (int)(delay->size);

		/* re-initialize chorus lfos */
		chorus->lfo_freq     = global.bps * state->chorus_lfo_rate;
		chorus->lfo_adjust   = chorus->lfo_freq * wave_period;
		chorus->phase_freq   = global.bps * state->chorus_phase_rate;
		chorus->phase_adjust = chorus->phase_freq * wave_period;

		/* per-lfo setup */
		for (lfo = 0; lfo < NUM_LFOS; lfo++) {
			/* re-calculate frequency and index adjustment */
			if (state->lfo_freq_base[lfo] >= FREQ_BASE_TEMPO) {
				part->lfo_freq[lfo]   = global.bps * state->lfo_rate[lfo];
				part->lfo_adjust[lfo] = part->lfo_freq[lfo] * wave_period;
			}
		}

		/* per-oscillator setup */
		for (osc = 0; osc < NUM_OSCS; osc++) {
			/* re-calculate tempo based osc freq */
			if (state->osc_freq_base[osc] >= FREQ_BASE_TEMPO) {
				for (voice_num = 0; voice_num < (unsigned int) setting_polyphony; voice_num++) {
					voice = get_voice(part_num, voice_num);
					voice->osc_freq[osc] = global.bps * state->osc_rate[osc];
				}
			}
		}
	}
}
