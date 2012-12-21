/*****************************************************************************
 *
 * engine.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2012 William Weston <whw@linuxmail.org>
 * Copyright (C) 2010 Anton Kormakov <assault64@gmail.com>
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
#ifndef _PHASEX_ENGINE_H_
#define _PHASEX_ENGINE_H_

#include <glib.h>
#include "phasex.h"
#include "wave.h"
#include "mididefs.h"
#include "jack.h"


/* Basis by which oscillator frequency and phase triggering is set */
#define FREQ_BASE_MIDI_KEY          0
#define FREQ_BASE_INPUT_1           1
#define FREQ_BASE_INPUT_2           2
#define FREQ_BASE_INPUT_STEREO      3
#define FREQ_BASE_AMP_ENVELOPE      4
#define FREQ_BASE_FILTER_ENVELOPE   5
#define FREQ_BASE_VELOCITY          6
#define FREQ_BASE_TEMPO             7
#define FREQ_BASE_TEMPO_KEYTRIG     8

/* Modulation types available on a per-oscillator basis */
#define MOD_TYPE_OFF                0
#define MOD_TYPE_MIX                1
#define MOD_TYPE_AM                 2
#define MOD_TYPE_NONE               3

/* Modes defining note to oscillator mapping and envelope handling */
#define KEYMODE_MONO_SMOOTH         0
#define KEYMODE_MONO_RETRIGGER      1
#define KEYMODE_MONO_MULTIKEY       2
#define KEYMODE_POLY                3

/* Filter keyfollow modes */
#define KEYFOLLOW_NONE              0
#define KEYFOLLOW_LAST              1
#define KEYFOLLOW_HIGH              2
#define KEYFOLLOW_LOW               3
#define KEYFOLLOW_MIDI              4

/* [-1,1] or [0,1] scaling on waveform */
#define POLARITY_BIPOLAR            0
#define POLARITY_UNIPOLAR           1

/* Signs */
#define SIGN_NEGATIVE               0
#define SIGN_POSITIVE               1

/* envelope intervals */
#define ENV_INTERVAL_ATTACK         0   /* standard attack */
#define ENV_INTERVAL_DECAY          1   /* standard decay */
#define ENV_INTERVAL_SUSTAIN        2   /* sustain with hold */
#define ENV_INTERVAL_RELEASE        3   /* standard release */
#define ENV_INTERVAL_FADE           4   /* fade-out for env filter */
#define ENV_INTERVAL_DONE           5   /* envelope has finished */


/* internal global parameters used by synth engine */
typedef struct global {
	sample_t    bpm;                        /* beats per minute */
	sample_t    bps;                        /* beats per second */
	sample_t    out1;                       /* output sample 2 */
	sample_t    out2;                       /* output sample 1 */
#ifdef ENABLE_DC_REJECTION_FILTER
	sample_t    dcR_const;
#endif
} GLOBAL;


