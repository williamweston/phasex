/*****************************************************************************
 *
 * param_cb.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2012 William Weston <whw@linuxmail.org>
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
#include "param_cb.h"
#include "phasex.h"
#include "settings.h"
#include "wave.h"
#include "filter.h"
#include "engine.h"
#include "patch.h"
#include "param.h"
#include "param_parse.h"
#include "bank.h"
#include "debug.h"


/*****************************************************************************
 * update_midi_channel()
 *****************************************************************************/
void
update_midi_channel(PARAM *param)
{
	PART            *part   = param->patch->part;
	int             int_val = param->value.int_val;

	part->midi_channel = int_val;
}

/*****************************************************************************
 * update_patch_tune()
 *****************************************************************************/
void
update_patch_tune(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->patch_tune_cc = (short) cc_val;
	state->patch_tune    = (short) int_val;
}

/*****************************************************************************
 * update_portamento()
 *****************************************************************************/
void
update_portamento(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	int             cc_val  = param->value.cc_val;
	VOICE           *voice;
	int             voice_num;

	state->portamento        = (short) cc_val;
	part->portamento_samples = env_table[cc_val];
	for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
		voice = get_voice(param->patch->part_num, voice_num);
		voice->portamento_samples = env_table[cc_val];
	}
}

/*****************************************************************************
 * update_keymode()
 *****************************************************************************/
void
update_keymode(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	VOICE           *voice;
	int             voice_num;

	state->keymode = (short) cc_val & 0x03;

	/* deactivate voices */
	for (voice_num = 0; voice_num < MAX_VOICES; voice_num++) {
		voice = get_voice(param->patch->part_num, voice_num);
		voice->active    = 0;
		voice->allocated = 0;
		voice->midi_key  = -1;
	}
}

/*****************************************************************************
 * update_keyfollow_vol()
 *****************************************************************************/
void
update_keyfollow_vol(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->keyfollow_vol = (short) cc_val;
}

/*****************************************************************************
 * update_volume()
 *****************************************************************************/
void
update_volume(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->volume_cc = (short) cc_val;
	state->volume    = gain_table[cc_val];
}

/*****************************************************************************
 * update_transpose()
 *****************************************************************************/
void
update_transpose(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;
	VOICE           *voice;
	int             voice_num;

	state->transpose_cc = (short) cc_val;
	state->transpose    = (short) int_val;
	for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
		voice = get_voice(param->patch->part_num, voice_num);
		voice->need_portamento = 1;
	}
}

/*****************************************************************************
 * update_input_boost()
 *****************************************************************************/
void
update_input_boost(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->input_boost_cc = (short) cc_val;
	state->input_boost    = 1.0 + (((sample_t) int_val) / 32.0);
}

/*****************************************************************************
 * update_input_follow()
 *****************************************************************************/
void
update_input_follow(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	int             cc_val  = param->value.cc_val;

	state->input_follow = (short) cc_val & 0x01;

	part->input_env_raw = 0.0;
	part->in1  = 0.0;
	part->in2  = 0.0;
	part->out1 = 0.0;
	part->out2 = 0.0;
}

/*****************************************************************************
 * update_pan()
 *****************************************************************************/
void
update_pan(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->pan_cc = (short) cc_val;
}

/*****************************************************************************
 * update_stereo_width()
 *****************************************************************************/
void
update_stereo_width(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->stereo_width_cc = (short) cc_val;
	state->stereo_width    = ((sample_t)(int_val) / 254.0) + 0.5;
}

/*****************************************************************************
 * update_amp_velocity()
 *****************************************************************************/
void
update_amp_velocity(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->amp_velocity_cc = (short) cc_val;
	state->amp_velocity    = ((sample_t) int_val) * 0.0078125;
}

/*****************************************************************************
 * update_filter_cutoff()
 *****************************************************************************/
void
update_filter_cutoff(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->filter_cutoff_cc    = (short) cc_val;
	part->filter_cutoff_target = (sample_t) int_val;
}

