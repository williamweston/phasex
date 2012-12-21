/*****************************************************************************
 *
 * param.c
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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "phasex.h"
#include "settings.h"
#include "config.h"
#include "param.h"
#include "param_strings.h"
#include "patch.h"
#include "bank.h"
#include "engine.h"
#include "param_cb.h"
#include "bpm.h"
#include "midi_process.h"
#include "debug.h"


PARAM_INFO          phasex_param_info[NUM_HELP_PARAMS];
PARAM_CB_INFO       cb_info[NUM_HELP_PARAMS];

PARAM_NAV_LIST      *param_nav_head = NULL;
PARAM_NAV_LIST      *param_nav_tail = NULL;


/*****************************************************************************
 * get_param()
 *****************************************************************************/
PARAM *
get_param(unsigned int part_num, unsigned int param_id)
{
	PATCH   *patch = get_patch(visible_sess_num, part_num, visible_prog_num[part_num]);

	return & (patch->param[param_id]);
}


/*****************************************************************************
 * get_param_by_name()
 *****************************************************************************/
PARAM *
get_param_by_name(PATCH *patch, char *param_name)
{
	PARAM               *param;
	unsigned int        param_num;

	for (param_num = 0; param_num < NUM_PARAMS; param_num++) {
		param = & (patch->param[param_num]);
		if (strcmp(param_name, param->info->name) == 0) {
			return param;
		}
	}
	return NULL;
}


/*****************************************************************************
 * get_param_id_by_name()
 *****************************************************************************/
int
get_param_id_by_name(char *param_name)
{
	unsigned int        param_num;

	for (param_num = 0; param_num < NUM_PARAMS; param_num++) {
		if (strcmp(param_name, phasex_param_info[param_num].name) == 0) {
			return (int) param_num;
		}
	}
	return -1;
}

/*****************************************************************************
 * get_param_info_by_id()
 *****************************************************************************/
PARAM_INFO *
get_param_info_by_id(unsigned int param_id)
{
	return & (phasex_param_info[param_id]);
}


/*****************************************************************************
 * get_param_cb_info_by_id()
 *****************************************************************************/
PARAM_CB_INFO *
get_param_cb_info_by_id(unsigned int param_id)
{
	return & (cb_info[param_id]);
}


/*****************************************************************************
 * run_param_callbacks()
 *****************************************************************************/
void
run_param_callbacks(int init)
{
	PARAM               *param;
	PATCH               *patch;
	unsigned int        part_num;
	unsigned int        param_num;

	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		patch = get_active_patch(part_num);
		for (param_num = 0; param_num < NUM_PARAMS; param_num++) {
			param = get_param(part_num, param_num);
			cb_info[param_num].update_patch_state(param);
		}
		if (init) {
			patch->modified = 0;
		}
	}
}


/*****************************************************************************
 * init_param_info()
 *
 * Initialize a single parameter global info record.
 *****************************************************************************/
void
init_param_info(unsigned int id, const char *name, const char *label, unsigned int type,
                int cc, int lim, int ccv, int ofst, unsigned int ix, int leap, int sr_dep,
                PARAM_CB callback, const char *label_list[], char *strval_list[])
{
	PARAM_INFO      *info   = get_param_info_by_id(id);
	PARAM_CB_INFO   *cbinfo = get_param_cb_info_by_id(id);
	const char      **numeric_label_list;
	int             j;

	info->id            = id;
	info->name          = name;
	info->label_text    = label;
	info->type          = type;
	info->cc_num        = cc;
	info->cc_limit      = lim;
	info->cc_offset     = ofst;
	info->index         = ix;
	info->leap          = leap;
	info->sr_dep        = sr_dep;
	info->event         = NULL;
	info->adj           = NULL;
	info->knob          = NULL;
	info->spin          = NULL;
	info->combo         = NULL;
	info->text          = NULL;
	info->label         = NULL;
	info->table         = NULL;
	info->button_group  = NULL;
	info->param_nav     = NULL;
	info->list_labels   = label_list;
	info->strval_list   = strval_list;
	info->locked        = 0;
	info->prelight      = 0;
	info->focused       = 0;
	info->sensitive     = 1;
	info->cc_default    = ccv;

	for (j = 0; j < 12; j++) {
		info->button[j]         = NULL;
		info->button_label[j]   = NULL;
	}

	cbinfo->update_patch_state = callback;

	if (id >= NUM_PARAMS) {
		info->locked = 1;
	}

	switch (info->type) {
	case PARAM_TYPE_INT:
	case PARAM_TYPE_REAL:
		if (info->list_labels == NULL) {
			if ((numeric_label_list = malloc(129 * sizeof(char *))) == NULL) {
				phasex_shutdown("Out of Memory!\n");
			}
			for (j = 0; j < 128; j++) {
				if ((numeric_label_list[j] = malloc(5 * sizeof(char))) == NULL) {
					phasex_shutdown("Out of Memory!\n");
				}
				snprintf((char *)(numeric_label_list[j]),
				         (5 * sizeof(char)),
				         "%4d",
				         (j + ofst));
			}
			numeric_label_list[128] = NULL;
			info->list_labels = numeric_label_list;
		}
		break;
	case PARAM_TYPE_LIST:
	case PARAM_TYPE_BOOL:
	case PARAM_TYPE_RATE:
	case PARAM_TYPE_BBOX:
	case PARAM_TYPE_DTNT:
	case PARAM_TYPE_HELP:
	default:
		break;
	}
}


/*****************************************************************************
 * init_params()
 *
 * Map out all parameters.
 * This table is a 1:1 control mapping.
 *****************************************************************************/
