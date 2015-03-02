/*****************************************************************************
 *
 * patch.h
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
#ifndef _PHASEX_PATCH_H_
#define _PHASEX_PATCH_H_

#include "phasex.h"


#define PATCH_NAME_LEN      24


/* list of patch directories for file chooser shortcuts */
typedef struct dir_list_node {
	char                    *name;
	int                     load_shortcut;
	int                     save_shortcut;
	struct dir_list_node    *next;
} DIR_LIST;


/* Parameter settings from the patch are copied into the patch state
   through patch loads, MIDI changes, or GUI changes.  Some parameters
   have both floating point and integer MIDI controller counterparts
   to help keep conversions in the engine to a minimum. */
typedef struct patch_state {
	/* general parameters */
	sample_t    volume;                     /* patch volume */
	short       volume_cc;
	short       bpm_cc;
	sample_t    bpm;                        /* beats per minute */
	short       patch_tune;                 /* patch tuning, A2FREQ +/- n/120th halfsteps */
	short       patch_tune_cc;
	short       keymode;                    /* ket to osc mode (mono _smooth or _multikey) */
	short       keyfollow_vol;              /* keyfollow volume cc (0=low freq, 127=high) */
	short       portamento;                 /* portamento level */
	short       transpose;                  /* halfsteps transpose, +/- */
	short       transpose_cc;               /* transpose midi cc [0=-64, 64=0, 127=63] */
	short       input_boost_cc;
	sample_t    input_boost;                /* sixteenth boost in input amplitude */
	short       input_follow;               /* apply input envelope follower */
	short       pan_cc;                     /* panning adj (0=right, 64=center, 127=left) */
	sample_t    stereo_width;               /* width of stereo image (0=mono 127=stereo) */
	short       stereo_width_cc;
	short       amp_velocity_cc;
	sample_t    amp_velocity;               /* amp velocity sensitivity */

	/* amplifier envelope parameters */
	short       amp_attack;                 /* amp envelope attack time */
	short       amp_decay;                  /* amp envelope decay time */
	sample_t    amp_sustain;                /* amp envelope sustain level */
	short       amp_sustain_cc;
	short       amp_release;                /* amp envelope release time */

	/* delay parameters */
	short       delay_crossover;            /* flip channels each time through delay ? */
	short       delay_feed_cc;
	sample_t    delay_feed;                 /* delay feedback */
	sample_t    delay_time;                 /* length of delay buffer in bars */
	short       delay_time_cc;
	short       delay_lfo;                  /* lfo to apply to delay */
	short       delay_lfo_cc;
	short       delay_mix_cc;
	sample_t    delay_mix;                  /* delay input to output mixing ratio */

	/* chorus parameters */
	sample_t    chorus_mix;                 /* chorus input to output mixing ratio */
	short       chorus_mix_cc;
	short       chorus_crossover;           /* flip channels each time through chorus ? */
	sample_t    chorus_feed;                /* chorus feedback */
	short       chorus_feed_cc;
	short       chorus_time_cc;
	sample_t    chorus_time;                /* length of chorus buffer in samples */
	sample_t    chorus_amount;              /* amount of chorus delay buffer for LFO to span */
	short       chorus_amount_cc;
	short       chorus_phase_rate_cc;
	sample_t    chorus_phase_rate;          /* chorus phase offset lfo rate */
	sample_t    chorus_phase_balance;       /* bal between 0,180 and 90,270 chorus phases */
	short       chorus_phase_balance_cc;
	short       chorus_lfo_wave;            /* waveform to use for chorus read lfo */
	sample_t    chorus_lfo_rate;            /* chorus lfo rate */
	short       chorus_lfo_rate_cc;

	/* filter parameters */
	short       filter_lfo_rate_cc;
	sample_t    filter_lfo_rate;            /* rate for filter lfo in bars per oscillation */
	sample_t    filter_cutoff;              /* filter cutoff frequency (midi note number) */
	short       filter_cutoff_cc;
	short       filter_resonance_cc;
	sample_t    filter_resonance;           /* filter resonance */
	short       filter_smoothing;           /* cc for cutoff smoothing decayed avg length */
	short       filter_keyfollow;           /* filter cutoff follows note frequency ? */
	short       filter_mode;                /* filter mode (0=lp, 1=hp, 2=bp, 3=bs, etc.) */
	short       filter_type;                /* filter type (dist,retro,etc.) */
	sample_t    filter_gain;                /* filter input gain */
	short       filter_gain_cc;
	short       filter_env_amount_cc;
	sample_t    filter_env_amount;          /* filter envelope coefficient */
	sample_t    filter_env_sign;            /* filter envelope sign (+1 or -1) */
	short       filter_env_sign_cc;
	short       filter_attack;              /* filter envelope attack time */
	short       filter_decay;               /* filter envelope decay time */
	short       filter_sustain_cc;
	sample_t    filter_sustain;             /* filter envelope sustain level */
	short       filter_release;             /* filter envelope release time */
	short       filter_lfo;                 /* lfo to apply to filter */
	short       filter_lfo_cc;
	short       filter_lfo_cutoff_cc;
	sample_t    filter_lfo_cutoff;          /* amount of lfo modulation for filter cutoff */
	sample_t    filter_lfo_resonance;       /* amount of lfo modulation for filter resonance */
	short       filter_lfo_resonance_cc;
	short       _padding1;

	/* oscillator paramaters */
	short       osc_wave[NUM_OSCS];         /* waveform number for each oscillator */
	short       osc_freq_base[NUM_OSCS];    /* frequency basis (0=wave; 1=env; 2=cc) */
	sample_t    osc_rate[NUM_OSCS];         /* base osc freq (before pitch/phase shift) */
	short       osc_rate_cc[NUM_OSCS];
	sample_t    osc_init_phase[NUM_OSCS];   /* initial phase position for oscillator wave */
	short       osc_init_phase_cc[NUM_OSCS];
	short       osc_polarity_cc[NUM_OSCS];  /* scale wave from [-1,1] or [0,1] ? */
	short       osc_channel[NUM_OSCS];      /* midi channel for this oscillator */
	short       osc_modulation[NUM_OSCS];   /* modulation type (0=AM; 1=Mix) */
	sample_t    osc_transpose[NUM_OSCS];    /* num halfsteps to transpose each osc */
	short       osc_transpose_cc[NUM_OSCS];
	sample_t    osc_fine_tune[NUM_OSCS];    /* num 1/120th halfsteps to fine tune osc */
	short       osc_fine_tune_cc[NUM_OSCS];
	sample_t    osc_pitchbend[NUM_OSCS];    /* per-osc +/- amount (halfsteps) to bend */
	short       osc_pitchbend_cc[NUM_OSCS];
	short       am_mod_type[NUM_OSCS];      /* type of modulator for amplitude modulation */
	short       am_lfo[NUM_OSCS];           /* lfo to use for amplitude modulation */
	short       am_lfo_cc[NUM_OSCS];
	sample_t    am_lfo_amount[NUM_OSCS];    /* range of am lfo */
	short       am_lfo_amount_cc[NUM_OSCS];
	short       freq_mod_type[NUM_OSCS];    /* type of modulator for frequency shift */
	short       freq_lfo[NUM_OSCS + 1];     /* lfo to use for frequency shift */
	short       freq_lfo_cc[NUM_OSCS];
	sample_t    freq_lfo_amount[NUM_OSCS];  /* range of freq shift lfo in halfsteps */
	short       freq_lfo_amount_cc[NUM_OSCS];
	sample_t    freq_lfo_fine[NUM_OSCS];    /* range of freq shift lfo in 1/120th halfsteps */
	short       freq_lfo_fine_cc[NUM_OSCS];
	short       phase_mod_type[NUM_OSCS];   /* modulator type for phase shift */
	short       phase_lfo[NUM_OSCS];        /* lfo to use for phase shift */
	short       phase_lfo_cc[NUM_OSCS];
	sample_t    phase_lfo_amount[NUM_OSCS]; /* range of phase shift lfo in degrees */
	short       phase_lfo_amount_cc[NUM_OSCS];
	short       wave_lfo[NUM_OSCS];         /* lfo to use for wave selector lfo */
	short       wave_lfo_cc[NUM_OSCS];
	sample_t    wave_lfo_amount[NUM_OSCS];  /* range of wave selector lfo */
	short       wave_lfo_amount_cc[NUM_OSCS];

	/* lfo parameters */
	short       lfo_wave[NUM_LFOS + 1];      /* waveform to use for lfo */
	short       lfo_freq_base[NUM_LFOS + 1]; /* freq_base for lfo (0=wave; 1=env; 2=cc) */
	sample_t    lfo_init_phase[NUM_LFOS + 1];/* initial phase position for lfo */
	short       lfo_init_phase_cc[NUM_LFOS + 1];
	sample_t    lfo_polarity[NUM_LFOS + 1];  /* lfo polarity (0=bipolar; 1=unipolar) */
	short       lfo_polarity_cc[NUM_LFOS + 1];
	sample_t    lfo_rate[NUM_LFOS + 1];      /* rate of lfo, in (96ths - 1) */
	short       lfo_rate_cc[NUM_LFOS + 1];
	short       lfo_transpose[NUM_LFOS + 1]; /* num halfsteps to transpose each lfo */
	short       lfo_transpose_cc[NUM_LFOS + 1];
	sample_t    lfo_pitchbend[NUM_LFOS + 1]; /* per-lfo +/- amount (halfsteps) to bend */
	short       lfo_pitchbend_cc[NUM_LFOS + 1];

	/* dedicated lfo parameters */
	sample_t    lfo_1_voice_am;
	short       lfo_1_voice_am_cc;
	sample_t    lfo_2_lfo_1_fm;
	short       lfo_2_lfo_1_fm_cc;
	sample_t    lfo_3_cutoff;
	short       lfo_3_cutoff_cc;
	sample_t    lfo_4_lfo_3_fm;
	short       lfo_4_lfo_3_fm_cc;

	/* pad to 64-byte boundary for cache performance. */
	char        padding[14];
} PATCH_STATE;