/*****************************************************************************
 * update_filter_resonance()
 *****************************************************************************/
void
update_filter_resonance(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->filter_resonance_cc = (short) cc_val;
	state->filter_resonance    = ((sample_t) int_val) * 0.0078125;
}

/*****************************************************************************
 * update_filter_smoothing()
 *****************************************************************************/
void
update_filter_smoothing(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	int             cc_val  = param->value.cc_val;

	state->filter_smoothing    = (short) cc_val;
	part->filter_smooth_len    = ((sample_t)(cc_val + 1)) * 160.0;
	part->filter_smooth_factor = 1.0 / (part->filter_smooth_len + 1.0);
}

/*****************************************************************************
 * update_filter_keyfollow()
 *****************************************************************************/
void
update_filter_keyfollow(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->filter_keyfollow = (short) cc_val % 5;
}

/*****************************************************************************
 * update_filter_mode()
 *****************************************************************************/
void
update_filter_mode(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->filter_mode = (short) cc_val % 9;
}

/*****************************************************************************
 * update_filter_type()
 *****************************************************************************/
void
update_filter_type(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	VOICE           *voice;
	int             cc_val  = param->value.cc_val;
	int             voice_num;

	state->filter_type = (short) cc_val % 6;

	for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
		voice = get_voice(param->patch->part_num, voice_num);

		voice->filter_hp1     = voice->filter_hp2     = 0.0;
		voice->filter_bp1     = voice->filter_bp2     = 0.0;
		voice->filter_lp1     = voice->filter_lp2     = 0.0;

		voice->filter_x_1     = voice->filter_x_2     = 0.0;
		voice->filter_y1_1    = voice->filter_y1_2    = 0.0;
		voice->filter_y2_1    = voice->filter_y2_2    = 0.0;
		voice->filter_y3_1    = voice->filter_y3_2    = 0.0;
		voice->filter_y4_1    = voice->filter_y4_2    = 0.0;
		voice->filter_oldx_1  = voice->filter_oldx_2  = 0.0;
		voice->filter_oldy1_1 = voice->filter_oldy1_2 = 0.0;
		voice->filter_oldy2_1 = voice->filter_oldy2_2 = 0.0;
		voice->filter_oldy3_1 = voice->filter_oldy3_2 = 0.0;
	}
}

/*****************************************************************************
 * update_filter_gain()
 *****************************************************************************/
void
update_filter_gain(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->filter_gain_cc = (short) cc_val;
	state->filter_gain    = gain_table[cc_val];
}

/*****************************************************************************
 * update_filter_env_amount()
 *****************************************************************************/
void
update_filter_env_amount(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->filter_env_amount_cc = (short) cc_val;
	state->filter_env_amount    = ((sample_t) int_val) * state->filter_env_sign;

	part->filter_env_offset = (state->filter_env_sign_cc == 0) ? state->filter_env_amount : 0.0;
}

/*****************************************************************************
 * update_filter_env_sign()
 *****************************************************************************/
void
update_filter_env_sign(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	int             cc_val  = param->value.cc_val;

	state->filter_env_sign_cc = (short) cc_val;
	state->filter_env_sign    = (((sample_t) cc_val) * 2.0) - 1.0;
	state->filter_env_amount  = ((sample_t) state->filter_env_amount_cc) * state->filter_env_sign;

	part->filter_env_offset = (state->filter_env_sign_cc == 0) ? state->filter_env_amount : 0.0;
}

/*****************************************************************************
 * update_filter_attack()
 *****************************************************************************/
void
update_filter_attack(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->filter_attack = (short) cc_val;
}

/*****************************************************************************
 * update_filter_decay()
 *****************************************************************************/
void
update_filter_decay(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->filter_decay = (short) cc_val;
}

/*****************************************************************************
 * update_filter_sustain()
 *****************************************************************************/
