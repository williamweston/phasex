/*****************************************************************************
 *
 * param_strings.c
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
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>



/*****************************************************************************
 * all param value name lists needed for patch io
 *****************************************************************************/

/* time based rate values */
char *rate_names[] = {
	"1/128",
	"1/64",
	"1/32",
	"3/64",
	"1/16",
	"5/64",
	"3/32",
	"7/64",
	"1/8",
	"9/64",
	"5/32",
	"11/64",
	"3/16",
	"13/64",
	"7/32",
	"15/64",
	"1/4",
	"17/64",
	"9/32",
	"19/64",
	"5/16",
	"21/64",
	"11/32",
	"23/64",
	"3/8",
	"25/64",
	"13/32",
	"27/64",
	"7/16",
	"29/64",
	"15/32",
	"31/64",
	"1/2",
	"33/64",
	"17/32",
	"35/64",
	"9/16",
	"37/64",
	"19/32",
	"39/64",
	"5/8",
	"41/64",
	"21/32",
	"43/64",
	"11/16",
	"45/64",
	"23/32",
	"47/64",
	"3/4",
	"49/64",
	"25/32",
	"51/64",
	"13/16",
	"53/64",
	"27/32",
	"55/64",
	"7/8",
	"57/64",
	"29/32",
	"59/64",
	"15/16",
	"61/64",
	"31/32",
	"63/64",
	"1/1",
	"1/48",
	"1/24",
	"1/16",
	"1/12",
	"5/48",
	"1/8",
	"7/48",
	"1/6",
	"3/16",
	"5/24",
	"11/48",
	"1/4",
	"13/48",
	"7/24",
	"5/16",
	"1/3",
	"17/48",
	"3/8",
	"19/48",
	"5/12",
	"7/16",
	"11/24",
	"23/48",
	"1/2",
	"25/48",
	"13/24",
	"9/16",
	"7/12",
	"29/48",
	"5/8",
	"31/48",
	"2/3",
	"11/16",
	"17/24",
	"35/48",
	"3/4",
	"37/48",
	"19/24",
	"13/16",
	"5/6",
	"41/48",
	"7/8",
	"43/48",
	"11/12",
	"15/16",
	"23/24",
	"47/48",
	"1/1",
	"2/1",
	"3/1",
	"4/1",
	"5/1",
	"6/1",
	"7/1",
	"8/1",
	"9/1",
	"10/1",
	"11/1",
	"12/1",
	"13/1",
	"14/1",
	"15/1",
	"16/1",
	NULL
};

/* Key mapping modes */
char *keymode_names[] = {
	"mono_smooth",
	"mono_retrig",
	"mono_multikey",
	"poly",
	NULL
};

/* Keyfollow modes */
char *keyfollow_names[] = {
	"off",
	"last",
	"high",
	"low",
	"keytrig",
	NULL
};

/* Names of modulators (oscs and lfos) */
char *mod_names[] = {
	"off",
	"osc_1",
	"osc_2",
	"osc_3",
	"osc_4",
	"lfo_1",
	"lfo_2",
	"lfo_3",
	"lfo_4",
	"velocity",
	NULL
};

/* Names of modulators (oscs and lfos) */
char *fm_mod_names[] = {
	"off",
	"osc_1",
	"osc_2",
	"osc_3",
	"osc_4",
	"osc_1_latch",
	"osc_2_latch",
	"osc_3_latch",
	"osc_4_latch",
	"lfo_1",
	"lfo_2",
	"lfo_3",
	"lfo_4",
	"velocity",
	NULL
};

/* Names of waves in osc_table */
char *wave_names[] = {
	"sine",
	"triangle",
	"saw",
	"revsaw",
	"square",
	"stair",
	"bl_saw",
	"bl_revsaw",
	"bl_square",
	"bl_stair",
	"juno_osc",
	"juno_saw",
	"juno_square",
	"juno_poly",
	"analog_square",
	"vox_1",
	"vox_2",
	"poly_sine",
	"poly_saw",
	"poly_revsaw",
	"poly_square_1",
	"poly_square_2",
	"poly_1",
	"poly_2",
	"poly_3",
	"poly_4",
	"null",
	"identity",
	NULL
};

