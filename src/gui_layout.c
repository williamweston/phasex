/*****************************************************************************
 *
 * gui_layout.c
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
#include <string.h>
#include <pango/pango.h>
#include <gtk/gtk.h>
#include "phasex.h"
#include "config.h"
#include "patch.h"
#include "param.h"
#include "gui_main.h"
#include "gui_navbar.h"
#include "gui_layout.h"
#include "gui_patch.h"
#include "gui_param.h"
#include "gtkknob.h"
#include "bank.h"
#include "session.h"
#include "settings.h"
#include "help.h"
#include "debug.h"


PARAM_GROUP param_group[NUM_PARAM_GROUPS];
PARAM_PAGE  param_page[NUM_PARAM_PAGES];

int         notebook_order[NUM_PARAM_GROUPS];
int         one_page_order[NUM_PARAM_GROUPS];
int         widescreen_order[NUM_PARAM_GROUPS];


/*****************************************************************************
 * init_param_groups()
 *
 * Map out parameter group layouts, and provide name and label info
 * for these groups.
 *
 * TODO: turn this into an array and an iterator loop
 *****************************************************************************/
void
init_param_groups(void)
{
	int     j = 0;
	int     k = 0;

	k = 0;
	notebook_order[0] = j;
	one_page_order[0] = j;
	widescreen_order[0] = j;
	param_group[j].name = "General";
	param_group[j].label = "<b>General</b>";
	param_group[j].full_x  = 0;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 0;
	param_group[j].wide_x        = 0;
	param_group[j].param_list[k++] = PARAM_BPM;
	param_group[j].param_list[k++] = PARAM_PATCH_TUNE;
	param_group[j].param_list[k++] = PARAM_KEYMODE;
	param_group[j].param_list[k++] = PARAM_KEYFOLLOW_VOL;
	param_group[j].param_list[k++] = PARAM_TRANSPOSE;
	param_group[j].param_list[k++] = PARAM_PORTAMENTO;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[4] = j;
	one_page_order[1] = j;
	widescreen_order[2] = j;
	param_group[j].name = "Input";
	param_group[j].label = "<b>Input</b>";
	param_group[j].full_x  = 0;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 2;
	param_group[j].wide_x        = 1;
	param_group[j].param_list[k++] = PARAM_INPUT_FOLLOW;
	param_group[j].param_list[k++] = PARAM_INPUT_BOOST;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[1] = j;
	one_page_order[2] = j;
	widescreen_order[1] = j;
	param_group[j].name = "Amplifier";
	param_group[j].label = "<b>Amplifier</b>";
	param_group[j].full_x  = 0;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 0;
	param_group[j].wide_x        = 0;
	param_group[j].param_list[k++] = PARAM_VOLUME;
	param_group[j].param_list[k++] = PARAM_PAN;
	param_group[j].param_list[k++] = PARAM_STEREO_WIDTH;
	param_group[j].param_list[k++] = PARAM_AMP_VELOCITY;
	param_group[j].param_list[k++] = PARAM_AMP_ATTACK;
	param_group[j].param_list[k++] = PARAM_AMP_DECAY;
	param_group[j].param_list[k++] = PARAM_AMP_SUSTAIN;
	param_group[j].param_list[k++] = PARAM_AMP_RELEASE;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[5] = j;
	one_page_order[3] = j;
	widescreen_order[3] = j;
	param_group[j].name = "Chorus";
	param_group[j].label = "<b>Chorus</b>";
	param_group[j].full_x  = 0;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 2;
	param_group[j].wide_x        = 1;
	param_group[j].param_list[k++] = PARAM_CHORUS_MIX;
	param_group[j].param_list[k++] = PARAM_CHORUS_AMOUNT;
	param_group[j].param_list[k++] = PARAM_CHORUS_TIME;
	param_group[j].param_list[k++] = PARAM_CHORUS_FEED;
	param_group[j].param_list[k++] = PARAM_CHORUS_CROSSOVER;
	param_group[j].param_list[k++] = PARAM_CHORUS_LFO_WAVE;
	param_group[j].param_list[k++] = PARAM_CHORUS_LFO_RATE;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[2] = j;
	one_page_order[4] = j;
	widescreen_order[10] = j;
	param_group[j].name = "Filter";
	param_group[j].label = "<b>Filter</b>";
	param_group[j].full_x  = 1;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 1;
	param_group[j].wide_x        = 4;
	param_group[j].param_list[k++] = PARAM_FILTER_CUTOFF;
	param_group[j].param_list[k++] = PARAM_FILTER_RESONANCE;
	param_group[j].param_list[k++] = PARAM_FILTER_SMOOTHING;
	param_group[j].param_list[k++] = PARAM_FILTER_KEYFOLLOW;
	param_group[j].param_list[k++] = PARAM_FILTER_MODE;
	param_group[j].param_list[k++] = PARAM_FILTER_TYPE;
	param_group[j].param_list[k++] = PARAM_FILTER_LFO;
	param_group[j].param_list[k++] = PARAM_FILTER_LFO_CUTOFF;
	param_group[j].param_list[k++] = PARAM_FILTER_LFO_RESONANCE;
	param_group[j].param_list[k++] = PARAM_FILTER_GAIN;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[3] = j;
	one_page_order[5] = j;
	widescreen_order[11] = j;
	param_group[j].name = "Filter Envelope";
	param_group[j].label = "<b>Filter Envelope</b>";
	param_group[j].full_x  = 1;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 1;
	param_group[j].wide_x        = 4;
	param_group[j].param_list[k++] = PARAM_FILTER_ENV_SIGN;
	param_group[j].param_list[k++] = PARAM_FILTER_ENV_AMOUNT;
	param_group[j].param_list[k++] = PARAM_FILTER_ATTACK;
	param_group[j].param_list[k++] = PARAM_FILTER_DECAY;
	param_group[j].param_list[k++] = PARAM_FILTER_SUSTAIN;
	param_group[j].param_list[k++] = PARAM_FILTER_RELEASE;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[7] = j;
	one_page_order[6] = j;
	widescreen_order[5] = j;
	param_group[j].name = "Delay";
	param_group[j].label = "<b>Delay</b>";
	param_group[j].full_x  = 1;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 2;
	param_group[j].wide_x        = 1;
	param_group[j].param_list[k++] = PARAM_DELAY_MIX;
	param_group[j].param_list[k++] = PARAM_DELAY_FEED;
	param_group[j].param_list[k++] = PARAM_DELAY_TIME;
	param_group[j].param_list[k++] = PARAM_DELAY_CROSSOVER;
	param_group[j].param_list[k++] = PARAM_DELAY_LFO;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[6] = j;
	one_page_order[7] = j;
	widescreen_order[4] = j;
	param_group[j].name = "Chorus Phaser";
	param_group[j].label = "<b>Chorus Phaser</b>";
	param_group[j].full_x  = 1;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 2;
	param_group[j].wide_x        = 1;
	param_group[j].param_list[k++] = PARAM_CHORUS_PHASE_RATE;
	param_group[j].param_list[k++] = PARAM_CHORUS_PHASE_BALANCE;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[12] = j;
	one_page_order[8] = j;
	widescreen_order[12] = j;
	param_group[j].name = "Osc-1";
	param_group[j].label = "<b>Osc-1</b>";
	param_group[j].full_x  = 2;
	param_group[j].notebook_page = 1;
	param_group[j].notebook_x    = 0;
	param_group[j].wide_x        = 5;
	param_group[j].param_list[k++] = PARAM_OSC1_MODULATION;
	param_group[j].param_list[k++] = PARAM_OSC1_POLARITY;
	param_group[j].param_list[k++] = PARAM_OSC1_FREQ_BASE;
	param_group[j].param_list[k++] = PARAM_OSC1_WAVE;
	param_group[j].param_list[k++] = PARAM_OSC1_RATE;
	param_group[j].param_list[k++] = PARAM_OSC1_INIT_PHASE;
	param_group[j].param_list[k++] = PARAM_OSC1_TRANSPOSE;
	param_group[j].param_list[k++] = PARAM_OSC1_FINE_TUNE;
	param_group[j].param_list[k++] = PARAM_OSC1_PITCHBEND;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[13] = j;
	one_page_order[9] = j;
	widescreen_order[13] = j;
	param_group[j].name = "Osc-1 Modulators";
	param_group[j].label = "<b>Osc-1 Modulators</b>";
	param_group[j].full_x  = 2;
	param_group[j].notebook_page = 1;
	param_group[j].notebook_x    = 0;
	param_group[j].wide_x        = 5;
	param_group[j].param_list[k++] = PARAM_OSC1_AM_LFO;
	param_group[j].param_list[k++] = PARAM_OSC1_AM_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC1_FREQ_LFO;
	param_group[j].param_list[k++] = PARAM_OSC1_FREQ_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC1_FREQ_LFO_FINE;
	param_group[j].param_list[k++] = PARAM_OSC1_PHASE_LFO;
	param_group[j].param_list[k++] = PARAM_OSC1_PHASE_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC1_WAVE_LFO;
	param_group[j].param_list[k++] = PARAM_OSC1_WAVE_LFO_AMOUNT;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[8] = j;
	one_page_order[10] = j;
	widescreen_order[6] = j;
	param_group[j].name = "LFO-1";
	param_group[j].label = "<b>LFO-1</b>";
	param_group[j].full_x  = 2;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 3;
	param_group[j].wide_x        = 2;
	param_group[j].param_list[k++] = PARAM_LFO1_POLARITY;
	param_group[j].param_list[k++] = PARAM_LFO1_FREQ_BASE;
	param_group[j].param_list[k++] = PARAM_LFO1_WAVE;
	param_group[j].param_list[k++] = PARAM_LFO1_RATE;
	param_group[j].param_list[k++] = PARAM_LFO1_INIT_PHASE;
	param_group[j].param_list[k++] = PARAM_LFO1_TRANSPOSE;
	param_group[j].param_list[k++] = PARAM_LFO1_PITCHBEND;
	param_group[j].param_list[k++] = PARAM_LFO1_VOICE_AM;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[14] = j;
	one_page_order[11] = j;
	widescreen_order[14] = j;
	param_group[j].name = "Osc-2";
	param_group[j].label = "<b>Osc-2</b>";
	param_group[j].full_x  = 3;
	param_group[j].notebook_page = 1;
	param_group[j].notebook_x    = 1;
	param_group[j].wide_x        = 6;
	param_group[j].param_list[k++] = PARAM_OSC2_MODULATION;
	param_group[j].param_list[k++] = PARAM_OSC2_POLARITY;
	param_group[j].param_list[k++] = PARAM_OSC2_FREQ_BASE;
	param_group[j].param_list[k++] = PARAM_OSC2_WAVE;
	param_group[j].param_list[k++] = PARAM_OSC2_RATE;
	param_group[j].param_list[k++] = PARAM_OSC2_INIT_PHASE;
	param_group[j].param_list[k++] = PARAM_OSC2_TRANSPOSE;
	param_group[j].param_list[k++] = PARAM_OSC2_FINE_TUNE;
	param_group[j].param_list[k++] = PARAM_OSC2_PITCHBEND;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[15] = j;
	one_page_order[12] = j;
	widescreen_order[15] = j;
	param_group[j].name = "Osc-2 Modulators";
	param_group[j].label = "<b>Osc-2 Modulators</b>";
	param_group[j].full_x  = 3;
	param_group[j].notebook_page = 1;
	param_group[j].notebook_x    = 1;
	param_group[j].wide_x        = 6;
	param_group[j].param_list[k++] = PARAM_OSC2_AM_LFO;
	param_group[j].param_list[k++] = PARAM_OSC2_AM_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC2_FREQ_LFO;
	param_group[j].param_list[k++] = PARAM_OSC2_FREQ_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC2_FREQ_LFO_FINE;
	param_group[j].param_list[k++] = PARAM_OSC2_PHASE_LFO;
	param_group[j].param_list[k++] = PARAM_OSC2_PHASE_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC2_WAVE_LFO;
	param_group[j].param_list[k++] = PARAM_OSC2_WAVE_LFO_AMOUNT;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[10] = j;
	one_page_order[13] = j;
	widescreen_order[8] = j;
	param_group[j].name = "LFO-2";
	param_group[j].label = "<b>LFO-2</b>";
	param_group[j].full_x  = 3;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 4;
	param_group[j].wide_x        = 3;
	param_group[j].param_list[k++] = PARAM_LFO2_POLARITY;
	param_group[j].param_list[k++] = PARAM_LFO2_FREQ_BASE;
	param_group[j].param_list[k++] = PARAM_LFO2_WAVE;
	param_group[j].param_list[k++] = PARAM_LFO2_RATE;
	param_group[j].param_list[k++] = PARAM_LFO2_INIT_PHASE;
	param_group[j].param_list[k++] = PARAM_LFO2_TRANSPOSE;
	param_group[j].param_list[k++] = PARAM_LFO2_PITCHBEND;
	param_group[j].param_list[k++] = PARAM_LFO2_LFO1_FM;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[16] = j;
	one_page_order[14] = j;
	widescreen_order[16] = j;
	param_group[j].name = "Osc-3";
	param_group[j].label = "<b>Osc-3</b>";
	param_group[j].full_x  = 4;
	param_group[j].notebook_page = 1;
	param_group[j].notebook_x    = 2;
	param_group[j].wide_x        = 7;
	param_group[j].param_list[k++] = PARAM_OSC3_MODULATION;
	param_group[j].param_list[k++] = PARAM_OSC3_POLARITY;
	param_group[j].param_list[k++] = PARAM_OSC3_FREQ_BASE;
	param_group[j].param_list[k++] = PARAM_OSC3_WAVE;
	param_group[j].param_list[k++] = PARAM_OSC3_RATE;
	param_group[j].param_list[k++] = PARAM_OSC3_INIT_PHASE;
	param_group[j].param_list[k++] = PARAM_OSC3_TRANSPOSE;
	param_group[j].param_list[k++] = PARAM_OSC3_FINE_TUNE;
	param_group[j].param_list[k++] = PARAM_OSC3_PITCHBEND;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[17] = j;
	one_page_order[15] = j;
	widescreen_order[17] = j;
	param_group[j].name = "Osc-3 Modulators";
	param_group[j].label = "<b>Osc-3 Modulators</b>";
	param_group[j].full_x  = 4;
	param_group[j].notebook_page = 1;
	param_group[j].notebook_x    = 2;
	param_group[j].wide_x        = 7;
	param_group[j].param_list[k++] = PARAM_OSC3_AM_LFO;
	param_group[j].param_list[k++] = PARAM_OSC3_AM_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC3_FREQ_LFO;
	param_group[j].param_list[k++] = PARAM_OSC3_FREQ_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC3_FREQ_LFO_FINE;
	param_group[j].param_list[k++] = PARAM_OSC3_PHASE_LFO;
	param_group[j].param_list[k++] = PARAM_OSC3_PHASE_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC3_WAVE_LFO;
	param_group[j].param_list[k++] = PARAM_OSC3_WAVE_LFO_AMOUNT;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[9] = j;
	one_page_order[16] = j;
	widescreen_order[7] = j;
	param_group[j].name = "LFO-3";
	param_group[j].label = "<b>LFO-3</b>";
	param_group[j].full_x  = 4;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 3;
	param_group[j].wide_x        = 2;
	param_group[j].param_list[k++] = PARAM_LFO3_POLARITY;
	param_group[j].param_list[k++] = PARAM_LFO3_FREQ_BASE;
	param_group[j].param_list[k++] = PARAM_LFO3_WAVE;
	param_group[j].param_list[k++] = PARAM_LFO3_RATE;
	param_group[j].param_list[k++] = PARAM_LFO3_INIT_PHASE;
	param_group[j].param_list[k++] = PARAM_LFO3_TRANSPOSE;
	param_group[j].param_list[k++] = PARAM_LFO3_PITCHBEND;
	param_group[j].param_list[k++] = PARAM_LFO3_CUTOFF;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[18] = j;
	one_page_order[17] = j;
	widescreen_order[18] = j;
	param_group[j].name = "Osc-4";
	param_group[j].label = "<b>Osc-4</b>";
	param_group[j].full_x  = 5;
	param_group[j].notebook_page = 1;
	param_group[j].notebook_x    = 3;
	param_group[j].wide_x        = 8;
	param_group[j].param_list[k++] = PARAM_OSC4_MODULATION;
	param_group[j].param_list[k++] = PARAM_OSC4_POLARITY;
	param_group[j].param_list[k++] = PARAM_OSC4_FREQ_BASE;
	param_group[j].param_list[k++] = PARAM_OSC4_WAVE;
	param_group[j].param_list[k++] = PARAM_OSC4_RATE;
	param_group[j].param_list[k++] = PARAM_OSC4_INIT_PHASE;
	param_group[j].param_list[k++] = PARAM_OSC4_TRANSPOSE;
	param_group[j].param_list[k++] = PARAM_OSC4_FINE_TUNE;
	param_group[j].param_list[k++] = PARAM_OSC4_PITCHBEND;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[19] = j;
	one_page_order[18] = j;
	widescreen_order[19] = j;
	param_group[j].name = "Osc-4 Modulators";
	param_group[j].label = "<b>Osc-4 Modulators</b>";
	param_group[j].full_x  = 5;
	param_group[j].notebook_page = 1;
	param_group[j].notebook_x    = 3;
	param_group[j].wide_x        = 8;
	param_group[j].param_list[k++] = PARAM_OSC4_AM_LFO;
	param_group[j].param_list[k++] = PARAM_OSC4_AM_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC4_FREQ_LFO;
	param_group[j].param_list[k++] = PARAM_OSC4_FREQ_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC4_FREQ_LFO_FINE;
	param_group[j].param_list[k++] = PARAM_OSC4_PHASE_LFO;
	param_group[j].param_list[k++] = PARAM_OSC4_PHASE_LFO_AMOUNT;
	param_group[j].param_list[k++] = PARAM_OSC4_WAVE_LFO;
	param_group[j].param_list[k++] = PARAM_OSC4_WAVE_LFO_AMOUNT;
	param_group[j].param_list[k++] = -1;
	j++;
	k = 0;
	notebook_order[11] = j;
	one_page_order[19] = j;
	widescreen_order[9] = j;
	param_group[j].name = "LFO-4";
	param_group[j].label = "<b>LFO-4</b>";
	param_group[j].full_x  = 5;
	param_group[j].notebook_page = 0;
	param_group[j].notebook_x    = 4;
	param_group[j].wide_x        = 3;
	param_group[j].param_list[k++] = PARAM_LFO4_POLARITY;
	param_group[j].param_list[k++] = PARAM_LFO4_FREQ_BASE;
	param_group[j].param_list[k++] = PARAM_LFO4_WAVE;
	param_group[j].param_list[k++] = PARAM_LFO4_RATE;
	param_group[j].param_list[k++] = PARAM_LFO4_INIT_PHASE;
	param_group[j].param_list[k++] = PARAM_LFO4_TRANSPOSE;
	param_group[j].param_list[k++] = PARAM_LFO4_PITCHBEND;
	param_group[j].param_list[k++] = PARAM_LFO4_LFO3_FM;
	param_group[j].param_list[k++] = -1;
}