void
update_filter_sustain(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;
	VOICE           *voice;
	int             voice_num;

	state->filter_sustain_cc   = (short) cc_val;
	state->filter_sustain      = ((sample_t) int_val) / 127.0;
	for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
		voice = get_voice(param->patch->part_num, voice_num);
		if (voice->cur_filter_interval == ENV_INTERVAL_SUSTAIN) {
			voice->filter_env_raw = state->filter_sustain;
		}
	}
}

/*****************************************************************************
 * update_filter_release()
 *****************************************************************************/
void
update_filter_release(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->filter_release = (short) cc_val;
}

/*****************************************************************************
 * update_filter_lfo()
 *****************************************************************************/
void
update_filter_lfo(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->filter_lfo_cc = (short) cc_val;
	state->filter_lfo    = (short)(cc_val + NUM_LFOS) % (NUM_LFOS + 1);
}

/*****************************************************************************
 * update_filter_lfo_cutoff()
 *****************************************************************************/
void
update_filter_lfo_cutoff(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->filter_lfo_cutoff_cc = (short) cc_val;
	state->filter_lfo_cutoff    = (sample_t) int_val;
}

/*****************************************************************************
 * update_filter_lfo_resonance()
 *****************************************************************************/
void
update_filter_lfo_resonance(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->filter_lfo_resonance_cc = (short) cc_val;
	state->filter_lfo_resonance    = (sample_t) int_val * 0.0078125;
}

/*****************************************************************************
 * update_amp_attack()
 *****************************************************************************/
void
update_amp_attack(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->amp_attack = (short) cc_val;
}

/*****************************************************************************
 * update_amp_decay()
 *****************************************************************************/
void
update_amp_decay(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->amp_decay = (short) cc_val;
}

/*****************************************************************************
 * update_amp_sustain()
 *****************************************************************************/
void
update_amp_sustain(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;
	VOICE           *voice;
	int             voice_num;

	state->amp_sustain_cc   = (short) cc_val;
	state->amp_sustain      = ((sample_t) int_val) / 127.0;
	for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
		voice = get_voice(param->patch->part_num, voice_num);
		if (voice->cur_amp_interval == ENV_INTERVAL_SUSTAIN) {
			voice->amp_env_raw = state->amp_sustain;
		}
	}
}

/*****************************************************************************
 * update_amp_release()
 *****************************************************************************/
void
update_amp_release(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->amp_release = (short) cc_val;
}

/*****************************************************************************
 * update_delay_mix()
 *****************************************************************************/
void
update_delay_mix(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	DELAY           *delay  = get_delay(param->patch->part_num);
	int             cc_val  = param->value.cc_val;

	state->delay_mix_cc = (short) cc_val;
	state->delay_mix    = mix_table[cc_val];

	if (cc_val == 0) {
		memset((void *)(delay->buf), 0, DELAY_MAX * 2 * sizeof(sample_t));
	}
}

/*****************************************************************************
 * update_delay_feed()
 *****************************************************************************/
void
update_delay_feed(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->delay_feed_cc = (short) cc_val;
	state->delay_feed    = mix_table[cc_val];
}

/*****************************************************************************
 * update_delay_crossover()
 *****************************************************************************/
void
update_delay_crossover(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->delay_crossover = (short) cc_val & 0x01;
}

/*****************************************************************************
 * update_delay_time()
 *****************************************************************************/
void
update_delay_time(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	DELAY           *delay  = get_delay(param->patch->part_num);

	state->delay_time_cc = (short) cc_val;
	state->delay_time    = 1.0 / get_rate_val(cc_val);
	delay->size          = state->delay_time * f_sample_rate / global.bps;
	delay->half_size     = state->delay_time * f_sample_rate * 0.5 / global.bps;
	delay->length        = (int)(delay->size);
}

/*****************************************************************************
 * update_delay_lfo()
 *****************************************************************************/
void
update_delay_lfo(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->delay_lfo_cc = (short) cc_val;
	state->delay_lfo    = (short)(cc_val + NUM_LFOS) % (NUM_LFOS + 1);
}