/* Filter modes */
char *filter_mode_names[] = {
	"lp",
	"hp",
	"bp",
	"bs",
	"lp+bp",
	"hp+bp",
	"lp+hp",
	"bs+bp",
	"experiment",
	NULL
};

/* Filter types */
char *filter_type_names[] = {
	"dist",
	"retro",
	"moog",
	"clean_moog",
	"3pole_dist",
	"3pole_raw",
	NULL
};

/* Frequency basees */
char *freq_base_names[] = {
	"midi_key",
	"input_1",
	"input_2",
	"input_stereo",
	"amp_env",
	"filter_env",
	"velocity",
	"tempo",
	"keytrig",
	NULL
};

/* Modulation types */
char *mod_type_names[] = {
	"off",
	"mix",
	"am",
	"mod",
	NULL
};

/* lfo modsource names */
char *lfo_names[] = {
	"off",
	"lfo_1",
	"lfo_2",
	"lfo_3",
	"lfo_4",
	NULL
};

/* velocity/lfo modsource names */
char *velo_lfo_names[] = {
	"velocity",
	"lfo_1",
	"lfo_2",
	"lfo_3",
	"lfo_4",
	NULL
};

/* Polarity types */
char *polarity_names[] = {
	"bipolar",
	"unipolar",
	NULL
};

/* Sign names */
char *sign_names[] = {
	"negative",
	"positive",
	NULL
};

/* Boolean values */
char *boolean_names[] = {
	"off",
	"on",
	NULL
};

/* MIDI channel names */
char *midi_ch_names[] = {
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13",
	"14",
	"15",
	"16",
	"omni",
	NULL
};


/*****************************************************************************
 * all value label lists needed for GUI
 *****************************************************************************/
const char *midi_ch_labels[] = {
	"1   ",
	"2   ",
	"3   ",
	"4   ",
	"5   ",
	"6   ",
	"7   ",
	"8   ",
	"9   ",
	"10  ",
	"11  ",
	"12  ",
	"13  ",
	"14  ",
	"15  ",
	"16  ",
	"Omni",
	NULL
};

const char *on_off_labels[] = {
	"Off",
	"On",
	NULL
};

const char *freq_base_labels[] = {
	"MIDI Key  ",
	"Input 1   ",
	"Input 2   ",
	"Input 1&2 ",
	"Amp Env   ",
	"Filter Env",
	"Velocity  ",
	"Tempo     ",
	"Tempo+Trig",
	NULL
};

const char *mod_type_labels[] = {
	"<small>Off</small>",
	"<small>Mix</small>",
	"<small>AM</small>",
	"<small>Mod</small>",
	NULL
};

const char *keymode_labels[] = {
	"MonoSmooth",
	"MonoReTrig",
	"MonoMulti ",
	"Poly      ",
	NULL
};

const char *keyfollow_labels[] = {
	"Off       ",
	"Newest    ",
	"Highest   ",
	"Lowest    ",
	"KeyTrig   ",
	NULL
};

const char *polarity_labels[] = {
	"[-1,1]",
	"[0,1]",
	NULL
};

const char *sign_labels[] = {
	"[-]",
	"[+]",
	NULL
};

const char *wave_labels[] = {
	"Sine      ",
	"Triangle  ",
	"Saw       ",
	"RevSaw    ",
	"Square    ",
	"Stair     ",
	"BL Saw    ",
	"BL RevSaw ",
	"BL Square ",
	"BL Stair  ",
	"Juno Osc  ",
	"Juno Saw  ",
	"Juno Sqr  ",
	"Juno Poly ",
	"Analog Sqr",
	"Vox 1     ",
	"Vox 2     ",
	"Poly Sine ",
	"Poly Saw  ",
	"Poly RvSaw",
	"Poly Sqr 1",
	"Poly Sqr 2",
	"Poly 1    ",
	"Poly 2    ",
	"Poly 3    ",
	"Poly 4    ",
	"Null      ",
	"Identity  ",
	NULL
};

const char *filter_mode_labels[] = {
	"LowPass   ",
	"HighPass  ",
	"BandPass  ",
	"BandStop  ",
	"LP+BP     ",
	"HP+BP     ",
	"LP+HP     ",
	"BS+BP     ",
	NULL
};

