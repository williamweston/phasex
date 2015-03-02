/*****************************************************************************
 *
 * param_parse.c
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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "phasex.h"
#include "settings.h"
#include "config.h"
#include "param.h"
#include "param_parse.h"
#include "param_strings.h"
#include "patch.h"
#include "engine.h"
#include "wave.h"
#include "debug.h"


/*****************************************************************************
 * get_rate_val()
 *
 * Given an input MIDI cltr value,
 * returns a time based rate to be used by the engine.
 *****************************************************************************/
sample_t
get_rate_val(int ctlr)
{
	if (ctlr <= 0) {
		return 32.0;
	}
	else if (ctlr <= 64) {
		return (1.0 / (((sample_t)(ctlr)) * 4.0 / 64.0));
	}
	else if (ctlr <= 111) {
		return (1.0 / (((sample_t)(ctlr - 64)) * 4.0 / 48.0));
	}
	else if (ctlr <= 127) {
		return (1.0 / (((sample_t)(ctlr - 111)) * 4.0));
	}

	return 0.25;
}


/*****************************************************************************
 * get_rate_ctlr()
 *
 * Given a rate value, checks it in the supported rates list and
 * returns the corresponding MIDI cc value.
 *****************************************************************************/
int
get_rate_ctlr(char *token, char *UNUSED(filename), int UNUSED(line))
{
	int     j;

	for (j = 0; j < 128; j++) {
		if (strncmp(token, rate_names[j], 5) == 0) {
			return j;
		}
	}

	/* the gui callback can ignore bad values */
	if (filename == NULL) {
		return PARAM_VAL_IGNORE;
	}

	/* when read from a file, default to 0 on bad values */
	return 0;
}


/*****************************************************************************
 * get_wave()
 *
 * Given an input token (waveform name),
 * returns a wave type enumerator.
 *****************************************************************************/
int
get_wave(char *token, char *filename, int line)
{
	int     wave = 0;

	if (strcmp(token, "sine") == 0) {
		return WAVE_SINE;
	}
	else if (strcmp(token, "triangle") == 0) {
		return WAVE_TRIANGLE;
	}
	else if (strcmp(token, "tri") == 0) {
		return WAVE_TRIANGLE;
	}
	else if (strcmp(token, "saw") == 0) {
		return WAVE_SAW;
	}
	else if (strcmp(token, "revsaw") == 0) {
		return WAVE_REVSAW;
	}
	else if (strcmp(token, "square") == 0) {
		return WAVE_SQUARE;
	}
	else if (strcmp(token, "stair") == 0) {
		return WAVE_STAIR;
	}
	else if (strcmp(token, "saw_s") == 0) {
		return WAVE_SAW_S;
	}
	else if (strcmp(token, "bl_saw") == 0) {
		return WAVE_SAW_S;
	}
	else if (strcmp(token, "revsaw_s") == 0) {
		return WAVE_REVSAW_S;
	}
	else if (strcmp(token, "bl_revsaw") == 0) {
		return WAVE_REVSAW_S;
	}
	else if (strcmp(token, "square_s") == 0) {
		return WAVE_SQUARE_S;
	}
	else if (strcmp(token, "bl_square") == 0) {
		return WAVE_SQUARE_S;
	}
	else if (strcmp(token, "stair_s") == 0) {
		return WAVE_STAIR_S;
	}
	else if (strcmp(token, "bl_stair") == 0) {
		return WAVE_STAIR_S;
	}
	else if (strcmp(token, "identity") == 0) {
		return WAVE_IDENTITY;
	}
	else if (strcmp(token, "null") == 0) {
		return WAVE_NULL;
	}
	else if (strcmp(token, "juno_osc") == 0) {
		return WAVE_JUNO_OSC;
	}
	else if (strcmp(token, "juno_saw") == 0) {
		return WAVE_JUNO_SAW;
	}
	else if (strcmp(token, "juno_square") == 0) {
		return WAVE_JUNO_SQUARE;
	}
	else if (strcmp(token, "juno_poly") == 0) {
		return WAVE_JUNO_POLY;
	}
	else if (strcmp(token, "analog_sine_2") == 0) {
		return WAVE_ANALOG_SINE_2;
	}
	else if (strncmp(token, "analog_sine", 11) == 0) {
		return WAVE_ANALOG_SINE_1;
	}
	else if (strcmp(token, "analog_square") == 0) {
		return WAVE_ANALOG_SQUARE;
	}
	else if (strcmp(token, "vox_1") == 0) {
		return WAVE_VOX_1;
	}
	else if (strcmp(token, "vox_2") == 0) {
		return WAVE_VOX_2;
	}
	else if (strcmp(token, "poly_sine") == 0) {
		return WAVE_POLY_SINE;
	}
	else if (strcmp(token, "poly_saw") == 0) {
		return WAVE_POLY_SAW;
	}
	else if (strcmp(token, "poly_revsaw") == 0) {
		return WAVE_POLY_REVSAW;
	}
	else if (strcmp(token, "poly_square_1") == 0) {
		return WAVE_POLY_SQUARE_1;
	}
	else if (strcmp(token, "poly_square_2") == 0) {
		return WAVE_POLY_SQUARE_2;
	}
	else if (strcmp(token, "poly_1") == 0) {
		return WAVE_POLY_1;
	}
	else if (strcmp(token, "poly_2") == 0) {
		return WAVE_POLY_2;
	}
	else if (strcmp(token, "poly_3") == 0) {
		return WAVE_POLY_3;
	}
	else if (strcmp(token, "poly_4") == 0) {
		return WAVE_POLY_4;
	}
	else if (((wave = atoi(token)) > 0) && (wave < NUM_WAVEFORMS)) {
		return wave;
	}
	else {
		PHASEX_ERROR("unknown wave type '%s' in %s, line %d -- using sine\n",
		             token, filename, line);
	}

	return WAVE_SINE;
}