/*****************************************************************************
 * update_chorus_mix()
 *****************************************************************************/
void
update_chorus_mix(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	CHORUS          *chorus = get_chorus(param->patch->part_num);
	int             cc_val  = param->value.cc_val;

	state->chorus_mix_cc = (short) cc_val;
	state->chorus_mix    = mix_table[cc_val];

	if (cc_val == 0) {
#ifdef INTERPOLATE_CHORUS
		memset((void *)(chorus->buf_1), 0, CHORUS_MAX     * sizeof(sample_t));
		memset((void *)(chorus->buf_2), 0, CHORUS_MAX     * sizeof(sample_t));
#else
		memset((void *)(chorus->buf),   0, CHORUS_MAX * 2 * sizeof(sample_t));
#endif
	}
}

/*****************************************************************************
 * update_chorus_feed()
 *****************************************************************************/
void
update_chorus_feed(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->chorus_feed_cc = (short) cc_val;
	state->chorus_feed    = mix_table[cc_val];
}

/*****************************************************************************
 * update_chorus_crossover()
 *****************************************************************************/
void
update_chorus_crossover(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->chorus_crossover = (short) cc_val & 0x01;
}

/*****************************************************************************
 * update_chorus_time()
 *****************************************************************************/
void
update_chorus_time(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;
	CHORUS          *chorus = get_chorus(param->patch->part_num);

	state->chorus_time_cc = (short) cc_val;
	state->chorus_time    = f_sample_rate / freq_table[state->patch_tune_cc][407 - int_val];
	chorus->length        = (int)(state->chorus_time) + 1;
	if (chorus->length == 0) {
		chorus->length++;
	}
	chorus->size        = state->chorus_time;
	chorus->half_size   = state->chorus_time * 0.5;
	chorus->delay_index = (chorus->write_index + chorus->bufsize -
	                       chorus->length - 1) & chorus->bufsize_mask;
}

/*****************************************************************************
 * update_chorus_amount()
 *****************************************************************************/
void
update_chorus_amount(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->chorus_amount_cc = (short) cc_val;
	state->chorus_amount    = ((sample_t) int_val) / 128.0;
}

/*****************************************************************************
 * update_chorus_phase_rate()
 *****************************************************************************/
void
update_chorus_phase_rate(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	CHORUS          *chorus = get_chorus(param->patch->part_num);

	state->chorus_phase_rate_cc = (short) cc_val;
	state->chorus_phase_rate    = get_rate_val(cc_val);
	chorus->phase_freq          = global.bps * state->chorus_phase_rate;
	chorus->phase_adjust        = chorus->phase_freq * wave_period;
}

/*****************************************************************************
 * update_chorus_phase_balance()
 *****************************************************************************/
void
update_chorus_phase_balance(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->chorus_phase_balance_cc = (short) cc_val;
	state->chorus_phase_balance    = ((sample_t) int_val) / 128.0;
}

/*****************************************************************************
 * update_chorus_lfo_wave()
 *****************************************************************************/
void
update_chorus_lfo_wave(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->chorus_lfo_wave = (short) cc_val % NUM_WAVEFORMS;
}

/*****************************************************************************
 * update_chorus_lfo_rate()
 *****************************************************************************/
void
update_chorus_lfo_rate(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	CHORUS          *chorus = get_chorus(param->patch->part_num);

	state->chorus_lfo_rate_cc = (short) cc_val;
	state->chorus_lfo_rate    = get_rate_val(cc_val);
	chorus->lfo_freq          = global.bps * state->chorus_lfo_rate;
	chorus->lfo_adjust        = chorus->lfo_freq * wave_period;
}

/*****************************************************************************
 * update_osc_modulation()
 *****************************************************************************/
void
update_osc_modulation(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	VOICE           *voice;
	int             voice_num;

	state->osc_modulation[param->info->index] = (short) cc_val & 0x03;
	if (cc_val == MOD_TYPE_OFF) {
		for (voice_num = 0; voice_num < MAX_VOICES; voice_num++) {
			voice = get_voice(param->patch->part_num, voice_num);
			voice->osc_out1[param->info->index] = 0.0;
			voice->osc_out2[param->info->index] = 0.0;
		}
	}
}