/*****************************************************************************
 * init_param_pages()
 *
 * Map out the parameter notebook pages for the GUI.
 * Each page has two columns of n parameter groups.
 * Parameter groups are read in order of index.
 *****************************************************************************/
void
init_param_pages(void)
{
	int     j = 0;

	param_page[j].label = "     Main    ";
	j++;
	param_page[j].label = " Oscillators ";
	j++;
}


/*****************************************************************************
 * create_param_group()
 *
 * Create a group of parameter input widgets complete with a frame,
 * and attach to a vbox.
 *****************************************************************************/
void
create_param_group(GtkWidget *main_window, GtkWidget *parent_vbox,
                   PARAM_GROUP *param_group, int page_num)
{
	GtkWidget       *frame;
	GtkWidget       *table;
	GtkWidget       *label;
	GtkWidget       *event;
	GtkWidget       *frame_event;
	unsigned int    num_params          = 0;
	unsigned int    param_num;

	/* Find number of parameters */
	while ((num_params < 15) && (param_group->param_list[num_params] > -1)) {
		num_params++;
	}

	/* Create frame with label, alignment, and table */
	frame = gtk_frame_new(NULL);
	widget_set_backing_store(frame);
	gtk_widget_set_name(frame, param_group->name);

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	label = gtk_label_new(param_group->label);
	gtk_widget_set_name(label, "GroupName");
	widget_set_custom_font(label, title_font_desc);
	widget_set_backing_store(label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_container_add(GTK_CONTAINER(event), label);
	gtk_frame_set_label_widget(GTK_FRAME(frame), event);

	frame_event = gtk_event_box_new();
	widget_set_backing_store(frame_event);
	gtk_container_add(GTK_CONTAINER(frame), frame_event);

	table = gtk_table_new(num_params, 3, FALSE);
	widget_set_backing_store(table);
	gtk_table_set_col_spacings(GTK_TABLE(table), 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 0);
	gtk_container_add(GTK_CONTAINER(frame_event), table);

	event = gtk_event_box_new();
	widget_set_backing_store(event);

	/* Create each parameter input within the group */
	for (param_num = 0; param_num < num_params; param_num++) {
		create_param_input(main_window, table, 0, param_num,
		                   & (gp->param[param_group->param_list[param_num]]), page_num);
	}

	/* Attach whole frame to parent */
	gtk_container_add(GTK_CONTAINER(event), frame);
	gtk_box_pack_start(GTK_BOX(parent_vbox), event, TRUE, TRUE, 0);

	/* keep track of widgets */
	param_group->event       = event;
	param_group->frame       = frame;
	param_group->frame_event = frame_event;
	param_group->table       = table;
}


/*****************************************************************************
 * create_param_notebook_page()
 *
 * Create a page of parameter input widgets.
 * Designed to be run out of a loop.
 *****************************************************************************/
void
create_param_notebook_page(GtkWidget *main_window, GtkWidget *notebook,
                           PARAM_PAGE *param_page, int page_num)
{
	GtkWidget       *label;
	GtkWidget       *page;
	GtkWidget       *event;
	GtkWidget       *hbox;
	GtkWidget       *vbox;
	int             j         = 0;
	int             k         = 0;
	int             x         = 0;
	int             max_x     = 0;

	/* Find number of columns for this notebook page */
	for (j = 0; j < NUM_PARAM_GROUPS; j++) {
		if ((param_group[j].notebook_page == page_num) && (param_group[j].notebook_x > max_x)) {
			max_x = param_group[j].notebook_x;
		}
	}

	/* Start with an hbox, so param group columns can be attached,
	   same as in one_page mode. */
	event = gtk_event_box_new();
	widget_set_backing_store(event);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(event), hbox);

	/* Run through all param groups, and add the ones for this page */
	for (x = 0; x <= max_x; x++) {
		vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
		for (j = 0; j < NUM_PARAM_GROUPS; j++) {
			k = notebook_order[j];
			if ((param_group[k].notebook_page == page_num) &&
			    (param_group[k].notebook_x == x)) {
				create_param_group(main_window, vbox, & (param_group[k]), page_num);
			}
		}
	}

	gtk_container_add(GTK_CONTAINER(notebook), event);

	/* Set the label */
	label = gtk_label_new(param_page->label);
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);

	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page_num);