/*****************************************************************************
 * get_polarity()
 *
 * Given an input token for polarity,
 * returns a polarity enumerator.
 *****************************************************************************/
int
get_polarity(char *token, char *filename, int line)
{
	if (strcmp(token, "unipolar") == 0) {
		return POLARITY_UNIPOLAR;
	}
	else if (strcmp(token, "bipolar") == 0) {
		return POLARITY_BIPOLAR;
	}

	PHASEX_ERROR("unknown polarity '%s' in %s, line %d -- using bipolar\n",
	             token, filename, line);

	return POLARITY_BIPOLAR;
}


/*****************************************************************************
 * get_ctlr()
 *
 * Given an input token, returns a MIDI controller value (0-127).
 *****************************************************************************/
int
get_ctlr(char *token, char *UNUSED(filename), int UNUSED(line))
{
	return atoi(token) % 128;
}


/*****************************************************************************
 * get_boolean()
 *
 * Given an input token, returns a boolean value of 1 or 0.
 *****************************************************************************/
int
get_boolean(char *token, char *UNUSED(filename), int UNUSED(line))
{
	if (strcmp(token, "1") == 0) {
		return 1;
	}
	else if (strcmp(token, "true") == 0) {
		return 1;
	}
	else if (strcmp(token, "yes") == 0) {
		return 1;
	}
	else if (strcmp(token, "on") == 0) {
		return 1;
	}
	return 0;
}


/*****************************************************************************
 * get_freq_base()
 *
 * Given an input token, returns waveform frequency/timing freq_base enumerator.
 *****************************************************************************/
int
get_freq_base(char *token, char *UNUSED(filename), int UNUSED(line))
{
	if (strcmp(token, "midi_key") == 0) {
		return FREQ_BASE_MIDI_KEY;
	}
	else if (strcmp(token, "tempo") == 0) {
		return FREQ_BASE_TEMPO;
	}
	else if (strcmp(token, "keytrig") == 0) {
		return FREQ_BASE_TEMPO_KEYTRIG;
	}
	else if (strcmp(token, "input_1") == 0) {
		return FREQ_BASE_INPUT_1;
	}
	else if (strcmp(token, "input_2") == 0) {
		return FREQ_BASE_INPUT_2;
	}
	else if (strcmp(token, "input_stereo") == 0) {
		return FREQ_BASE_INPUT_STEREO;
	}
	else if (strncmp(token, "amp_env", 7) == 0) {
		return FREQ_BASE_AMP_ENVELOPE;
	}
	else if (strncmp(token, "filter_env", 10) == 0) {
		return FREQ_BASE_FILTER_ENVELOPE;
	}
	else if (strncmp(token, "velocity", 8) == 0) {
		return FREQ_BASE_VELOCITY;
	}
	else {
		return FREQ_BASE_MIDI_KEY;
	}
}