/*****************************************************************************
 * update_osc_wave()
 *****************************************************************************/
void
update_osc_wave(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->osc_wave[param->info->index] = (short) cc_val % NUM_WAVEFORMS;
}

/*****************************************************************************
 * update_osc_freq_base()
 *****************************************************************************/
void
update_osc_freq_base(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	VOICE           *voice;
	int             voice_num;

	state->osc_freq_base[param->info->index] = (short) cc_val;

	if (state->osc_freq_base[param->info->index] >= FREQ_BASE_TEMPO) {
		for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
			voice = get_voice(param->patch->part_num, voice_num);
			voice->osc_freq[param->info->index] = global.bps * state->osc_rate[param->info->index];
		}
	}
}

/*****************************************************************************
 * update_osc_rate()
 *****************************************************************************/
void
update_osc_rate(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	VOICE           *voice;
	int             voice_num;

	state->osc_rate_cc[param->info->index] = (short) cc_val;
	if (state->osc_freq_base[param->info->index] >= FREQ_BASE_TEMPO) {
		state->osc_rate[param->info->index] = get_rate_val(cc_val);
		for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
			voice = get_voice(param->patch->part_num, voice_num);
			voice->osc_freq[param->info->index] = global.bps * state->osc_rate[param->info->index];
		}
	}
}

/*****************************************************************************
 * update_osc_polarity()
 *****************************************************************************/
void
update_osc_polarity(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->osc_polarity_cc[param->info->index] = (short) cc_val & 0x01;
}

/*****************************************************************************
 * update_osc_init_phase()
 *****************************************************************************/
void
update_osc_init_phase(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->osc_init_phase_cc[param->info->index] = (short) cc_val;
	state->osc_init_phase[param->info->index]    = ((sample_t) int_val) / 128.0;
	part->osc_init_index[param->info->index]     =
		state->osc_init_phase[param->info->index] * F_WAVEFORM_SIZE;
}

/*****************************************************************************
 * update_osc_transpose()
 *****************************************************************************/
void
update_osc_transpose(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	VOICE           *voice;
	int             voice_num;
	int             osc;

	state->osc_transpose_cc[param->info->index] = (short) cc_val;
	state->osc_transpose[param->info->index]    =
		(1.0 / 120.0) * ((sample_t) state->osc_fine_tune[param->info->index]);
	for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
		voice = get_voice(param->patch->part_num, voice_num);
		voice->need_portamento = 1;
		if ((state->portamento > 0) && (voice->active)) {
			/* Since portamento is activated for the entire voice, all oscs
			   need to be set properly here, not just this one. */
			for (osc = 0; osc < NUM_OSCS; osc++) {
				voice->osc_portamento[osc] =
					4.0 * (freq_table[state->patch_tune_cc]
					       [256 + voice->osc_key[osc] + state->transpose +
					        state->osc_transpose_cc[osc] - 64] -
					       voice->osc_freq[osc]) /
					(sample_t)(voice->portamento_samples - 1);
			}
			/* Start portamento now that frequency adjustment is known. */
			voice->portamento_sample = voice->portamento_samples;
		}
	}
}

/*****************************************************************************
 * update_osc_fine_tune()
 *****************************************************************************/
void
update_osc_fine_tune(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;
	VOICE           *voice;
	int             voice_num;

	state->osc_fine_tune_cc[param->info->index] = (short) cc_val;
	state->osc_fine_tune[param->info->index]    = (sample_t) int_val;
	state->osc_transpose[param->info->index]    = (1.0 / 120.0) * ((sample_t) int_val);
	for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
		voice = get_voice(param->patch->part_num, voice_num);
		voice->need_portamento = 1;
	}
}