/* internal per voice parameters used by synth engine */
typedef struct voice {
	short       active;                     /* active flag, for poly mode */
	short       allocated;                  /* only allocated voices become active */
	short       midi_key;                   /* midi note in play */
	short       keypressed;                 /* midi key currently held */
	short       need_portamento;            /* does we need portamento handling? */
	short       vol_key;                    /* midi note to use for volume keyfollow */
	short       osc_wave;                   /* internal value for osc wave num */
	short       cur_amp_interval;           /* current interval within the envelope */
	int         id;
	int         cur_amp_sample;             /* sample number within current envelope peice */
	int         portamento_sample;          /* sample number within portamento */
	int         portamento_samples;         /* portamento time in samples */
	int         age;                        /* voice age, in samples */
	sample_t    out1;                       /* output sample 1 */
	sample_t    out2;                       /* output sample 2 */
	sample_t    amp_env;                    /* smoothed final output of env generator */
	sample_t    amp_env_raw;                /* raw envelope value (before smoothing) */
	sample_t    amp_env_log;                /* log-scaled amp envelope for applying gain */
	int         amp_env_dur[6];             /* envelope duration in samples */
	sample_t    amp_env_delta[6];           /* envelope value increment */
	sample_t    amp_env_gain_step[6];       /* logarithmic gain increment */
	sample_t    osc_out1[NUM_OSCS + 1];     /* output waveform value 1 */
	sample_t    osc_out2[NUM_OSCS + 1];     /* output waveform value 2 */
	sample_t    osc_freq[NUM_OSCS];         /* oscillator wave frequency used by engine */
	sample_t    osc_portamento[NUM_OSCS];   /* sample-wise freq adjust amt for portamento */
	sample_t    osc_phase_adjust[NUM_OSCS]; /* phase adjustment to wavetable index */
	sample_t    index[NUM_OSCS];            /* unconverted index into waveform lookup table */
	sample_t    last_index[NUM_OSCS];       /* last output waveform (mono) */
	short       latch[NUM_OSCS];            /* flag for latching init phase of other oscs */
	short       osc_key[NUM_OSCS];          /* current midi note for each osc */
	sample_t    filter_key_adj;             /* index adjustment to use for filter keyfollow */
	sample_t    filter_env;                 /* smoothed final filter envelope output */
	sample_t    filter_env_raw;             /* raw envelope value (before smoothing) */
	sample_t    filter_env_delta[6];        /* envelope value increment */
	int         filter_env_dur[6];          /* envelope duration in samples */
	int         cur_filter_sample;          /* sample number within current envelope peice */
	short       cur_filter_interval;        /* current interval within the envelope */
	short       velocity;                   /* per-voice velocity raw value */
	short       _padding1;
	short       _padding2;
	sample_t    velocity_coef_linear;       /* per-voice linear velocity coefficient */
	sample_t    velocity_target_linear;     /* target for velocity_coef_linear smoothing */
	sample_t    velocity_coef_log;          /* per-voice logarithmic velocity coefficient */
	sample_t    velocity_target_log;        /* target for velocity_coef_log smoothing */

	sample_t    filter_lp1;                 /* filter lowpass output 1 */
	sample_t    filter_lp2;                 /* filter lowpass output 2 */
	sample_t    filter_hp1;                 /* filter highpass output 1 */
	sample_t    filter_hp2;                 /* filter highpass output 2 */
	sample_t    filter_bp1;                 /* filter bandpass output 1 */
	sample_t    filter_bp2;                 /* filter bandpass output 2 */
	sample_t    filter_x_1;
	sample_t    filter_x_2;
	sample_t    filter_y1_1;
	sample_t    filter_y1_2;
	sample_t    filter_y2_1;
	sample_t    filter_y2_2;
	sample_t    filter_y3_1;
	sample_t    filter_y3_2;
	sample_t    filter_y4_1;
	sample_t    filter_y4_2;
	sample_t    filter_oldx_1;
	sample_t    filter_oldx_2;
	sample_t    filter_oldy1_1;
	sample_t    filter_oldy1_2;
	sample_t    filter_oldy2_1;
	sample_t    filter_oldy2_2;
	sample_t    filter_oldy3_1;
	sample_t    filter_oldy3_2;
} VOICE;


/* linked list of keys currently held in play */
typedef struct keylist {
	short           midi_key;
	struct keylist  *next;
} KEYLIST;


