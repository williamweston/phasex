/*****************************************************************************
 *
 * param_cb.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2009 William Weston <william.h.weston@gmail.com>
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
#ifndef _PHASEX_PARAM_CB_H_
#define _PHASEX_PARAM_CB_H_

#include "param.h"


void update_midi_channel(PARAM *param);
void update_bpm(PARAM *param);
void update_patch_tune(PARAM *param);
void update_portamento(PARAM *param);
void update_keymode(PARAM *param);
void update_keyfollow_vol(PARAM *param);
void update_volume(PARAM *param);
void update_transpose(PARAM *param);
void update_input_boost(PARAM *param);
void update_input_follow(PARAM *param);
void update_pan(PARAM *param);
void update_stereo_width(PARAM *param);
void update_amp_velocity(PARAM *param);
void update_filter_cutoff(PARAM *param);
void update_filter_resonance(PARAM *param);
void update_filter_smoothing(PARAM *param);
void update_filter_keyfollow(PARAM *param);
void update_filter_mode(PARAM *param);
void update_filter_type(PARAM *param);
void update_filter_gain(PARAM *param);
void update_filter_env_amount(PARAM *param);
void update_filter_env_sign(PARAM *param);
void update_filter_attack(PARAM *param);
void update_filter_decay(PARAM *param);
void update_filter_sustain(PARAM *param);
void update_filter_release(PARAM *param);
void update_filter_lfo(PARAM *param);
void update_filter_lfo_cutoff(PARAM *param);
void update_filter_lfo_resonance(PARAM *param);
void update_amp_attack(PARAM *param);
void update_amp_decay(PARAM *param);
void update_amp_sustain(PARAM *param);
void update_amp_release(PARAM *param);
void update_delay_mix(PARAM *param);
void update_delay_feed(PARAM *param);
void update_delay_crossover(PARAM *param);
void update_delay_time(PARAM *param);
void update_delay_lfo(PARAM *param);
void update_chorus_mix(PARAM *param);
void update_chorus_feed(PARAM *param);
void update_chorus_crossover(PARAM *param);
void update_chorus_time(PARAM *param);
void update_chorus_amount(PARAM *param);
void update_chorus_phase_rate(PARAM *param);
void update_chorus_phase_balance(PARAM *param);
void update_chorus_lfo_wave(PARAM *param);
void update_chorus_lfo_rate(PARAM *param);
void update_osc_modulation(PARAM *param);
void update_osc_wave(PARAM *param);
void update_osc_freq_base(PARAM *param);
void update_osc_rate(PARAM *param);
void update_osc_polarity(PARAM *param);
void update_osc_init_phase(PARAM *param);
void update_osc_transpose(PARAM *param);
void update_osc_fine_tune(PARAM *param);
void update_osc_pitchbend(PARAM *param);
void update_osc_am_lfo(PARAM *param);
void update_osc_am_lfo_amount(PARAM *param);
void update_osc_freq_lfo(PARAM *param);
void update_osc_freq_lfo_amount(PARAM *param);
void update_osc_freq_lfo_fine(PARAM *param);
void update_osc_phase_lfo(PARAM *param);
void update_osc_phase_lfo_amount(PARAM *param);
void update_osc_wave_lfo(PARAM *param);
void update_osc_wave_lfo_amount(PARAM *param);
void update_lfo_wave(PARAM *param);
void update_lfo_freq_base(PARAM *param);
void update_lfo_rate(PARAM *param);
void update_lfo_polarity(PARAM *param);
void update_lfo_init_phase(PARAM *param);
void update_lfo_transpose(PARAM *param);
void update_lfo_pitchbend(PARAM *param);
void update_lfo_voice_am(PARAM *param);
void update_lfo_lfo_rate(PARAM *param);
void update_lfo_cutoff(PARAM *param);


#endif /* _PHASEX_PARAM_CB_H_ */