/*****************************************************************************
 * update_osc_pitchbend()
 *****************************************************************************/
void
update_osc_pitchbend(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->osc_pitchbend_cc[param->info->index] = (short) cc_val;
	state->osc_pitchbend[param->info->index]    = (sample_t) int_val;
}

/*****************************************************************************
 * update_osc_am_lfo()
 *****************************************************************************/
void
update_osc_am_lfo(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	short           cc_val  = (short) param->value.cc_val;

	if ((cc_val <= 0) || (cc_val > (NUM_LFOS + NUM_OSCS + 1))) {
		state->am_lfo_cc[param->info->index] = 0;
		state->am_lfo[param->info->index]    = LFO_OFF;
		part->osc_am_mod[param->info->index] = MOD_OFF;
	}
	else {
		state->am_lfo_cc[param->info->index] = cc_val;
		if (cc_val <= NUM_OSCS) {
			state->am_mod_type[param->info->index] = MOD_TYPE_OSC;
			state->am_lfo[param->info->index]      = LFO_OFF;
			part->osc_am_mod[param->info->index]   = (short)(cc_val - 1);
		}
		else if (cc_val <= (NUM_LFOS + NUM_OSCS)) {
			state->am_mod_type[param->info->index] = MOD_TYPE_LFO;
			state->am_lfo[param->info->index]      = (short)(cc_val - NUM_OSCS - 1);
			part->osc_am_mod[param->info->index]   = MOD_OFF;
		}
		else if (cc_val == NUM_LFOS + NUM_OSCS + 1) {
			state->am_mod_type[param->info->index] = MOD_TYPE_VELOCITY;
			state->am_lfo[param->info->index]      = LFO_OFF;
			part->osc_am_mod[param->info->index]   = MOD_VELOCITY;
		}
	}
}

/*****************************************************************************
 * update_osc_am_lfo_amount()
 *****************************************************************************/
void
update_osc_am_lfo_amount(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->am_lfo_amount_cc[param->info->index] = (short) cc_val;
	state->am_lfo_amount[param->info->index]    = ((sample_t) int_val) / 64.0;
}

/*****************************************************************************
 * update_osc_freq_lfo()
 *****************************************************************************/
void
update_osc_freq_lfo(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	short   cc_val  = (short) param->value.cc_val;

	if ((cc_val <= 0) || (cc_val > (NUM_LFOS + (NUM_OSCS * 2) + 1))) {
		state->freq_mod_type[param->info->index] = MOD_TYPE_OFF;
		state->freq_lfo_cc[param->info->index]   = 0;
		state->freq_lfo[param->info->index]      = LFO_OFF;
		part->osc_freq_mod[param->info->index]   = MOD_OFF;
	}
	else {
		state->freq_lfo_cc[param->info->index] = cc_val;
		if (cc_val <= NUM_OSCS) {
			state->freq_mod_type[param->info->index] = MOD_TYPE_OSC;
			state->freq_lfo[param->info->index]      = LFO_OFF;
			part->osc_freq_mod[param->info->index]   = (short)(cc_val - 1);
		}
		else if (cc_val <= (NUM_OSCS * 2)) {
			state->freq_mod_type[param->info->index] = MOD_TYPE_OSC_LATCH;
			state->freq_lfo[param->info->index]      = LFO_OFF;
			part->osc_freq_mod[param->info->index]   = (short)(cc_val - NUM_OSCS - 1);
		}
		else if (cc_val <= (NUM_LFOS + (NUM_OSCS * 2))) {
			part->osc_freq_mod[param->info->index]   = MOD_OFF;
			state->freq_mod_type[param->info->index] = MOD_TYPE_LFO;
			state->freq_lfo[param->info->index]      = (short)(cc_val - (NUM_OSCS * 2) - 1);
		}
		else if (cc_val == (NUM_LFOS + (NUM_OSCS * 2) + 1)) {
			state->freq_mod_type[param->info->index] = MOD_TYPE_VELOCITY;
			state->freq_lfo[param->info->index]      = LFO_OFF;
			part->osc_freq_mod[param->info->index]   = MOD_VELOCITY;
		}
	}
}