/* internal per part (per patch) parameters used by synth engine */
typedef struct part {
	KEYLIST     keylist[128];
	KEYLIST     *head;
	KEYLIST     *cur;
	KEYLIST     *prev;
	MIDI_EVENT  event_queue[MIDI_EVENT_POOL_SIZE];
	MIDI_EVENT  bulk_queue[MIDI_EVENT_POOL_SIZE];
	int         portamento_samples;         /* portamento time in samples */
	int         portamento_sample;          /* sample number within portamento */
	int         midi_channel;
	short       hold_pedal;                 /* flag to indicate hold pedal in use */
	short       midi_key;                   /* last midi key pressed */
	short       prev_key;                   /* previous to last midi key pressed */
	short       last_key;                   /* last key put into play */
	short       high_key;                   /* highest oscillator key in play */
	short       low_key;                    /* lowest oscillator key in play */
	short       velocity;                   /* most recent note-on velocity */
	short       _padding1;
	sample_t    velocity_coef;              /* velocity coefficient for calculations */
	sample_t    velocity_target;            /* target for velocity_coef smoothing */
	sample_t    filter_cutoff_target;       /* filter value smoothing algorithm moves to */
	sample_t    filter_smooth_len;          /* number of samples used for smoothing cutoff */
	sample_t    filter_smooth_factor;       /* 1.0 / (filter_smooth_len + 1.0) */
	sample_t    filter_env_offset;          /* offset for filter env adj (for negative env) */
	sample_t    pitch_bend_base;            /* pitch bender, scaled to [-1, 1] */
	sample_t    pitch_bend_target;          /* target for pitch bender smoothing */
	sample_t    denormal_offset;            /* small dc offset to steer clear of NANs */
	sample_t    dcR_in1;
	sample_t    dcR_in2;
	sample_t    dcR_out1;
	sample_t    dcR_out2;
	sample_t    input_env_raw;              /* input envelope raw value */
	sample_t    input_env_attack;           /* input envelope follower attack in samples */
	sample_t    input_env_release;          /* input envelope follower release in samples */
	sample_t    in1;                        /* input sample 1 */
	sample_t    in2;                        /* input sample 2 */
	sample_t    out1;                       /* output sample 1 */
	sample_t    out2;                       /* output sample 2 */
	sample_t    amp_env_max;                /* max of amp env for all active voices */
	sample_t    filter_env_max;             /* max of filter env for all active voices */
	sample_t    env_buffer[16];
	sample_t    osc_init_index[NUM_OSCS];   /* initial phase index for oscillator */
	sample_t    osc_pitch_bend[NUM_OSCS];   /* current per-osc pitchbend amount */
	sample_t    osc_phase_adjust[NUM_OSCS]; /* phase adjustment to wavetable index */
	short       osc_wave[NUM_OSCS];         /* current wave for osc, including wave lfo */
	short       osc_am_mod[NUM_OSCS];       /* osc to use as AM modulator */
	short       osc_freq_mod[NUM_OSCS];     /* osc to use as FM modulator */
	short       osc_phase_mod[NUM_OSCS];    /* osc to use as phase modulator */
	short       lfo_key[NUM_LFOS + 1];      /* current midi note for each lfo */
	short       _padding2[3];
	sample_t    lfo_pitch_bend[NUM_LFOS + 1]; /* current per-LFO pitchbend amount */
	sample_t    lfo_freq[NUM_LFOS + 1];     /* lfo frequency */
	sample_t    lfo_init_index[NUM_LFOS + 1]; /* initial phase index for LFO waveform */
	sample_t    lfo_adjust[NUM_LFOS + 1];   /* num samples to adjust for current lfo */
	sample_t    lfo_portamento[NUM_LFOS + 1]; /* sample-wise freq adjust amt for portamento */
	sample_t    lfo_index[NUM_LFOS + 1];    /* unconverted index into waveform lookup table */
	sample_t    lfo_out[NUM_LFOS + 2];      /* raw sample output for LFOs */
	sample_t    lfo_freq_lfo_mod[NUM_LFOS + 1];
	short       _padding3;
	short       _padding4;
	int         _padding5;
	int         _padding6;
	long long   _padding7;
	long long   _padding8;
	long long   _padding9;
	long long   _padding10;
	volatile     sample_t   output_buffer1[PHASEX_MAX_BUFSIZE];
	volatile     sample_t   output_buffer2[PHASEX_MAX_BUFSIZE];
} PART;


typedef struct delay {
	sample_t    size;                   /* length of delay buffer in samples */
	sample_t    half_size;              /* length of delay buffer in samples */
	int         write_index;            /* buffer write position */
	int         read_index;             /* buffer read position (lfo modulated) */
	int         bufsize;                /* size of delay buffer in samples */
	int         bufsize_mask;           /* binary mask value for delay bufsize */
	int         length;                 /* integer length lf delay buffer in samples */
	char        _padding[36];
	sample_t    buf[(DELAY_MAX) * 2];   /* stereo delay circular buffer */
} DELAY;