#include "param.h"


/* forward declaration */
struct part;


typedef struct patch {
	char                *name;
	char                *filename;
	char                *directory;
	PATCH_STATE         *state;     /* engine patch state */
	struct part         *part;      /* engine part */
	unsigned int        sess_num;
	unsigned int        part_num;
	unsigned int        prog_num;
	int                 modified;
	int                 revision;
	struct phasex_param param[NUM_HELP_PARAMS];
} PATCH;


extern PATCH        *active_patch[MAX_PARTS];
extern PATCH_STATE  *active_state[MAX_PARTS];

extern DIR_LIST     *patch_dir_list;

extern int          patch_load_in_progress;

extern int          patch_name_changed;


unsigned int get_visible_program_number(void);
PATCH *get_patch(unsigned int sess_num, unsigned int part_num, unsigned int prog_num);
PATCH *get_visible_patch(void);
struct part *get_visible_part(void);
PATCH_STATE *get_patch_state(unsigned int sess_num, unsigned int part_num, unsigned int prog_num);
PATCH_STATE *get_active_state(unsigned int part_num);
PATCH *get_active_patch(unsigned int part_num);
PATCH *set_active_patch(unsigned int sess_num, unsigned int part_num, unsigned int prog_num);
char *get_param_strval(PARAM *param);
void init_patch_state(PATCH *patch);
void init_patch(PATCH *patch);
void init_patch_data_structures(PATCH *patch,
                                unsigned int sess_num,
                                unsigned int part_num,
                                unsigned int prog_num);
void init_patch_param_data(void);
int read_patch(char *filename, PATCH *patch);
int save_patch(char *filename, PATCH *patch);


#endif /* _PHASEX_PATCH_H_ */