#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(notebook), page, FALSE);
#endif
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook), page, label);
}


/*****************************************************************************
 * create_param_notebook()
 *
 * Create notebook of parameters, all ready to go.
 *****************************************************************************/
void
create_param_notebook(GtkWidget  *main_window,
                      GtkWidget  *box,
                      PARAM_PAGE *param_page)
{
	GtkWidget       *notebook;
	int             page_num;

	/* create a notebook and attach it to the main window */
	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);

	/* build the pages for the notebook */
	for (page_num = 0; page_num < NUM_PARAM_PAGES; page_num++) {
		create_param_notebook_page(main_window, notebook, & (param_page[page_num]), page_num);
	}

	gtk_box_pack_end(GTK_BOX(box), notebook, TRUE, TRUE, 0);

	/* create the patch bank group at the top */
	create_navbar(main_window, box);
}


/*****************************************************************************
 * create_param_one_page()
 *
 * Create one square-ish page of parameters.
 *****************************************************************************/
void
create_param_one_page(GtkWidget  *main_window,
                      GtkWidget  *box,
                      PARAM_PAGE *UNUSED(param_page))
{
	GtkWidget       *hbox;
	GtkWidget       *vbox;
	int             j;
	int             k;
	int             x;
	int             max_x     = 0;

	/* Find number of columns */
	for (j = 0; j < NUM_PARAM_GROUPS; j++) {
		if (param_group[j].full_x > max_x) {
			max_x = param_group[j].full_x;
		}
	}

	/* create an hbox to pack columns of param groups into */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(box), hbox, TRUE, TRUE, 0);

	/* create the param groups that will be attached to this table */
	for (x = 0; x <= max_x; x++) {
		vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
		for (j = 0; j < NUM_PARAM_GROUPS; j++) {
			k = one_page_order[j];
			if (param_group[k].full_x == x) {
				create_param_group(main_window, vbox, & (param_group[k]), 0);
			}
		}
	}

	/* create the patch bank group at the top */
	create_navbar(main_window, box);
}


/*****************************************************************************
 * create_param_widescreen()
 *
 * Create one page of parameters in a widescreen format.
 *****************************************************************************/
void
create_param_widescreen(GtkWidget  *main_window,
                        GtkWidget  *box,
                        PARAM_PAGE *UNUSED(param_page))
{
	GtkWidget       *hbox;
	GtkWidget       *vbox;
	int             j;
	int             k;
	int             x;
	int             max_x     = 0;

	/* Find number of columns */
	for (j = 0; j < NUM_PARAM_GROUPS; j++) {
		if (param_group[j].wide_x > max_x) {
			max_x = param_group[j].wide_x;
		}
	}

	/* create an hbox to pack columns of param groups into */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(box), hbox, TRUE, TRUE, 0);

	/* create the param groups that will be attached to this table */
	for (x = 0; x <= max_x; x++) {
		vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
		for (j = 0; j < NUM_PARAM_GROUPS; j++) {
			k = widescreen_order[j];
			if (param_group[k].wide_x == x) {
				create_param_group(main_window, vbox, & (param_group[k]), 0);
			}
		}
	}

	/* create the patch bank group at the top */
	create_navbar(main_window, box);
}