/*****************************************************************************
 * update_osc_freq_lfo_amount()
 *****************************************************************************/
void
update_osc_freq_lfo_amount(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->freq_lfo_amount_cc[param->info->index] = (short) cc_val;
	state->freq_lfo_amount[param->info->index]    =
		(((sample_t) int_val)) +
		((sample_t)(state->freq_lfo_fine[param->info->index]) * (1.0 / 120.0));
}

/*****************************************************************************
 * update_osc_freq_lfo_fine()
 *****************************************************************************/
void
update_osc_freq_lfo_fine(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->freq_lfo_fine_cc[param->info->index] = (short) cc_val;
	state->freq_lfo_fine[param->info->index]    = (sample_t) int_val;
	state->freq_lfo_amount[param->info->index]  =
		((sample_t)(state->freq_lfo_amount_cc[param->info->index] - 64)) +
		(((sample_t) int_val) * (1.0 / 120.0));
}

/*****************************************************************************
 * update_osc_phase_lfo()
 *****************************************************************************/
void
update_osc_phase_lfo(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	short           cc_val  = (short) param->value.cc_val;

	if ((cc_val <= 0) || (cc_val > (NUM_LFOS + NUM_OSCS + 1))) {
		state->phase_mod_type[param->info->index] = MOD_TYPE_OFF;
		state->phase_lfo_cc[param->info->index]   = 0;
		state->phase_lfo[param->info->index]      = LFO_OFF;
		part->osc_phase_mod[param->info->index]   = MOD_OFF;
	}
	else {
		state->phase_lfo_cc[param->info->index] = cc_val;
		if (cc_val <= NUM_OSCS) {
			state->phase_mod_type[param->info->index] = MOD_TYPE_OSC;
			state->phase_lfo[param->info->index]      = LFO_OFF;
			part->osc_phase_mod[param->info->index]   = (short)(cc_val - 1);
		}
		else if (cc_val <= (NUM_LFOS + NUM_OSCS)) {
			state->phase_mod_type[param->info->index] = MOD_TYPE_LFO;
			state->phase_lfo[param->info->index]      = (short)(cc_val - NUM_OSCS - 1);
			part->osc_phase_mod[param->info->index]   = MOD_OFF;
		}
		else if (cc_val == (NUM_LFOS + NUM_OSCS + 1)) {
			state->phase_mod_type[param->info->index] = MOD_TYPE_VELOCITY;
			state->phase_lfo[param->info->index]      = LFO_OFF;
			part->osc_phase_mod[param->info->index]   = MOD_VELOCITY;
		}
	}
}

/*****************************************************************************
 * update_osc_phase_lfo_amount()
 *****************************************************************************/
void
update_osc_phase_lfo_amount(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->phase_lfo_amount_cc[param->info->index] = (short) cc_val;
	state->phase_lfo_amount[param->info->index]    = ((sample_t) int_val) / 120.0;
}

/*****************************************************************************
 * update_osc_wave_lfo()
 *****************************************************************************/
void
update_osc_wave_lfo(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->wave_lfo_cc[param->info->index] = (short) cc_val;
	state->wave_lfo[param->info->index]    = (short)(cc_val + NUM_LFOS) % (NUM_LFOS + 1);
}

/*****************************************************************************
 * update_osc_wave_lfo_amount()
 *****************************************************************************/
void
update_osc_wave_lfo_amount(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->wave_lfo_amount_cc[param->info->index] = (short) cc_val;
	state->wave_lfo_amount[param->info->index]    = (sample_t) int_val;
}

/*****************************************************************************
 * update_lfo_wave()
 *****************************************************************************/
void
update_lfo_wave(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->lfo_wave[param->info->index] = (short) cc_val % NUM_WAVEFORMS;
}

/*****************************************************************************
 * update_lfo_freq_base()
 *****************************************************************************/