typedef struct chorus {
	sample_t    phase_index_a;          /* index into chorus phase lfo */
	sample_t    phase_index_b;          /* index into chorus phase lfo+90 */
	sample_t    phase_index_c;          /* index into chorus phase lfo+180 */
	sample_t    phase_index_d;          /* index into chorus phase lfo+270 */
	sample_t    phase_amount_a;         /* amount to mix from lfo based position */
	sample_t    phase_amount_b;         /* amount to mix from lfo+90 based position */
	sample_t    phase_amount_c;         /* amount to mix from lfo+180 based position */
	sample_t    phase_amount_d;         /* amount to mix from lfo+270 based position */
#ifdef INTERPOLATE_CHORUS
	sample_t    read_index_a;           /* chorus_buffer read position (lfo modulated) */
	sample_t    read_index_b;           /* chorus_buffer read position (offset 90 deg) */
	sample_t    read_index_c;           /* chorus_buffer read position (offset 180 deg) */
	sample_t    read_index_d;           /* chorus_buffer read position (offset 270 deg) */
#else
	int         read_index_a;           /* chorus_buffer read position (lfo modulated) */
	int         read_index_b;           /* chorus_buffer read position (offset 90 deg) */
	int         read_index_c;           /* chorus_buffer read position (offset 180 deg) */
	int         read_index_d;           /* chorus_buffer read position (offset 270 deg) */
#endif
	sample_t    lfo_index_a;            /* index into chorus lfo */
	sample_t    lfo_index_b;            /* index into chorus lfo (offset ~90 deg) */
	sample_t    lfo_index_c;            /* index into chorus lfo (offset 180 deg) */
	sample_t    lfo_index_d;            /* index into chorus lfo (offset ~270 deg) */
	int         write_index;            /* chorus_buffer write position */
	int         delay_index;            /* chorus_buffer feedback read position */
	int         bufsize;                /* size of chorus buffer in samples */
	int         bufsize_mask;           /* binary mask for chorus buffer size */
	int         length;                 /* int   length of chorus buffer in samples */
	sample_t    size;                   /* float length of chorus buffer in samples */
	sample_t    half_size;              /* float half the length of chorus buffer */
	sample_t    lfo_freq;               /* chorus lfo frequency */
	sample_t    lfo_adjust;             /* chorus lfo index sample-by-sample adjustor */
	sample_t    lfo_index;              /* master index into chorus lfo */
	sample_t    phase_freq;             /* chorus phase lfo frequency */
	sample_t    phase_adjust;           /* chorus phase lfo index increment size */
	char        _padding[16];
#ifdef INTERPOLATE_CHORUS
	sample_t    buf_1[(CHORUS_MAX)];    /* left mono chorus circular buffer */
	sample_t    buf_2[(CHORUS_MAX)];    /* right mono chorus circular buffer */
#else
	sample_t    buf[(CHORUS_MAX) * 2];  /* stereo chorus circular buffer */
#endif
} CHORUS;


#ifdef ENABLE_INPUTS
extern sample_t         input_buffer1[PHASEX_MAX_BUFSIZE];
extern sample_t         input_buffer2[PHASEX_MAX_BUFSIZE];
#endif

extern sample_t         output_buffer1[PHASEX_MAX_BUFSIZE];
extern sample_t         output_buffer2[PHASEX_MAX_BUFSIZE];

extern PART             synth_part[MAX_PARTS];
extern VOICE            voice_pool[MAX_PARTS][MAX_VOICES];
extern DELAY            per_part_delay[MAX_PARTS];
extern CHORUS           per_part_chorus[MAX_PARTS];
extern GLOBAL           global;

extern volatile gint    engine_ready[MAX_PARTS];

extern int              sample_rate;
extern sample_t         f_sample_rate;
extern sample_t         nyquist_freq;
extern sample_t         wave_period;
extern sample_t         wave_fp_period;

extern sample_t         aftertouch_smooth_len;
extern sample_t         aftertouch_smooth_factor;
extern sample_t         pitch_bend_smooth_len;
extern sample_t         pitch_bend_smooth_factor;


#define get_part(part_num)              (&(synth_part[part_num]))
#define get_voice(part_num, voice_num)  (&(voice_pool[part_num][voice_num]))
#define get_delay(part_num)             (&(per_part_delay[part_num]))
#define get_chorus(part_num)            (&(per_part_chorus[part_num]))


#include "patch.h"


void init_engine_buffers(void);
void init_engine_internals(void);
void init_engine_parameters(void);
void *engine_thread(void *arg);
void start_engine_threads(void);
void stop_engine(void);
void run_cycle(unsigned int part_num, unsigned int nframes, sample_t *out1, sample_t *out2);

/* these functions are internal to the synth engine */
void run_chorus(CHORUS *this_chorus, PART *part, PATCH_STATE *state);
void run_delay(DELAY *this_delay, PART *part, PATCH_STATE *state);
void run_osc(VOICE *voice, PART *part, PATCH_STATE *state, unsigned int osc);
void run_oscillators(VOICE *voice, PART *part, PATCH_STATE *state);
void run_voice(VOICE *voice, PART *part, PATCH_STATE *state);
void run_voices(PART *part, PATCH_STATE *state, unsigned int part_num);
void run_lfo(PART *part, PATCH_STATE *state, unsigned int lfo, unsigned int UNUSED(part_num));
void run_lfos(PART *part, PATCH_STATE *state, unsigned int part_num);
void run_voice_envelope(PART *part,
                        PATCH_STATE *state,
                        VOICE *voice,
                        unsigned int UNUSED(part_num));
void run_voice_envelopes(PART *part, PATCH_STATE *state, unsigned int part_num);
void run_part(PART *part, PATCH_STATE *state, unsigned int part_num);
void run_parts(void);


#endif /* _PHASEX_ENGINE_H_ */