void
init_params(void)
{
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_BPM,                   "bpm",                 "BPM",         PARAM_TYPE_INT,  -1, 127,  64,  64, 0,  8, 1, update_bpm,                 NULL,               NULL);
	init_param_info(PARAM_PATCH_TUNE,            "patch_tune",          "Patch Tune",  PARAM_TYPE_INT,  -1, 127,  64, -64, 0, 12, 0, update_patch_tune,          NULL,               NULL);
	init_param_info(PARAM_KEYMODE,               "keymode",             "KeyMode",     PARAM_TYPE_DTNT, -1,   3,   3,   0, 0,  1, 0, update_keymode,             keymode_labels,     keymode_names);
	init_param_info(PARAM_KEYFOLLOW_VOL,         "keyfollow_vol",       "VolKeyFollow", PARAM_TYPE_INT,  -1, 127,  64, -64, 0,  8, 0, update_keyfollow_vol,       NULL,               NULL);
	init_param_info(PARAM_TRANSPOSE,             "transpose",           "Transpose",   PARAM_TYPE_INT,  -1, 127,  64, -64, 0, 12, 0, update_transpose,           NULL,               NULL);
	init_param_info(PARAM_PORTAMENTO,            "portamento",          "Portamento",  PARAM_TYPE_INT,  -1, 127,   0,   0, 0,  8, 0, update_portamento,          NULL,               NULL);
	init_param_info(PARAM_INPUT_BOOST,           "input_boost",         "Input Boost", PARAM_TYPE_REAL, -1, 127,   0,   0, 0,  8, 0, update_input_boost,         NULL,               NULL);
	init_param_info(PARAM_INPUT_FOLLOW,          "input_follow",        "Env Follower", PARAM_TYPE_BOOL, -1,   1,   0,   0, 0,  1, 0, update_input_follow,        on_off_labels,      boolean_names);
	init_param_info(PARAM_PAN,                   "pan",                 "Pan",         PARAM_TYPE_REAL, -1, 127,  64, -64, 0,  8, 0, update_pan,                 NULL,               NULL);
	init_param_info(PARAM_STEREO_WIDTH,          "stereo_width",        "Stereo Width", PARAM_TYPE_REAL, -1, 127, 127,   0, 0,  8, 0, update_stereo_width,        NULL,               NULL);
	init_param_info(PARAM_AMP_VELOCITY,          "amp_velocity",        "Velocity",    PARAM_TYPE_INT,  -1, 127,   0,   0, 0,  8, 0, update_amp_velocity,        NULL,               NULL);
	init_param_info(PARAM_AMP_ATTACK,            "amp_attack",          "Attack",      PARAM_TYPE_INT,  -1, 127,  16,   0, 0,  8, 0, update_amp_attack,          NULL,               NULL);
	init_param_info(PARAM_AMP_DECAY,             "amp_decay",           "Decay",       PARAM_TYPE_INT,  -1, 127,  32,   0, 0,  8, 0, update_amp_decay,           NULL,               NULL);
	init_param_info(PARAM_AMP_SUSTAIN,           "amp_sustain",         "Sustain",     PARAM_TYPE_REAL, -1, 127,  32,   0, 0,  8, 0, update_amp_sustain,         NULL,               NULL);
	init_param_info(PARAM_AMP_RELEASE,           "amp_release",         "Release",     PARAM_TYPE_INT,  -1, 127,  32,   0, 0,  8, 0, update_amp_release,         NULL,               NULL);
	init_param_info(PARAM_VOLUME,                "volume",              "Patch Vol",   PARAM_TYPE_REAL, -1, 127,   0,   0, 0,  8, 0, update_volume,              NULL,               NULL);
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_FILTER_CUTOFF,         "filter_cutoff",       "Cutoff",      PARAM_TYPE_REAL,  1, 127, 127,   0, 0, 12, 0, update_filter_cutoff,       NULL,               NULL);
	init_param_info(PARAM_FILTER_RESONANCE,      "filter_resonance",    "Resonance",   PARAM_TYPE_INT,  -1, 127,  32,   0, 0,  8, 0, update_filter_resonance,    NULL,               NULL);
	init_param_info(PARAM_FILTER_SMOOTHING,      "filter_smoothing",    "Smoothing",   PARAM_TYPE_INT,  -1, 127,   0,   0, 0,  8, 0, update_filter_smoothing,    NULL,               NULL);
	init_param_info(PARAM_FILTER_KEYFOLLOW,      "filter_keyfollow",    "KeyFollow",   PARAM_TYPE_DTNT, -1,   4,   0,   0, 0,  1, 0, update_filter_keyfollow,    keyfollow_labels,   keyfollow_names);
	init_param_info(PARAM_FILTER_MODE,           "filter_mode",         "Mode",        PARAM_TYPE_DTNT, -1,   7,   0,   0, 0,  1, 0, update_filter_mode,         filter_mode_labels, filter_mode_names);
	init_param_info(PARAM_FILTER_TYPE,           "filter_type",         "Type",        PARAM_TYPE_DTNT, -1,   5,   0,   0, 0,  1, 0, update_filter_type,         filter_type_labels, filter_type_names);
	init_param_info(PARAM_FILTER_GAIN,           "filter_gain",         "Filter Gain", PARAM_TYPE_REAL, -1, 127,  64,   0, 0,  8, 0, update_filter_gain,         NULL,               NULL);
	init_param_info(PARAM_FILTER_ENV_AMOUNT,     "filter_env_amount",   "Env Amt",     PARAM_TYPE_REAL, -1, 127,  12,   0, 0, 12, 0, update_filter_env_amount,   NULL,               NULL);
	init_param_info(PARAM_FILTER_ENV_SIGN,       "filter_env_sign",     "Env Sign",    PARAM_TYPE_BOOL, -1,   1,   1,   0, 0,  1, 0, update_filter_env_sign,     sign_labels,        sign_names);
	init_param_info(PARAM_FILTER_LFO,            "filter_lfo",          "LFO",         PARAM_TYPE_BBOX, -1,   4,   0,   0, 0,  1, 0, update_filter_lfo,          velo_lfo_labels,    velo_lfo_names);
	init_param_info(PARAM_FILTER_LFO_CUTOFF,     "filter_lfo_cutoff",   "LFO Cutoff",  PARAM_TYPE_REAL, -1, 127,  64, -64, 0, 12, 0, update_filter_lfo_cutoff,   NULL,               NULL);
	init_param_info(PARAM_FILTER_LFO_RESONANCE,  "filter_lfo_resonance", "LFO Res",     PARAM_TYPE_REAL, -1, 127,  64, -64, 0,  8, 0, update_filter_lfo_resonance, NULL,               NULL);
	init_param_info(PARAM_FILTER_ATTACK,         "filter_attack",       "Attack",      PARAM_TYPE_INT,  -1, 127,   0,   0, 0,  8, 0, update_filter_attack,       NULL,               NULL);
	init_param_info(PARAM_FILTER_DECAY,          "filter_decay",        "Decay",       PARAM_TYPE_INT,  -1, 127,  48,   0, 0,  8, 0, update_filter_decay,        NULL,               NULL);
	init_param_info(PARAM_FILTER_SUSTAIN,        "filter_sustain",      "Sustain",     PARAM_TYPE_REAL, -1, 127,   0,   0, 0,  8, 0, update_filter_sustain,      NULL,               NULL);
	init_param_info(PARAM_FILTER_RELEASE,        "filter_release",      "Release",     PARAM_TYPE_INT,  -1, 127,  32,   0, 0,  8, 0, update_filter_release,      NULL,               NULL);
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_CHORUS_MIX,            "chorus_mix",          "Mix",         PARAM_TYPE_REAL, -1, 127,  32,   0, 0,  8, 0, update_chorus_mix,          NULL,               NULL);
	init_param_info(PARAM_CHORUS_AMOUNT,         "chorus_amount",       "Depth",       PARAM_TYPE_REAL, -1, 127,  32,   0, 0,  8, 0, update_chorus_amount,       NULL,               NULL);
	init_param_info(PARAM_CHORUS_TIME,           "chorus_time",         "Delay Time",  PARAM_TYPE_INT,  -1, 127,   0,   0, 0, 12, 1, update_chorus_time,         NULL,               NULL);
	init_param_info(PARAM_CHORUS_PHASE_RATE,     "chorus_phase_rate",   "Phase Rate",  PARAM_TYPE_RATE, -1, 127,   0,   0, 0,  4, 1, update_chorus_phase_rate,   rate_labels,        rate_names);
	init_param_info(PARAM_CHORUS_PHASE_BALANCE,  "chorus_phase_balance", "Phase Bal",   PARAM_TYPE_REAL, -1, 127,  64,   0, 0,  8, 0, update_chorus_phase_balance, NULL,               NULL);
	init_param_info(PARAM_CHORUS_FEED,           "chorus_feed",         "Feedback",    PARAM_TYPE_REAL, -1, 127,   0,   0, 0,  8, 0, update_chorus_feed,         NULL,               NULL);
	init_param_info(PARAM_CHORUS_LFO_WAVE,       "chorus_lfo_wave",     "LFO Wave",    PARAM_TYPE_DTNT, -1,  27,   0,   0, 0,  1, 0, update_chorus_lfo_wave,     wave_labels,        wave_names);
	init_param_info(PARAM_CHORUS_LFO_RATE,       "chorus_lfo_rate",     "LFO Rate",    PARAM_TYPE_RATE, -1, 127,   0,   0, 0,  4, 1, update_chorus_lfo_rate,     rate_labels,        rate_names);
	init_param_info(PARAM_CHORUS_CROSSOVER,      "chorus_crossover",    "Crossover",   PARAM_TYPE_BOOL, -1,   1,   0,   0, 0,  1, 0, update_chorus_crossover,    on_off_labels,      boolean_names);
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_DELAY_MIX,             "delay_mix",           "Mix",         PARAM_TYPE_REAL, -1, 127,  16,   0, 0,  8, 0, update_delay_mix,           NULL,               NULL);
	init_param_info(PARAM_DELAY_FEED,            "delay_feed",          "Feedback",    PARAM_TYPE_REAL, -1, 127,  16,   0, 0,  8, 0, update_delay_feed,          NULL,               NULL);
	init_param_info(PARAM_DELAY_CROSSOVER,       "delay_crossover",     "Crossover",   PARAM_TYPE_BOOL, -1, 127,   0,   0, 0,  1, 0, update_delay_crossover,     on_off_labels,      boolean_names);
	init_param_info(PARAM_DELAY_TIME,            "delay_time",          "Time",        PARAM_TYPE_RATE, -1, 111,  64,   0, 0,  4, 1, update_delay_time,          rate_labels,        rate_names);
	init_param_info(PARAM_DELAY_LFO,             "delay_lfo",           "LFO",         PARAM_TYPE_BBOX, -1, 127,   0,   0, 0,  1, 0, update_delay_lfo,           lfo_labels,         lfo_names);
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_OSC1_MODULATION,       "osc1_modulation",     "Mix Mod",     PARAM_TYPE_BBOX, -1,   3,   1,   0, 0,  1, 0, update_osc_modulation,      mod_type_labels,    mod_type_names);
	init_param_info(PARAM_OSC1_WAVE,             "osc1_wave",           "Wave",        PARAM_TYPE_DTNT, -1,  27,   0,   0, 0,  1, 0, update_osc_wave,            wave_labels,        wave_names);
	init_param_info(PARAM_OSC1_FREQ_BASE,        "osc1_source",         "Source",      PARAM_TYPE_DTNT, -1,   8,   0,   0, 0,  1, 1, update_osc_freq_base,       freq_base_labels,   freq_base_names);
	init_param_info(PARAM_OSC1_RATE,             "osc1_rate",           "Rate",        PARAM_TYPE_RATE, -1, 127,   0,   0, 0,  4, 1, update_osc_rate,            rate_labels,        rate_names);
	init_param_info(PARAM_OSC1_POLARITY,         "osc1_polarity",       "Polarity",    PARAM_TYPE_BOOL, -1,   1,   0,   0, 0,  1, 0, update_osc_polarity,        polarity_labels,    polarity_names);
	init_param_info(PARAM_OSC1_INIT_PHASE,       "osc1_init_phase",     "Init Phase",  PARAM_TYPE_REAL, -1, 127,   0,   0, 0, 16, 0, update_osc_init_phase,      NULL,               NULL);
	init_param_info(PARAM_OSC1_TRANSPOSE,        "osc1_transpose",      "Transpose",   PARAM_TYPE_INT,  -1, 127,  64, -64, 0, 12, 0, update_osc_transpose,       NULL,               NULL);
	init_param_info(PARAM_OSC1_FINE_TUNE,        "osc1_fine_tune",      "Fine Tune",   PARAM_TYPE_INT,  -1, 127,  64, -64, 0,  8, 0, update_osc_fine_tune,       NULL,               NULL);
	init_param_info(PARAM_OSC1_PITCHBEND,        "osc1_pitchbend",      "Pitchbend",   PARAM_TYPE_REAL, -1, 127,  64, -64, 0, 12, 0, update_osc_pitchbend,       NULL,               NULL);
	init_param_info(PARAM_OSC1_AM_LFO,           "osc1_am_mod",         "AM Mod",      PARAM_TYPE_DTNT, -1,   9,   0,   0, 0,  1, 0, update_osc_am_lfo,          mod_labels,         mod_names);
	init_param_info(PARAM_OSC1_AM_LFO_AMOUNT,    "osc1_am_mod_amount",  "AM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 0,  8, 0, update_osc_am_lfo_amount,   NULL,               NULL);
	init_param_info(PARAM_OSC1_FREQ_LFO,         "osc1_fm_mod",         "FM Mod",      PARAM_TYPE_DTNT, -1,  13,   0,   0, 0,  1, 0, update_osc_freq_lfo,        fm_mod_labels,      fm_mod_names);
	init_param_info(PARAM_OSC1_FREQ_LFO_AMOUNT,  "osc1_fm_mod_amount",  "FM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 0, 12, 0, update_osc_freq_lfo_amount, NULL,               NULL);
	init_param_info(PARAM_OSC1_FREQ_LFO_FINE,    "osc1_fm_mod_fine",    "FM Fine",     PARAM_TYPE_REAL, -1, 127,  64, -64, 0,  8, 0, update_osc_freq_lfo_fine,   NULL,               NULL);
	init_param_info(PARAM_OSC1_PHASE_LFO,        "osc1_pm_mod",         "PM Mod",      PARAM_TYPE_DTNT, -1,   9,   0,   0, 0,  1, 0, update_osc_phase_lfo,       mod_labels,         mod_names);
	init_param_info(PARAM_OSC1_PHASE_LFO_AMOUNT, "osc1_pm_mod_amount",  "PM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 0, 15, 0, update_osc_phase_lfo_amount, NULL,               NULL);
	init_param_info(PARAM_OSC1_WAVE_LFO,         "osc1_wave_lfo",       "Wave LFO",    PARAM_TYPE_BBOX, -1,   4,   4,   0, 0,  1, 0, update_osc_wave_lfo,        lfo_labels,         lfo_names);
	init_param_info(PARAM_OSC1_WAVE_LFO_AMOUNT,  "osc1_wave_lfo_amount", "Wave Amt",    PARAM_TYPE_REAL, -1, 127,  64, -64, 0,  1, 0, update_osc_wave_lfo_amount, NULL,               NULL);
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_OSC2_MODULATION,       "osc2_modulation",     "Mix Mod",     PARAM_TYPE_BBOX, -1,   3,   1,   0, 1,  1, 0, update_osc_modulation,      mod_type_labels,    mod_type_names);
	init_param_info(PARAM_OSC2_WAVE,             "osc2_wave",           "Wave",        PARAM_TYPE_DTNT, -1,  27,   0,   0, 1,  1, 0, update_osc_wave,            wave_labels,        wave_names);
	init_param_info(PARAM_OSC2_FREQ_BASE,        "osc2_source",         "Source",      PARAM_TYPE_DTNT, -1,   8,   0,   0, 1,  1, 1, update_osc_freq_base,       freq_base_labels,   freq_base_names);
	init_param_info(PARAM_OSC2_RATE,             "osc2_rate",           "Rate",        PARAM_TYPE_RATE, -1, 127,   0,   0, 1,  4, 1, update_osc_rate,            rate_labels,        rate_names);
	init_param_info(PARAM_OSC2_POLARITY,         "osc2_polarity",       "Polarity",    PARAM_TYPE_BOOL, -1,   1,   0,   0, 1,  1, 0, update_osc_polarity,        polarity_labels,    polarity_names);
	init_param_info(PARAM_OSC2_INIT_PHASE,       "osc2_init_phase",     "Init Phase",  PARAM_TYPE_REAL, -1, 127,   0,   0, 1, 16, 0, update_osc_init_phase,      NULL,               NULL);
	init_param_info(PARAM_OSC2_TRANSPOSE,        "osc2_transpose",      "Transpose",   PARAM_TYPE_INT,  -1, 127,  64, -64, 1, 12, 0, update_osc_transpose,       NULL,               NULL);
	init_param_info(PARAM_OSC2_FINE_TUNE,        "osc2_fine_tune",      "Fine Tune",   PARAM_TYPE_INT,  -1, 127,  64, -64, 1,  8, 0, update_osc_fine_tune,       NULL,               NULL);
	init_param_info(PARAM_OSC2_PITCHBEND,        "osc2_pitchbend",      "Pitchbend",   PARAM_TYPE_REAL, -1, 127,  64, -64, 1, 12, 0, update_osc_pitchbend,       NULL,               NULL);
	init_param_info(PARAM_OSC2_AM_LFO,           "osc2_am_mod",         "AM Mod",      PARAM_TYPE_DTNT, -1,   9,   0,   0, 1,  1, 0, update_osc_am_lfo,          mod_labels,         mod_names);
	init_param_info(PARAM_OSC2_AM_LFO_AMOUNT,    "osc2_am_mod_amount",  "AM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 1,  8, 0, update_osc_am_lfo_amount,   NULL,               NULL);
	init_param_info(PARAM_OSC2_FREQ_LFO,         "osc2_fm_mod",         "FM Mod",      PARAM_TYPE_DTNT, -1,  13,   0,   0, 1,  1, 0, update_osc_freq_lfo,        fm_mod_labels,      fm_mod_names);
	init_param_info(PARAM_OSC2_FREQ_LFO_AMOUNT,  "osc2_fm_mod_amount",  "FM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 1, 12, 0, update_osc_freq_lfo_amount, NULL,               NULL);
	init_param_info(PARAM_OSC2_FREQ_LFO_FINE,    "osc2_fm_mod_fine",    "FM Fine",     PARAM_TYPE_REAL, -1, 127,  64, -64, 1,  8, 0, update_osc_freq_lfo_fine,   NULL,               NULL);
	init_param_info(PARAM_OSC2_PHASE_LFO,        "osc2_pm_mod",         "PM Mod",      PARAM_TYPE_DTNT, -1,   9,   0,   0, 1,  1, 0, update_osc_phase_lfo,       mod_labels,         mod_names);
	init_param_info(PARAM_OSC2_PHASE_LFO_AMOUNT, "osc2_pm_mod_amount",  "PM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 1, 15, 0, update_osc_phase_lfo_amount, NULL,               NULL);
	init_param_info(PARAM_OSC2_WAVE_LFO,         "osc2_wave_lfo",       "Wave LFO",    PARAM_TYPE_BBOX, -1,   4,   4,   0, 1,  1, 0, update_osc_wave_lfo,        lfo_labels,         lfo_names);
	init_param_info(PARAM_OSC2_WAVE_LFO_AMOUNT,  "osc2_wave_lfo_amount", "Wave Amt",    PARAM_TYPE_REAL, -1, 127,  64, -64, 1,  1, 0, update_osc_wave_lfo_amount, NULL,               NULL);
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_OSC3_MODULATION,       "osc3_modulation",     "Mix Mod",     PARAM_TYPE_BBOX, -1,   3,   1,   0, 2,  1, 0, update_osc_modulation,      mod_type_labels,    mod_type_names);
	init_param_info(PARAM_OSC3_WAVE,             "osc3_wave",           "Wave",        PARAM_TYPE_DTNT, -1,  27,   0,   0, 2,  1, 0, update_osc_wave,            wave_labels,        wave_names);
	init_param_info(PARAM_OSC3_FREQ_BASE,        "osc3_source",         "Source",      PARAM_TYPE_DTNT, -1,   8,   0,   0, 2,  1, 1, update_osc_freq_base,       freq_base_labels,   freq_base_names);
	init_param_info(PARAM_OSC3_RATE,             "osc3_rate",           "Rate",        PARAM_TYPE_RATE, -1, 127,   0,   0, 2,  4, 1, update_osc_rate,            rate_labels,        rate_names);
	init_param_info(PARAM_OSC3_POLARITY,         "osc3_polarity",       "Polarity",    PARAM_TYPE_BOOL, -1,   1,   0,   0, 2,  1, 0, update_osc_polarity,        polarity_labels,    polarity_names);
	init_param_info(PARAM_OSC3_INIT_PHASE,       "osc3_init_phase",     "Init Phase",  PARAM_TYPE_REAL, -1, 127,   0,   0, 2, 16, 0, update_osc_init_phase,      NULL,               NULL);
	init_param_info(PARAM_OSC3_TRANSPOSE,        "osc3_transpose",      "Transpose",   PARAM_TYPE_INT,  -1, 127,  64, -64, 2, 12, 0, update_osc_transpose,       NULL,               NULL);
	init_param_info(PARAM_OSC3_FINE_TUNE,        "osc3_fine_tune",      "Fine Tune",   PARAM_TYPE_INT,  -1, 127,  64, -64, 2,  8, 0, update_osc_fine_tune,       NULL,               NULL);
	init_param_info(PARAM_OSC3_PITCHBEND,        "osc3_pitchbend",      "Pitchbend",   PARAM_TYPE_REAL, -1, 127,  64, -64, 2, 12, 0, update_osc_pitchbend,       NULL,               NULL);
	init_param_info(PARAM_OSC3_AM_LFO,           "osc3_am_mod",         "AM Mod",      PARAM_TYPE_DTNT, -1,   9,   0,   0, 2,  1, 0, update_osc_am_lfo,          mod_labels,         mod_names);
	init_param_info(PARAM_OSC3_AM_LFO_AMOUNT,    "osc3_am_mod_amount",  "AM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 2,  8, 0, update_osc_am_lfo_amount,   NULL,               NULL);
	init_param_info(PARAM_OSC3_FREQ_LFO,         "osc3_fm_mod",         "FM Mod",      PARAM_TYPE_DTNT, -1,  13,   0,   0, 2,  1, 0, update_osc_freq_lfo,        fm_mod_labels,      fm_mod_names);
	init_param_info(PARAM_OSC3_FREQ_LFO_AMOUNT,  "osc3_fm_mod_amount",  "FM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 2, 12, 0, update_osc_freq_lfo_amount, NULL,               NULL);
	init_param_info(PARAM_OSC3_FREQ_LFO_FINE,    "osc3_fm_mod_fine",    "FM Fine",     PARAM_TYPE_REAL, -1, 127,  64, -64, 2,  8, 0, update_osc_freq_lfo_fine,   NULL,               NULL);
	init_param_info(PARAM_OSC3_PHASE_LFO,        "osc3_pm_mod",         "PM Mod",      PARAM_TYPE_DTNT, -1,   9,   0,   0, 2,  1, 0, update_osc_phase_lfo,       mod_labels,         mod_names);
	init_param_info(PARAM_OSC3_PHASE_LFO_AMOUNT, "osc3_pm_mod_amount",  "PM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 2, 15, 0, update_osc_phase_lfo_amount, NULL,               NULL);
	init_param_info(PARAM_OSC3_WAVE_LFO,         "osc3_wave_lfo",       "Wave LFO",    PARAM_TYPE_BBOX, -1,   4,   4,   0, 2,  1, 0, update_osc_wave_lfo,        lfo_labels,         lfo_names);
	init_param_info(PARAM_OSC3_WAVE_LFO_AMOUNT,  "osc3_wave_lfo_amount", "Wave Amt",    PARAM_TYPE_REAL, -1, 127,  64, -64, 2,  1, 0, update_osc_wave_lfo_amount, NULL,               NULL);
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_OSC4_MODULATION,       "osc4_modulation",     "Mix Mod",     PARAM_TYPE_BBOX, -1,   3,   1,   0, 3,  1, 0, update_osc_modulation,      mod_type_labels,    mod_type_names);
	init_param_info(PARAM_OSC4_WAVE,             "osc4_wave",           "Wave",        PARAM_TYPE_DTNT, -1,  27,   0,   0, 3,  1, 0, update_osc_wave,            wave_labels,        wave_names);
	init_param_info(PARAM_OSC4_FREQ_BASE,        "osc4_source",         "Source",      PARAM_TYPE_DTNT, -1,   8,   0,   0, 3,  1, 1, update_osc_freq_base,       freq_base_labels,   freq_base_names);
	init_param_info(PARAM_OSC4_RATE,             "osc4_rate",           "Rate",        PARAM_TYPE_RATE, -1, 127,   0,   0, 3,  4, 1, update_osc_rate,            rate_labels,        rate_names);
	init_param_info(PARAM_OSC4_POLARITY,         "osc4_polarity",       "Polarity",    PARAM_TYPE_BOOL, -1,   1,   0,   0, 3,  1, 0, update_osc_polarity,        polarity_labels,    polarity_names);
	init_param_info(PARAM_OSC4_INIT_PHASE,       "osc4_init_phase",     "Init Phase",  PARAM_TYPE_REAL, -1, 127,   0,   0, 3, 16, 0, update_osc_init_phase,      NULL,               NULL);
	init_param_info(PARAM_OSC4_TRANSPOSE,        "osc4_transpose",      "Transpose",   PARAM_TYPE_INT,  -1, 127,  64, -64, 3, 12, 0, update_osc_transpose,       NULL,               NULL);
	init_param_info(PARAM_OSC4_FINE_TUNE,        "osc4_fine_tune",      "Fine Tune",   PARAM_TYPE_INT,  -1, 127,  64, -64, 3,  8, 0, update_osc_fine_tune,       NULL,               NULL);
	init_param_info(PARAM_OSC4_PITCHBEND,        "osc4_pitchbend",      "Pitchbend",   PARAM_TYPE_REAL, -1, 127,  64, -64, 3, 12, 0, update_osc_pitchbend,       NULL,               NULL);
	init_param_info(PARAM_OSC4_AM_LFO,           "osc4_am_mod",         "AM Mod",      PARAM_TYPE_DTNT, -1,   9,   0,   0, 3,  1, 0, update_osc_am_lfo,          mod_labels,         mod_names);
	init_param_info(PARAM_OSC4_AM_LFO_AMOUNT,    "osc4_am_mod_amount",  "AM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 3,  8, 0, update_osc_am_lfo_amount,   NULL,               NULL);
	init_param_info(PARAM_OSC4_FREQ_LFO,         "osc4_fm_mod",         "FM Mod",      PARAM_TYPE_DTNT, -1,  13,   0,   0, 3,  1, 0, update_osc_freq_lfo,        fm_mod_labels,      fm_mod_names);
	init_param_info(PARAM_OSC4_FREQ_LFO_AMOUNT,  "osc4_fm_mod_amount",  "FM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 3, 12, 0, update_osc_freq_lfo_amount, NULL,               NULL);
	init_param_info(PARAM_OSC4_FREQ_LFO_FINE,    "osc4_fm_mod_fine",    "FM Fine",     PARAM_TYPE_REAL, -1, 127,  64, -64, 3,  8, 0, update_osc_freq_lfo_fine,   NULL,               NULL);
	init_param_info(PARAM_OSC4_PHASE_LFO,        "osc4_pm_mod",         "PM Mod",      PARAM_TYPE_DTNT, -1,   9,   0,   0, 3,  1, 0, update_osc_phase_lfo,       mod_labels,         mod_names);
	init_param_info(PARAM_OSC4_PHASE_LFO_AMOUNT, "osc4_pm_mod_amount",  "PM Amt",      PARAM_TYPE_REAL, -1, 127,  64, -64, 3, 15, 0, update_osc_phase_lfo_amount, NULL,               NULL);
	init_param_info(PARAM_OSC4_WAVE_LFO,         "osc4_wave_lfo",       "Wave LFO",    PARAM_TYPE_BBOX, -1,   4,   4,   0, 3,  1, 0, update_osc_wave_lfo,        lfo_labels,         lfo_names);
	init_param_info(PARAM_OSC4_WAVE_LFO_AMOUNT,  "osc4_wave_lfo_amount", "Wave Amt",    PARAM_TYPE_REAL, -1, 127,  64, -64, 3,  1, 0, update_osc_wave_lfo_amount, NULL,               NULL);
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_LFO1_WAVE,             "lfo1_wave",           "Wave",        PARAM_TYPE_DTNT, -1,  27,   0,   0, 0,  1, 0, update_lfo_wave,            wave_labels,        wave_names);
	init_param_info(PARAM_LFO1_FREQ_BASE,        "lfo1_source",         "Source",      PARAM_TYPE_DTNT, -1,   8,   0,   0, 0,  1, 1, update_lfo_freq_base,       freq_base_labels,   freq_base_names);
	init_param_info(PARAM_LFO1_RATE,             "lfo1_rate",           "Rate",        PARAM_TYPE_RATE, -1, 127,   0,   0, 0,  4, 1, update_lfo_rate,            rate_labels,        rate_names);
	init_param_info(PARAM_LFO1_POLARITY,         "lfo1_polarity",       "Polarity",    PARAM_TYPE_BOOL, -1,   1,   0,   0, 0,  1, 0, update_lfo_polarity,        polarity_labels,    polarity_names);
	init_param_info(PARAM_LFO1_INIT_PHASE,       "lfo1_init_phase",     "Init Phase",  PARAM_TYPE_REAL, -1, 127,   0,   0, 0, 16, 0, update_lfo_init_phase,      NULL,               NULL);
	init_param_info(PARAM_LFO1_TRANSPOSE,        "lfo1_transpose",      "Transpose",   PARAM_TYPE_INT,  -1, 127,  64, -64, 0, 12, 0, update_lfo_transpose,       NULL,               NULL);
	init_param_info(PARAM_LFO1_PITCHBEND,        "lfo1_pitchbend",      "Pitchbend",   PARAM_TYPE_REAL, -1, 127,  64, -64, 0, 12, 0, update_lfo_pitchbend,       NULL,               NULL);
	init_param_info(PARAM_LFO1_VOICE_AM,         "lfo1_voice_am",       "Voice AM",    PARAM_TYPE_REAL, -1, 127,  64, -64, 0,  8, 0, update_lfo_voice_am,        NULL,               NULL);
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_LFO2_WAVE,             "lfo2_wave",           "Wave",        PARAM_TYPE_DTNT, -1,  27,   0,   0, 1,  1, 0, update_lfo_wave,            wave_labels,        wave_names);
	init_param_info(PARAM_LFO2_FREQ_BASE,        "lfo2_source",         "Source",      PARAM_TYPE_DTNT, -1,   8,   0,   0, 1,  1, 1, update_lfo_freq_base,       freq_base_labels,   freq_base_names);
	init_param_info(PARAM_LFO2_RATE,             "lfo2_rate",           "Rate",        PARAM_TYPE_RATE, -1, 127,   0,   0, 1,  4, 1, update_lfo_rate,            rate_labels,        rate_names);
	init_param_info(PARAM_LFO2_POLARITY,         "lfo2_polarity",       "Polarity",    PARAM_TYPE_BOOL, -1,   1,   0,   0, 1,  1, 0, update_lfo_polarity,        polarity_labels,    polarity_names);
	init_param_info(PARAM_LFO2_INIT_PHASE,       "lfo2_init_phase",     "Init Phase",  PARAM_TYPE_REAL, -1, 127,   0,   0, 1, 16, 0, update_lfo_init_phase,      NULL,               NULL);
	init_param_info(PARAM_LFO2_TRANSPOSE,        "lfo2_transpose",      "Transpose",   PARAM_TYPE_INT,  -1, 127,  64, -64, 1, 12, 0, update_lfo_transpose,       NULL,               NULL);
	init_param_info(PARAM_LFO2_PITCHBEND,        "lfo2_pitchbend",      "Pitchbend",   PARAM_TYPE_REAL, -1, 127,  64, -64, 1, 12, 0, update_lfo_pitchbend,       NULL,               NULL);
	init_param_info(PARAM_LFO2_LFO1_FM,          "lfo2_lfo1_fm",        "LFO-1 Rate",  PARAM_TYPE_REAL, -1, 127,  64, -64, 1, 12, 0, update_lfo_lfo_rate,        NULL,               NULL);
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_LFO3_WAVE,             "lfo3_wave",           "Wave",        PARAM_TYPE_DTNT, -1,  27,   0,   0, 2,  1, 0, update_lfo_wave,            wave_labels,        wave_names);
	init_param_info(PARAM_LFO3_FREQ_BASE,        "lfo3_source",         "Source",      PARAM_TYPE_DTNT, -1,   8,   0,   0, 2,  1, 1, update_lfo_freq_base,       freq_base_labels,   freq_base_names);
	init_param_info(PARAM_LFO3_RATE,             "lfo3_rate",           "Rate",        PARAM_TYPE_RATE, -1, 127,   0,   0, 2,  4, 1, update_lfo_rate,            rate_labels,        rate_names);
	init_param_info(PARAM_LFO3_POLARITY,         "lfo3_polarity",       "Polarity",    PARAM_TYPE_BOOL, -1,   1,   0,   0, 2,  1, 0, update_lfo_polarity,        polarity_labels,    polarity_names);
	init_param_info(PARAM_LFO3_INIT_PHASE,       "lfo3_init_phase",     "Init Phase",  PARAM_TYPE_REAL, -1, 127,   0,   0, 2, 16, 0, update_lfo_init_phase,      NULL,               NULL);
	init_param_info(PARAM_LFO3_TRANSPOSE,        "lfo3_transpose",      "Transpose",   PARAM_TYPE_INT,  -1, 127,  64, -64, 2, 12, 0, update_lfo_transpose,       NULL,               NULL);
	init_param_info(PARAM_LFO3_PITCHBEND,        "lfo3_pitchbend",      "Pitchbend",   PARAM_TYPE_REAL, -1, 127,  64, -64, 2, 12, 0, update_lfo_pitchbend,       NULL,               NULL);
	init_param_info(PARAM_LFO3_CUTOFF,           "lfo3_cutoff",         "Cutoff",      PARAM_TYPE_REAL, -1, 127,  64, -64, 2, 12, 0, update_lfo_cutoff,          NULL,               NULL);
	/*              index                         name                   label          type             cc  lim  ccv ofst ix leap    callback                    labels              names */
	init_param_info(PARAM_LFO4_WAVE,             "lfo4_wave",           "Wave",        PARAM_TYPE_DTNT, -1,  27,   0,   0, 3,  1, 0, update_lfo_wave,            wave_labels,        wave_names);
	init_param_info(PARAM_LFO4_FREQ_BASE,        "lfo4_source",         "Source",      PARAM_TYPE_DTNT, -1,   8,   0,   0, 3,  1, 1, update_lfo_freq_base,       freq_base_labels,   freq_base_names);
	init_param_info(PARAM_LFO4_RATE,             "lfo4_rate",           "Rate",        PARAM_TYPE_RATE, -1, 127,   0,   0, 3,  4, 1, update_lfo_rate,            rate_labels,        rate_names);
	init_param_info(PARAM_LFO4_POLARITY,         "lfo4_polarity",       "Polarity",    PARAM_TYPE_BOOL, -1,   1,   0,   0, 3,  1, 0, update_lfo_polarity,        polarity_labels,    polarity_names);
	init_param_info(PARAM_LFO4_INIT_PHASE,       "lfo4_init_phase",     "Init Phase",  PARAM_TYPE_REAL, -1, 127,   0,   0, 3, 16, 0, update_lfo_init_phase,      NULL,               NULL);
	init_param_info(PARAM_LFO4_TRANSPOSE,        "lfo4_transpose",      "Transpose",   PARAM_TYPE_INT,  -1, 127,  64, -64, 3, 12, 0, update_lfo_transpose,       NULL,               NULL);
	init_param_info(PARAM_LFO4_PITCHBEND,        "lfo4_pitchbend",      "Pitchbend",   PARAM_TYPE_REAL, -1, 127,  64, -64, 3, 12, 0, update_lfo_pitchbend,       NULL,               NULL);
	init_param_info(PARAM_LFO4_LFO3_FM,          "lfo4_lfo3_fm",        "LFO-3 Rate",  PARAM_TYPE_REAL, -1, 127,  64, -64, 3, 12, 0, update_lfo_lfo_rate,        NULL,               NULL);

	/* Initialize the pseudo parameters */
	init_param_info(PARAM_MIDI_CHANNEL,          "midi_channel",        "MIDI Channel", PARAM_TYPE_DTNT, -1,  16,   0,   0, 0,  1, 0, update_midi_channel,        midi_ch_labels,     midi_ch_names);
	init_param_info(PARAM_PART_NUMBER,           "part_number",         "Part #",      PARAM_TYPE_HELP, -1,   0,   0,   0, 0,  0, 0, NULL, NULL, NULL);
	init_param_info(PARAM_PROGRAM_NUMBER,        "program_number",      "Program #",   PARAM_TYPE_HELP, -1,   0,   0,   0, 0,  0, 0, NULL, NULL, NULL);
	init_param_info(PARAM_PATCH_NAME,            "patch_name",          "Patch Name",  PARAM_TYPE_HELP, -1,   0,   0,   0, 0,  0, 0, NULL, NULL, NULL);
	init_param_info(PARAM_SESSION_NUMBER,        "session_number",      "Session #",   PARAM_TYPE_HELP, -1,   0,   0,   0, 0,  0, 0, NULL, NULL, NULL);
	init_param_info(PARAM_SESSION_NAME,          "session_name",        "Session Name", PARAM_TYPE_HELP, -1,   0,   0,   0, 0,  0, 0, NULL, NULL, NULL);
	init_param_info(PARAM_PHASEX_HELP,           "using_phasex",        "Using PHASEX", PARAM_TYPE_HELP, -1,   0,   0,   0, 0,  0, 0, NULL, NULL, NULL);
}