void
update_lfo_freq_base(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	int             cc_val  = param->value.cc_val;

	state->lfo_freq_base[param->info->index] = (short) cc_val;
	if (state->lfo_freq_base[param->info->index] >= FREQ_BASE_TEMPO) {
		part->lfo_freq[param->info->index]   =
			global.bps * state->lfo_rate[param->info->index];
		part->lfo_adjust[param->info->index] =
			part->lfo_freq[param->info->index] * wave_period;
	}
}

/*****************************************************************************
 * update_lfo_rate()
 *****************************************************************************/
void
update_lfo_rate(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	int             cc_val  = param->value.cc_val;

	state->lfo_rate_cc[param->info->index] = (short) cc_val;
	if (state->lfo_freq_base[param->info->index] >= FREQ_BASE_TEMPO) {
		state->lfo_rate[param->info->index]  = get_rate_val(cc_val);
		part->lfo_freq[param->info->index]   =
			global.bps * state->lfo_rate[param->info->index];
		part->lfo_adjust[param->info->index] =
			part->lfo_freq[param->info->index] * wave_period;
	}
}

/*****************************************************************************
 * update_lfo_polarity()
 *****************************************************************************/
void
update_lfo_polarity(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;

	state->lfo_polarity_cc[param->info->index] = (short) cc_val & 0x01;
}

/*****************************************************************************
 * update_lfo_init_phase()
 *****************************************************************************/
void
update_lfo_init_phase(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->lfo_init_phase_cc[param->info->index] = (short) cc_val;
	state->lfo_init_phase[param->info->index]    = ((sample_t) int_val) / 128.0;
	part->lfo_init_index[param->info->index]     =
		state->lfo_init_phase[param->info->index] * F_WAVEFORM_SIZE;
}

/*****************************************************************************
 * update_lfo_transpose()
 *****************************************************************************/
void
update_lfo_transpose(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;
	VOICE           *voice;
	int             voice_num;

	state->lfo_transpose_cc[param->info->index] = (short) cc_val;
	state->lfo_transpose[param->info->index]    = (short) int_val;
	for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
		voice = get_voice(param->patch->part_num, voice_num);
		voice->need_portamento = 1;
	}
}

/*****************************************************************************
 * update_lfo_pitchbend()
 *****************************************************************************/
void
update_lfo_pitchbend(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->lfo_pitchbend_cc[param->info->index] = (short) cc_val;
	state->lfo_pitchbend[param->info->index]    = (sample_t) int_val;
}

/*****************************************************************************
 * update_lfo_voice_am()
 *****************************************************************************/
void
update_lfo_voice_am(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->lfo_1_voice_am_cc = (short) cc_val;
	state->lfo_1_voice_am    = ((sample_t) int_val) * 0.015625;
}

/*****************************************************************************
 * update_lfo_lfo_rate()
 *****************************************************************************/
void
update_lfo_lfo_rate(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	PART            *part   = param->patch->part;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	if (param->info->index == 1) {
		state->lfo_2_lfo_1_fm_cc  = (short) cc_val;
		state->lfo_2_lfo_1_fm     = (sample_t) int_val;
		part->lfo_freq_lfo_mod[0] = state->lfo_2_lfo_1_fm;
	}
	else if (param->info->index == 3) {
		state->lfo_4_lfo_3_fm_cc  = (short) cc_val;
		state->lfo_4_lfo_3_fm     = (sample_t) int_val;
		part->lfo_freq_lfo_mod[2] = state->lfo_4_lfo_3_fm;
	}
}

/*****************************************************************************
 * update_lfo_cutoff()
 *****************************************************************************/
void
update_lfo_cutoff(PARAM *param)
{
	PATCH_STATE     *state  = param->patch->state;
	int             cc_val  = param->value.cc_val;
	int             int_val = param->value.int_val;

	state->lfo_3_cutoff_cc = (short) cc_val;
	state->lfo_3_cutoff    = (sample_t) int_val;
}