const char *filter_type_labels[] = {
	"Dist      ",
	"Retro     ",
	"Moog Dist ",
	"Moog Clean",
	"3Pole Dist",
	"3Pole Raw ",
	NULL
};

const char *lfo_labels[] = {
	"<small>Off</small>",
	"<small>  1</small>",
	"<small>  2</small>",
	"<small>  3</small>",
	"<small>  4</small>",
	NULL
};

const char *velo_lfo_labels[] = {
	"<small>Velo</small>",
	"<small>  1</small>",
	"<small>  2</small>",
	"<small>  3</small>",
	"<small>  4</small>",
	NULL
};

const char *mod_labels[] = {
	"Off       ",
	"Osc-1     ",
	"Osc-2     ",
	"Osc-3     ",
	"Osc-4     ",
	"LFO-1     ",
	"LFO-2     ",
	"LFO-3     ",
	"LFO-4     ",
	"Velocity  ",
	NULL
};

const char *fm_mod_labels[] = {
	"Off       ",
	"Osc-1     ",
	"Osc-2     ",
	"Osc-3     ",
	"Osc-4     ",
	"Osc1-Latch",
	"Osc2-Latch",
	"Osc3-Latch",
	"Osc4-Latch",
	"LFO-1     ",
	"LFO-2     ",
	"LFO-3     ",
	"LFO-4     ",
	"Velocity  ",
	NULL
};

const char *rate_labels[] = {
	"1/128",
	" 1/64",
	" 1/32",
	" 3/64",
	" 1/16",
	" 5/64",
	" 3/32",
	" 7/64",
	" 1/8 ",
	" 9/64",
	" 5/32",
	"11/64",
	" 3/16",
	"13/64",
	" 7/32",
	"15/64",
	" 1/4 ",
	"17/64",
	" 9/32",
	"19/64",
	" 5/16",
	"21/64",
	"11/32",
	"23/64",
	" 3/8 ",
	"25/64",
	"13/32",
	"27/64",
	" 7/16",
	"29/64",
	"15/32",
	"31/64",
	" 1/2 ",
	"33/64",
	"17/32",
	"35/64",
	" 9/16",
	"37/64",
	"19/32",
	"39/64",
	" 5/8 ",
	"41/64",
	"21/32",
	"43/64",
	"11/16",
	"45/64",
	"23/32",
	"47/64",
	" 3/4 ",
	"49/64",
	"25/32",
	"51/64",
	"13/16",
	"53/64",
	"27/32",
	"55/64",
	" 7/8 ",
	"57/64",
	"29/32",
	"59/64",
	"15/16",
	"61/64",
	"31/32",
	"63/64",
	" 1/1 ",
	" 1/48",
	" 1/24",
	" 1/16",
	" 1/12",
	" 5/48",
	" 1/8 ",
	" 7/48",
	" 1/6 ",
	" 3/16",
	" 5/24",
	"11/48",
	" 1/4 ",
	"13/48",
	" 7/24",
	" 5/16",
	" 1/3 ",
	"17/48",
	" 3/8 ",
	"19/48",
	" 5/12",
	" 7/16",
	"11/24",
	"23/48",
	" 1/2 ",
	"25/48",
	"13/24",
	" 9/16",
	" 7/12",
	"29/48",
	" 5/8 ",
	"31/48",
	" 2/3 ",
	"11/16",
	"17/24",
	"35/48",
	" 3/4 ",
	"37/48",
	"19/24",
	"13/16",
	" 5/6 ",
	"41/48",
	" 7/8 ",
	"43/48",
	"11/12",
	"15/16",
	"23/24",
	"47/48",
	" 1/1 ",
	" 2/1 ",
	" 3/1 ",
	" 4/1 ",
	" 5/1 ",
	" 6/1 ",
	" 7/1 ",
	" 8/1 ",
	" 9/1 ",
	"10/1 ",
	"11/1 ",
	"12/1 ",
	"13/1 ",
	"14/1 ",
	"15/1 ",
	"16/1 ",
	NULL
};
