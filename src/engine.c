/*****************************************************************************
 *
 * engine.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2015 Willaim Weston <william.h.weston@gmail.com>
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
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <glib.h>
#include "phasex.h"
#include "timekeeping.h"
#include "buffer.h"
#include "wave.h"
#include "filter.h"
#include "engine.h"
#include "patch.h"
#include "param.h"
#include "midi_event.h"
#include "midi_process.h"
#include "jack.h"
#include "settings.h"
#include "driver.h"
#include "debug.h"


#ifdef ENABLE_INPUTS
sample_t        input_buffer1[PHASEX_MAX_BUFSIZE];
sample_t        input_buffer2[PHASEX_MAX_BUFSIZE];
#endif

sample_t        output_buffer1[PHASEX_MAX_BUFSIZE];
sample_t        output_buffer2[PHASEX_MAX_BUFSIZE];

PART            synth_part[MAX_PARTS];
VOICE           voice_pool[MAX_PARTS][MAX_VOICES];
DELAY           per_part_delay[MAX_PARTS];
CHORUS          per_part_chorus[MAX_PARTS];
GLOBAL          global;

pthread_mutex_t engine_ready_mutex[MAX_PARTS];
pthread_cond_t  engine_ready_cond[MAX_PARTS];
int             engine_ready[MAX_PARTS];

int             sample_rate                 = 0;
sample_t        f_sample_rate               = 0.0;
sample_t        nyquist_freq                = 22050.0;
sample_t        wave_period                 = F_WAVEFORM_SIZE / 44100.0;

sample_t        aftertouch_smooth_len;
sample_t        aftertouch_smooth_factor;

sample_t        pitch_bend_smooth_len;
sample_t        pitch_bend_smooth_factor;

int             sample_rate_mode            = SAMPLE_RATE_NORMAL;


/*****************************************************************************
 * init_engine_buffers()
 *****************************************************************************/
void
init_engine_buffers(void)
{
	PART            *part;
	DELAY           *delay;
	CHORUS          *chorus;
	int             part_num;

	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		part   = get_part(part_num);
		delay  = get_delay(part_num);
		chorus = get_chorus(part_num);

#ifdef INTERPOLATE_CHORUS
		memset((void *)(chorus->buf_1), 0, CHORUS_MAX     * sizeof(sample_t));
		memset((void *)(chorus->buf_2), 0, CHORUS_MAX     * sizeof(sample_t));
#else
		memset((void *)(chorus->buf),   0, CHORUS_MAX * 2 * sizeof(sample_t));
#endif
		memset((void *)(delay->buf),    0, DELAY_MAX  * 2 * sizeof(sample_t));

		memset((void *)(part->output_buffer1), 0,
		       PHASEX_MAX_BUFSIZE * sizeof(jack_default_audio_sample_t));
		memset((void *)(part->output_buffer2), 0,
		       PHASEX_MAX_BUFSIZE * sizeof(jack_default_audio_sample_t));
	}

#ifdef ENABLE_INPUTS
	memset((void *)(input_buffer1),  0,
	       PHASEX_MAX_BUFSIZE * sizeof(jack_default_audio_sample_t));
	memset((void *)(input_buffer2),  0,
	       PHASEX_MAX_BUFSIZE * sizeof(jack_default_audio_sample_t));
#endif
}


/*****************************************************************************
 * init_engine_internals()
 *
 * Initializes parameters used by engine.
 * Run once before engine enters its main loop.
 *****************************************************************************/
void
init_engine_internals(void)
{
	PART            *part;
	DELAY           *delay;
	CHORUS          *chorus;
	unsigned int    part_num;
	unsigned int    j;
	static int      once = 1;

	sample_rate_mode = setting_sample_rate_mode;

	/* clear static mem */
	memset(&global,     0, sizeof(GLOBAL));
	memset(&voice_pool, 0, MAX_PARTS * MAX_VOICES * sizeof(VOICE));

	init_engine_buffers();

	/* keep engine re-init from clobbering the parts */
	if (once) {
		memset(&synth_part, 0, MAX_PARTS * sizeof(PART));
		once = 0;
	}

	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		part   = get_part(part_num);
		delay  = get_delay(part_num);
		chorus = get_chorus(part_num);

		pthread_cond_init(&(engine_ready_cond[part_num]), NULL);
		engine_ready[part_num] = 0;

		/* no midi keys in play yet */
		part->head     = NULL;
		part->cur      = NULL;
		part->prev     = NULL;
		part->midi_key = -1;
		part->prev_key = -1;

		/* set buffer sizes and zero buffers */
		delay->bufsize  = DELAY_MAX;
		chorus->bufsize = CHORUS_MAX;

#ifdef ENABLE_INPUTS
		/* initialize input envelope follower */
		part->input_env_raw     = 0.0;
		part->input_env_attack  = MATH_EXP(MATH_LOG(0.01) / (12));
		part->input_env_release = MATH_EXP(MATH_LOG(0.01) / (24000));
#endif

		/* calculate mask for chorus and delay buffer sizes */
		for (j = 1; j < 24; j++) {
			if ((CHORUS_MAX >> j) == 1) {
				chorus->bufsize_mask = ((1 << j) - 1);
				break;
			}
		}
		for (j = 1; j < 24; j++) {
			if ((DELAY_MAX >> j) == 1) {
				delay->bufsize_mask = ((1 << j) - 1);
				break;
			}
		}
	}

	init_midi_processor();

	/* initialize parameter smoothing */
	aftertouch_smooth_len    = f_sample_rate * 0.25;    /* 1/4 second   */
	aftertouch_smooth_factor = 1.0 / (aftertouch_smooth_len + 1.0);
	pitch_bend_smooth_len    = f_sample_rate * 0.03125; /* 1/32 second  */
	pitch_bend_smooth_factor = 1.0 / (pitch_bend_smooth_len + 1.0);

	/* (-3dB @ 20Hz) DC blocking filter */
	/* 1.0 - (M_PI * 2 * freq / (sample_t) f_sample_rate) */
	global.wdcf = 125.7 / (sample_t) f_sample_rate;
}


/*****************************************************************************
 * init_engine_parameters()
 *
 * Initializes parameters used by engine.
 * Run once before engine enters its main loop.
 *****************************************************************************/
void
init_engine_parameters(void)
{
	PART            *part;
	PATCH           *patch;
	PATCH_STATE     *state;
	VOICE           *voice;
	DELAY           *delay;
	CHORUS          *chorus;
	unsigned int    osc;
	unsigned int    lfo;
	unsigned int    part_num;
	unsigned int    voice_num;

	init_buffer_indices(0);

	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		part   = get_part(part_num);
		patch  = get_active_patch(part_num);
		state  = get_active_state(part_num);
		delay  = get_delay(part_num);
		chorus = get_chorus(part_num);

		/* set initial bpm/bps from patch bpm */
		global.bpm = (sample_t)(state->bpm);
		global.bps = (sample_t)(state->bpm) / 60.0;

		/* set initial patch state for this part */
		init_patch_state(patch);

		/* init portamento */
		part->portamento_samples = env_table[state->portamento];

		/* initialize delay */
		delay->size        = state->delay_time * f_sample_rate / global.bps;
		delay->length      = (int)(delay->size);
		delay->write_index = 0;

		/* initialize chorus */
		chorus->lfo_freq     = global.bps * state->chorus_lfo_rate;
		chorus->size         = state->chorus_time;
		chorus->half_size    = state->chorus_time * 0.5;
		chorus->length       = (int)(state->chorus_time);
		chorus->lfo_adjust   = chorus->lfo_freq * wave_period;
		chorus->write_index  = 0;
		chorus->delay_index  = chorus->bufsize - chorus->length - 1;
		chorus->phase_freq   = global.bps * state->chorus_phase_rate;
		chorus->phase_adjust = chorus->phase_freq * wave_period;

		/* set chorus phase lfo indices */
		chorus->phase_index_a = 0.0;

		chorus->phase_index_b = chorus->phase_index_a + (F_WAVEFORM_SIZE * 0.25);
		if (chorus->phase_index_b >= F_WAVEFORM_SIZE) {
			chorus->phase_index_b -= F_WAVEFORM_SIZE;
		}

		chorus->phase_index_c = chorus->phase_index_a + (F_WAVEFORM_SIZE * 0.5);
		if (chorus->phase_index_c >= F_WAVEFORM_SIZE) {
			chorus->phase_index_c -= F_WAVEFORM_SIZE;
		}

		chorus->phase_index_d = chorus->phase_index_a + (F_WAVEFORM_SIZE * 0.75);
		if (chorus->phase_index_d >= F_WAVEFORM_SIZE) {
			chorus->phase_index_d -= F_WAVEFORM_SIZE;
		}

		/* set mix weights for the 90 degree offset chorus lfo positions */
		chorus->phase_amount_a = (1.0 + wave_lookup(WAVE_SINE, (int) chorus->phase_index_a))
			* 0.5 * (mix_table[127 - state->chorus_phase_balance_cc]);

		chorus->phase_amount_b = (1.0 + wave_lookup(WAVE_SINE, (int) chorus->phase_index_b))
			* 0.5 * (mix_table[state->chorus_phase_balance_cc]);

		chorus->phase_amount_c = (1.0 + wave_lookup(WAVE_SINE, (int) chorus->phase_index_c))
			* 0.5 * (mix_table[127 - state->chorus_phase_balance_cc]);

		chorus->phase_amount_d = (1.0 + wave_lookup(WAVE_SINE, (int) chorus->phase_index_d))
			* 0.5 * (mix_table[state->chorus_phase_balance_cc]);

		/* set chorus lfo indices */
		chorus->lfo_index_a = 0.0;

		if ((chorus->lfo_index_b = chorus->lfo_index_a +
		     (F_WAVEFORM_SIZE * 0.25)) >= F_WAVEFORM_SIZE) {
			chorus->lfo_index_b -= F_WAVEFORM_SIZE;
		}

		if ((chorus->lfo_index_c = chorus->lfo_index_a +
		     (F_WAVEFORM_SIZE * 0.5)) >= F_WAVEFORM_SIZE) {
			chorus->lfo_index_c -= F_WAVEFORM_SIZE;
		}

		if ((chorus->lfo_index_d = chorus->lfo_index_a +
		     (F_WAVEFORM_SIZE * 0.75)) >= F_WAVEFORM_SIZE) {
			chorus->lfo_index_d -= F_WAVEFORM_SIZE;
		}

		/* initialize pitch bend attrs */
		part->pitch_bend_target = part->pitch_bend_base = 0.0;

		/* initialize filter attrs */
		state->filter_env_amount   = (sample_t) state->filter_env_amount_cc;
		state->filter_env_amount   *= state->filter_env_sign;
		part->filter_cutoff_target = state->filter_cutoff;
		part->filter_env_offset    = (state->filter_env_sign_cc == 0) ?
			state->filter_env_amount : 0.0;

		/* init velocity */
		part->velocity        = 0;
		part->velocity_coef   = 1.0;
		part->velocity_target = 1.0;

		/* init special controllers */
		part->hold_pedal      = 0;

		/* init envelope tracking values */
		part->amp_env_max     = 0.0;
		part->filter_env_max  = 0.0;

		/* init denormal offset (sign gets flipped every frame) */
		part->denormal_offset = (sample_t)(1e-19);

		/* per-lfo setup (including LFO_OFF/LFO_VELOCITY) */
		for (lfo = 0; lfo <= NUM_LFOS; lfo++) {

			/* calculate initial indices from initial phases */
			part->lfo_init_index[lfo] = state->lfo_init_phase[lfo] * F_WAVEFORM_SIZE;
			part->lfo_index[lfo]      = part->lfo_init_index[lfo];

			/* calculate frequency and corresponding index adjustment */
			part->lfo_freq[lfo]   = global.bps * state->lfo_rate[lfo];
			part->lfo_adjust[lfo] = part->lfo_freq[lfo] * wave_period;

			/* zero out remaining floating point params */
			part->lfo_pitch_bend[lfo]   = 0.0;
			part->lfo_portamento[lfo]   = 0.0;
			part->lfo_out[lfo]          = 0.0;
			part->lfo_freq_lfo_mod[lfo] = 0.0;
		}

		/* per-oscillator setup */
		for (osc = 0; osc < NUM_OSCS; osc++) {
			/* calculate initial indices from initial phases */
			part->osc_init_index[osc] = state->osc_init_phase[osc] * F_WAVEFORM_SIZE;

			/* zero out floating point params */
			part->osc_pitch_bend[osc] = 0.0;
		}

		/* now handle voice specific inits */
		for (voice_num = 0; voice_num < MAX_VOICES; voice_num++) {
			voice = get_voice(part_num, voice_num);
			voice->id = (int) voice_num;

			/* init portamento and velocity */
			voice->portamento_samples     = env_table[state->portamento];
			voice->velocity               = 0;
			voice->velocity_coef_linear   = 1.0;
			voice->velocity_target_linear = 1.0;
			voice->velocity_coef_log      = 1.0;
			voice->velocity_target_log    = 1.0;

			/* generate the size in samples and value
			   deltas for our envelope intervals */
			voice->amp_env_dur[ENV_INTERVAL_ATTACK] =
				env_interval_dur[ENV_INTERVAL_ATTACK][state->amp_attack];

			/* TODO: test with hold pedal. */
			if (state->amp_attack || state->amp_decay) {
				voice->amp_env_delta[ENV_INTERVAL_ATTACK] =
					1.0 / (sample_t) voice->amp_env_dur[ENV_INTERVAL_ATTACK];
			}
			else {
				voice->amp_env_delta[ENV_INTERVAL_ATTACK] =
					state->amp_sustain /
					(sample_t) voice->amp_env_dur[ENV_INTERVAL_ATTACK];
			}

			voice->amp_env_dur[ENV_INTERVAL_DECAY]     =
				env_interval_dur[ENV_INTERVAL_DECAY][state->amp_decay];
			voice->amp_env_delta[ENV_INTERVAL_DECAY]   =
				(state->amp_sustain - 1.0) /
				(sample_t) voice->amp_env_dur[ENV_INTERVAL_DECAY];
			voice->amp_env_dur[ENV_INTERVAL_SUSTAIN]   = 1;
			voice->amp_env_delta[ENV_INTERVAL_SUSTAIN] = 0.0;
			voice->amp_env_dur[ENV_INTERVAL_RELEASE]   =
				env_interval_dur[ENV_INTERVAL_RELEASE][state->amp_release];
			voice->amp_env_delta[ENV_INTERVAL_RELEASE] =
				(0.0 - state->amp_sustain) /
				(sample_t) voice->amp_env_dur[ENV_INTERVAL_RELEASE];
			voice->amp_env_dur[ENV_INTERVAL_FADE]      = -1;
			voice->amp_env_delta[ENV_INTERVAL_FADE]    = 0.0;
			voice->amp_env_dur[ENV_INTERVAL_DONE]      = -1;
			voice->amp_env_delta[ENV_INTERVAL_DONE]    = 0.0;

			/* initialize envelope state (after release,
			   waiting for attack) and amplitude */
			voice->cur_amp_interval = ENV_INTERVAL_DONE;
			voice->cur_amp_sample   = voice->amp_env_dur[ENV_INTERVAL_DONE];
			voice->amp_env          = 0.0;
			voice->amp_env_raw      = 0.0;

			/* generate the size in samples and value
			   deltas for our envelope intervals */
			voice->filter_env_dur[ENV_INTERVAL_ATTACK]    =
				env_interval_dur[ENV_INTERVAL_ATTACK][state->filter_attack];

			/* TODO: test with hold pedal. */
			if (state->filter_attack || state->filter_decay) {
				voice->filter_env_delta[ENV_INTERVAL_ATTACK] =
					1.0 / (sample_t) voice->filter_env_dur[ENV_INTERVAL_ATTACK];
			}
			else {
				voice->filter_env_delta[ENV_INTERVAL_ATTACK] =
					state->filter_sustain /
					(sample_t) voice->filter_env_dur[ENV_INTERVAL_ATTACK];
			}

			voice->filter_env_dur[ENV_INTERVAL_DECAY]     =
				env_interval_dur[ENV_INTERVAL_DECAY][state->filter_decay];
			voice->filter_env_delta[ENV_INTERVAL_DECAY]   =
				(state->filter_sustain - 1.0) /
				(sample_t) voice->filter_env_dur[ENV_INTERVAL_DECAY];
			voice->filter_env_dur[ENV_INTERVAL_SUSTAIN]   = 1;
			voice->filter_env_delta[ENV_INTERVAL_SUSTAIN] = 0.0;
			voice->filter_env_dur[ENV_INTERVAL_RELEASE]   =
				env_interval_dur[ENV_INTERVAL_RELEASE][state->filter_release];
			voice->filter_env_delta[ENV_INTERVAL_RELEASE] =
				(0.0 - state->filter_sustain) /
				(sample_t) voice->filter_env_dur[ENV_INTERVAL_RELEASE];
			voice->filter_env_dur[ENV_INTERVAL_FADE]      = -1;
			voice->filter_env_delta[ENV_INTERVAL_FADE]    = 0.0;
			voice->filter_env_dur[ENV_INTERVAL_DONE]      = -1;
			voice->filter_env_delta[ENV_INTERVAL_DONE]    = 0.0;

			/* Initialize envelope state (after release,
			   waiting for attack) and amplitude. */
			voice->cur_filter_interval = ENV_INTERVAL_DONE;
			voice->cur_filter_sample   = voice->filter_env_dur[ENV_INTERVAL_DONE];
			voice->filter_env          = 0.0;
			voice->filter_env_raw      = 0.0;

			/* filter outputs */
			voice->filter_hp1 = voice->filter_hp2 = 0.0;
			voice->filter_lp1 = voice->filter_lp2 = 0.0;
			voice->filter_bp1 = voice->filter_bp2 = 0.0;

			/* signify that we're not in a portamento slide */
			voice->portamento_sample = -1;

			/* per-oscillator-per-voice setup */
			for (osc = 0; osc < NUM_OSCS; osc++) {

				/* handle tempo based initial frequencies */
				if (state->osc_freq_base[osc] >= FREQ_BASE_TEMPO) {
					voice->osc_freq[osc] = state->osc_rate[osc] * global.bps;
				}

				/* zero out floating point params */
				voice->osc_out1[osc]       = 0.0;
				voice->osc_out2[osc]       = 0.0;
				voice->osc_portamento[osc] = 0.0;
				voice->index[osc]          = 0.0;
				voice->last_index[osc]     = 0.0;
				voice->latch[osc]          = 0;
			}

			/* initialize the MOD_OFF dummy-voice */
			voice->osc_out1[MOD_OFF] = 0.0;
			voice->osc_out2[MOD_OFF] = 0.0;

			voice->out1              = 0.0;
			voice->out2              = 0.0;

			/* set inactive and ageless */
			voice->active     = 0;
			voice->allocated  = 0;
			voice->age        = 0;
			voice->midi_key   = -1;
			voice->keypressed = -1;

			/* init dc filters */
			voice->dcf1 = 0.0;
			voice->dcf2 = 0.0;

			/* initialize moog filters */
			voice->filter_y1_1    = 0.0;
			voice->filter_y1_2    = 0.0;
			voice->filter_y2_1    = 0.0;
			voice->filter_y2_2    = 0.0;
			voice->filter_y3_1    = 0.0;
			voice->filter_y3_2    = 0.0;
			voice->filter_y4_1    = 0.0;
			voice->filter_y4_2    = 0.0;
			voice->filter_oldx_1  = 0.0;
			voice->filter_oldx_2  = 0.0;
			voice->filter_oldy1_1 = 0.0;
			voice->filter_oldy1_2 = 0.0;
			voice->filter_oldy2_1 = 0.0;
			voice->filter_oldy2_2 = 0.0;
			voice->filter_oldy3_1 = 0.0;
			voice->filter_oldy3_2 = 0.0;
		}

		/* mono gets voice 0 */
		if (state->keymode != KEYMODE_POLY) {
			voice = get_voice(part_num, 0);
			voice->active    = 1;
			voice->allocated = 1;
		}
	}
}


/*****************************************************************************
 * resync_engine()
 *
 * Resyncs engine thread with new audio settings.
 *****************************************************************************/
unsigned int
resync_engine(int part_num,
              unsigned char resync_buffer_size,
              unsigned char resync_sample_rate,
              char *debug_color)
{
	PATCH       *patch;

	PHASEX_DEBUG((DEBUG_CLASS_MIDI |
	              DEBUG_CLASS_MIDI_TIMING |
	              DEBUG_CLASS_ENGINE_TIMING),
	             "%s<<<<<SYNC:%d>>>>> " DEBUG_COLOR_DEFAULT,
	             debug_color, part_num);

	if (resync_buffer_size) {
		buffer_period_size = (unsigned int)(g_atomic_int_get(&new_buffer_period_size));
		buffer_size        = buffer_period_size * buffer_periods;
		buffer_latency     = DEFAULT_LATENCY_PERIODS * buffer_period_size;
		buffer_size_mask   = buffer_size - 1;
		buffer_period_mask = buffer_period_size - 1;
		need_index_resync[part_num] = 0;
	}
	if (resync_sample_rate) {
		sample_rate    = g_atomic_int_get(&new_sample_rate);
		f_sample_rate  = (sample_t)(sample_rate);
		nyquist_freq   = (sample_t)(sample_rate / 2);
		wave_period    = (sample_t)(F_WAVEFORM_SIZE / f_sample_rate);
		build_filter_tables();
		build_env_tables();
		init_engine_internals();
		init_engine_parameters();
		patch = get_active_patch(part_num);
		init_patch_state(patch);
	}

	return get_engine_index();
}


/*****************************************************************************
 * engine_thread()
 *
 * Main sound synthesis thread.  (One thread per part.)
 *****************************************************************************/
void *
engine_thread(void *arg)
{
	unsigned int        part_num        = ((unsigned int)((long int) arg % MAX_PARTS));
	PART                *part           = get_part(part_num);
	PATCH_STATE         *state          = get_active_state(part_num);
	MIDI_EVENT          *event;
	struct sched_param  schedparam;
	pthread_t           thread_id;
#ifdef ENABLE_INPUTS
	sample_t            tmp;
#endif
	unsigned int        e_index         = get_engine_index();
	unsigned int        m_index         = e_index;
	int                 cycle_frame     = (int) buffer_period_size;
	sample_t            last_out1       = 0;
	sample_t            last_out2       = 0;
	timecalc_t          delta_nsec;
	struct timespec     now;
	struct timespec     sleep_time      = { 0, 0 };
	int                 engine_sleep_time;

	/* Sleep interval should be in the range of 100us - 2000us, depending on
	   sample rate, buffer size, and number of parts. */
	engine_sleep_time = 1000000 * (int) buffer_period_size /
		MAX_PARTS / (int) sample_rate / 2;

	PHASEX_DEBUG(DEBUG_CLASS_INIT, "Starting Engine Thread for Part %d  (sleep_time=%d)\n",
	             (part_num + 1), engine_sleep_time);

	/* set realtime scheduling and priority */
	thread_id = pthread_self();
	memset(&schedparam, 0, sizeof(struct sched_param));
	schedparam.sched_priority = setting_engine_priority;
	pthread_setschedparam(thread_id, setting_sched_policy, &schedparam);

	/* broadcast the engine ready condition */
	pthread_mutex_lock(&engine_ready_mutex[part_num]);
	engine_ready[part_num] = 1;
	pthread_cond_broadcast(&engine_ready_cond[part_num]);
	pthread_mutex_unlock(&engine_ready_mutex[part_num]);

	/* MAIN LOOP: one time through for each sample */
	while (!engine_stopped && !pending_shutdown) {

		if (cycle_frame >= (int) buffer_period_size) {
			cycle_frame = 0;

			event = &(part->event_queue[e_index]);
			if (event->type == MIDI_EVENT_RESYNC) {
				e_index = resync_engine(part_num,
				                        event->data[4],
				                        event->data[0],
				                        DEBUG_COLOR_PINK);
			}

			/* At period boundry, set patch state in case of program change. */
			state = get_active_state(part_num);

			/* sleep (if necessary) until next midi period has started. */
			delta_nsec = get_time_delta(&now);
			if (delta_nsec >= 0.0) {
				inc_midi_index();
			}
			while (!engine_stopped && !pending_shutdown && (test_engine_index(e_index))) {
				if (need_index_resync[part_num]) {
					e_index = resync_engine(part_num, 1, 0, DEBUG_COLOR_CYAN);
					delta_nsec = get_time_delta(&now);
					if (delta_nsec >= 0.0) {
						inc_midi_index();
					}
					continue;
				}

				/* usually signifies a clock start */
				if (delta_nsec == 0.0) {
					/* if (part_num == 0) */ {
						PHASEX_DEBUG(DEBUG_CLASS_ENGINE_TIMING, "*");
					}
					sleep_time.tv_nsec = (long int)(engine_sleep_time) * (long int)(sample_rate);
				}
				/* woke up too early -- sleep for rest of midi period. */
				else if (delta_nsec < 0.0) {
					/* if (part_num == 0) */ {
						PHASEX_DEBUG(DEBUG_CLASS_ENGINE_TIMING, ",");
					}
					sleep_time.tv_nsec = - (long int) delta_nsec;
				}
				/* normal adaptive sleep. */
				else if (delta_nsec < nsec_per_period) {
					/* if (part_num == 0) */ {
						PHASEX_DEBUG(DEBUG_CLASS_ENGINE_TIMING, "%c", ('a' + part_num));
					}
					sleep_time.tv_nsec = (long int)(nsec_per_period - delta_nsec +
					                                (8.0 * nsec_per_frame));
				}
				/* We're still waiting on next MIDI clock.  Generally not
				   reached once the MIDI clock has stabliized (a few seconds
				   after clock start). */
				else {
					/* if (part_num == 0) */ {
						PHASEX_DEBUG(DEBUG_CLASS_ENGINE_TIMING, ".");
					}
					sleep_time.tv_nsec = (long int)(engine_sleep_time * 1000);
				}
#ifdef HAVE_CLOCK_NANOSLEEP
				clock_nanosleep(CLOCK_MONOTONIC, 0, &sleep_time, NULL);
#else
				usleep(sleep_time.tv_nsec / 1000);
#endif

				delta_nsec = get_time_delta(&now);
				if (delta_nsec >= 0.0) {
					inc_midi_index();
				}
			}
			//if (part_num == 0) {
			PHASEX_DEBUG(DEBUG_CLASS_ENGINE_TIMING,
			             DEBUG_COLOR_RED "[%c:%d] " DEBUG_COLOR_DEFAULT,
			             ('a' + part_num), (e_index / buffer_period_size));
			//}

			/* Check for forced index resync.  This happens when
			   (re)starting audio and midi subsystems. */
			if (need_index_resync[part_num]) {
				e_index = resync_engine(part_num, 1, 0, DEBUG_COLOR_MAGENTA);
			}

			m_index = e_index;
			e_index = (e_index + buffer_period_size) & buffer_size_mask;
		}

#ifdef ENABLE_INPUTS
		/* Handle input envelope follower (boost is handled
		   while copying from ringbuffer). */
		tmp = (sample_t)(MATH_ABS(part->in1) + MATH_ABS(part->in2));
		if (tmp > 2.0) {
			tmp = 1.0;
		}
		else {
			tmp *= 0.5;
		}
		if (tmp > part->input_env_raw) {
			part->input_env_raw = part->input_env_attack  * (part->input_env_raw - tmp) +
				tmp - part->denormal_offset;
		}
		else {
			part->input_env_raw = part->input_env_release * (part->input_env_raw - tmp) +
				tmp - part->denormal_offset;
		}
#endif

		/* get any new midi events for this part */
		process_midi_events(m_index, (unsigned int)cycle_frame, part_num);

		/* generate sample for this part */
		run_part(part, state, part_num);

		/* For oversampling, generate another sample and use
		   linear interpolation. */
		if (sample_rate_mode == SAMPLE_RATE_OVERSAMPLE) {
			last_out1 = part->out1;
			last_out2 = part->out2;

			run_part(part, state, part_num);

			part->out1  += last_out1;
			part->out1  *= 0.5;

			part->out2  += last_out2;
			part->out2  *= 0.5;
		}

		/* undersample needs to check to second frame's
		   position in the midi event queue */
		else if (sample_rate_mode == SAMPLE_RATE_UNDERSAMPLE) {
			process_midi_events(m_index, (unsigned int)(cycle_frame + 1), part_num);
		}

		/* flip sign of denormal offset */
		part->denormal_offset *= -1.0;

		/* set thread cancellation point out outside critical section */
		pthread_testcancel();

#ifdef ENABLE_INPUTS
		/* get current input sample from buffer */
		part->in1 = (sample_t) input_buffer1[e_index + cycle_frame] * state->input_boost;
		part->in2 = (sample_t) input_buffer2[e_index + cycle_frame] * state->input_boost;
#endif

		/* for undersampling, use linear interpolation on
		   input and output */
		if (sample_rate_mode == SAMPLE_RATE_UNDERSAMPLE) {
#ifdef ENABLE_INPUTS
			part->in1 += ((sample_t) input_buffer1[e_index + cycle_frame] * state->input_boost);
			part->in1 *= 0.5;

			part->in2 += ((sample_t) input_buffer2[e_index + cycle_frame] * state->input_boost);
			part->in2 *= 0.5;
#endif
			part->output_buffer1[e_index + cycle_frame] = (sample_t)((part->out1 + last_out1) * 0.5);
			part->output_buffer2[e_index + cycle_frame] = (sample_t)((part->out2 + last_out2) * 0.5);

			//e_index = (e_index + 1) & buffer_size_mask;
			cycle_frame++;

			last_out1 = part->out1;
			last_out2 = part->out2;
		}

		/* output this sample to the buffer */
		part->output_buffer1[e_index + cycle_frame] = part->out1;
		part->output_buffer2[e_index + cycle_frame] = part->out2;

		/* update buffer position */
		//e_index = (e_index + 1) & buffer_size_mask;
		cycle_frame++;
	}

	/* end of engine thread */
	pthread_exit(NULL);
	return NULL;
}


/*****************************************************************************
 * start_engine_threads()
 *****************************************************************************/
void
start_engine_threads(void)
{
	unsigned int    part_num;
	int             ret;

	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		g_atomic_int_set(& (engine_ready[part_num]), 0);
		if ((ret = pthread_create(&engine_thread_p[part_num], NULL, &engine_thread,
		                          (void *)((long unsigned int) part_num))) != 0) {
			phasex_shutdown("Unable to start engine thread.\n");
		}
	}
	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		while (g_atomic_int_get(&engine_ready[part_num]) != 1) {
			usleep(100000);
		}
	}
	set_engine_priority(NULL, NULL);
	usleep(150000);
}


/*****************************************************************************
 * stop_engine()
 *****************************************************************************/
void
stop_engine(void)
{
	engine_stopped = 1;
}


/*****************************************************************************
 * run_part()
 *
 * Generate a single sample for one part.
 *****************************************************************************/
void
run_part(PART *part, PATCH_STATE *state, unsigned int part_num)
{
	unsigned int    osc;

	/* generate amplitude envelopes for all voices */
	run_voice_envelopes(part, state, part_num);

	/* pitch bender smoothing */
	part->pitch_bend_base = ((pitch_bend_smooth_len * part->pitch_bend_base) +
	                         part->pitch_bend_target) * pitch_bend_smooth_factor;

	/* generate output from lfos */
	run_lfos(part, state, part_num);

	/* parts get mixed at end of voice loop, so init now */
	part->out1 = part->out2 = 0.0;

	/* update number of samples left in portamento */
	if (part->portamento_sample > 0) {
		part->portamento_sample--;
	}

	/* per-part-per-osc calculations */
	for (osc = 0; osc < NUM_OSCS; osc++) {

		/* handle wave selector lfo */
		part->osc_wave[osc] = (short)(state->osc_wave[osc] +
		                              (int)(part->lfo_out[state->wave_lfo[osc]] *
		                                    state->wave_lfo_amount[osc]) +
		                              (NUM_WAVEFORMS << 4)) % NUM_WAVEFORMS;
	}

	/* filter cutoff and channel aftertouch smoothing */
	state->filter_cutoff = ((part->filter_smooth_len * state->filter_cutoff) +
	                        part->filter_cutoff_target) * part->filter_smooth_factor;
	part->velocity_coef  = ((aftertouch_smooth_len * part->velocity_coef) +
	                        part->velocity_target) * aftertouch_smooth_factor;

	/* generate the voices, including filters */
	run_voices(part, state, part_num);

	/* apply input follower envelope, if needed */
#ifdef ENABLE_INPUTS
	if (state->input_follow) {
		part->out1 *= part->input_env_raw;
		part->out2 *= part->input_env_raw;
	}
#endif

	/* now apply patch volume and panning */
	part->out1 *= state->volume * pan_table[127 - state->pan_cc];
	part->out2 *= state->volume * pan_table[state->pan_cc];


	/* effects are last in the chain. */
	if (state->chorus_mix_cc) {
		run_chorus(get_chorus(part_num), part, state);
	}
	if (state->delay_mix_cc) {
		run_delay(get_delay(part_num), part, state);
	}
}


/*****************************************************************************
 * run_voice_envelopes()
 *
 * Generate all voice envelopes for the current sample.
 *****************************************************************************/
void
run_voice_envelopes(PART *part, PATCH_STATE *state, unsigned int part_num)
{
	VOICE           *voice;
	unsigned int    voice_num;

	/* reset envelope tracking variables */
	part->amp_env_max    = 0.0;
	part->filter_env_max = 0.0;

	/* generate envelopes for all voices on this part */
	for (voice_num = 0; voice_num < (unsigned int) setting_polyphony; voice_num++) {
		voice = get_voice(part_num, voice_num);

		/* skip over inactive voices */
		if ((voice->allocated == 0)) {
			continue;
		}

		run_voice_envelope(part, state, voice, part_num);
	}
}


/*****************************************************************************
 * run_voice_envelope()
 *
 * Generate this sample's voice envelope for current part.
 *****************************************************************************/
void
run_voice_envelope(PART         *part,
                   PATCH_STATE  *state,
                   VOICE        *voice,
                   unsigned int UNUSED(part_num))
{
	unsigned int        osc;

	/* mark voice as active, since we know it's allocated */
	voice->active = 1;

	/* has the end of an envelope interval been reached? */
	if (voice->cur_amp_sample < 0) {
		if ((voice->cur_amp_interval != ENV_INTERVAL_SUSTAIN) ||
		    ((voice->keypressed == -1) && !part->hold_pedal)) {
			/* switch on interval just finishing */
			switch (voice->cur_amp_interval) {
			case ENV_INTERVAL_ATTACK:
				/* move on to decay */
				voice->cur_amp_interval++;
				if (!part->hold_pedal) {
					voice->amp_env_raw = 1.0;
				}
				voice->amp_env_dur[ENV_INTERVAL_DECAY]   =
					env_interval_dur[ENV_INTERVAL_DECAY][state->amp_decay];
				voice->amp_env_delta[ENV_INTERVAL_DECAY] =
					/* TODO: test with hold pedal.                       */
					/* no hold pedal: (state->amp_sustain - 1.0) /       */
					/* (sample_t)voice->amp_env_dur[ENV_INTERVAL_DECAY]; */
					(state->amp_sustain - voice->amp_env_raw) /
					(sample_t) voice->amp_env_dur[ENV_INTERVAL_DECAY];
				voice->amp_env_raw += voice->amp_env_delta[ENV_INTERVAL_DECAY];
				break;
			case ENV_INTERVAL_DECAY:
				/* move on to sustain */
				voice->cur_amp_interval++;
				break;
			case ENV_INTERVAL_SUSTAIN:
				/* move on to release */
				voice->cur_amp_interval++;
				voice->amp_env_dur[ENV_INTERVAL_RELEASE]   =
					env_interval_dur[ENV_INTERVAL_RELEASE][state->amp_release];
				voice->amp_env_delta[ENV_INTERVAL_RELEASE] =
					/* TODO: test with hold pedal.                         */
					/* no hold pedal: (0.0 - state->amp_sustain) /         */
					/* (sample_t)voice->amp_env_dur[ENV_INTERVAL_RELEASE]; */
					(0.0 - voice->amp_env_raw) /
					(sample_t) voice->amp_env_dur[ENV_INTERVAL_RELEASE];
				voice->amp_env_raw += voice->amp_env_delta[ENV_INTERVAL_RELEASE];
				break;
			case ENV_INTERVAL_RELEASE:
				/* move on to fade */
				voice->cur_amp_interval++;
				voice->amp_env_dur[ENV_INTERVAL_FADE]   =
					env_interval_dur[ENV_INTERVAL_RELEASE][11];
				voice->amp_env_delta[ENV_INTERVAL_FADE] =
					(0.0 - voice->amp_env_raw) /
					(sample_t) voice->amp_env_dur[ENV_INTERVAL_FADE];
				voice->amp_env_raw += voice->amp_env_delta[ENV_INTERVAL_FADE];
				break;
			case ENV_INTERVAL_FADE:
				voice->amp_env_raw *= 0.95;
				/* wait for envelope to fade below audible range */
				if (voice->amp_env_raw > MINIMUM_GAIN) {
					break;
				}
				/* envelope can now finish */
				voice->amp_env_raw = 0.0;
				voice->cur_amp_interval = ENV_INTERVAL_DONE;
				/* intentional fall-through */
			case ENV_INTERVAL_DONE:
				voice->active    = 0;
				voice->allocated = 0;
				voice->age       = 0;
				voice->midi_key  = -1;
				for (osc = 0; osc < NUM_OSCS; osc++) {
					voice->osc_out1[osc] = 0.0;
					voice->osc_out2[osc] = 0.0;
				}
				voice->out1 = 0.0;
				voice->out2 = 0.0;
				voice->amp_env_raw  = 0.0;
				break;
			}
			voice->cur_amp_sample = voice->amp_env_dur[voice->cur_amp_interval];
		}
	}

	/* still inside the amp envelope interval */
	else {
		/* decrement our sample number for this envelope interval */
		voice->cur_amp_sample--;

		/* add the per-sample delta to the envelope */
		if ((voice->amp_env_raw += voice->amp_env_delta[voice->cur_amp_interval]) < MINIMUM_GAIN) {
			voice->amp_env_raw = 0.0;
			voice->cur_amp_sample = -1;
		}
		else if (voice->amp_env_raw > 1.0) {
			voice->amp_env_raw = 1.0;
			voice->cur_amp_sample = -1;
		}
	}

	/* do almost the same thing for filter envelope. */
	if (voice->cur_filter_sample < 0) {
		if ((voice->cur_filter_interval != ENV_INTERVAL_SUSTAIN) ||
		    ((voice->keypressed == -1) && !part->hold_pedal)) {
			/* switch on interval just finishing */
			switch (voice->cur_filter_interval) {
			case ENV_INTERVAL_ATTACK:
				/* move on to decay */
				voice->cur_filter_interval++;
				if (!part->hold_pedal) {
					voice->filter_env_raw = 1.0;
				}
				voice->filter_env_dur[ENV_INTERVAL_DECAY]   =
					env_interval_dur[ENV_INTERVAL_DECAY][state->filter_decay];
				voice->filter_env_delta[ENV_INTERVAL_DECAY] =
					/* TODO: test with hold pedal.                          */
					/* no hold pedal:   (state->filter_sustain - 1.0) /     */
					/* (sample_t)voice->filter_env_dur[ENV_INTERVAL_DECAY]; */
					(state->filter_sustain - voice->filter_env_raw) /
					(sample_t) voice->filter_env_dur[ENV_INTERVAL_DECAY];
				voice->filter_env_raw += voice->filter_env_delta[ENV_INTERVAL_DECAY];
				break;
			case ENV_INTERVAL_DECAY:
				/* move on to sustain */
				voice->cur_filter_interval++;
				break;
			case ENV_INTERVAL_SUSTAIN:
				/* move on to release */
				voice->cur_filter_interval++;
				voice->filter_env_dur[ENV_INTERVAL_RELEASE]   =
					env_interval_dur[ENV_INTERVAL_RELEASE]
					[state->filter_release];
				voice->filter_env_delta[ENV_INTERVAL_RELEASE] =
					/* TODO: test with hold pedal.                            */
					/* no hold pedal: (0.0 - state->filter_sustain) /         */
					/* (sample_t)voice->filter_env_dur[ENV_INTERVAL_RELEASE]; */
					(0.0 - voice->filter_env_raw) /
					(sample_t) voice->filter_env_dur[ENV_INTERVAL_RELEASE];
				voice->filter_env_raw += voice->filter_env_delta[ENV_INTERVAL_RELEASE];
				break;
			case ENV_INTERVAL_RELEASE:
				/* move on to fade */
				voice->cur_filter_interval++;
				voice->filter_env_dur[ENV_INTERVAL_FADE]   =
					env_interval_dur[ENV_INTERVAL_RELEASE][11];
				voice->filter_env_delta[ENV_INTERVAL_FADE] =
					(0.0 - voice->filter_env_raw) /
					(sample_t) voice->filter_env_dur[ENV_INTERVAL_FADE];
				voice->filter_env_raw += voice->filter_env_delta[ENV_INTERVAL_FADE];
				break;
				/* intentional fall-through */
			case ENV_INTERVAL_FADE:
				voice->filter_env_raw *= 0.97;
				/* wait for envelope to fade.  amp env should finish first. */
				if (voice->filter_env_raw > 0.0) {
					break;
				}
				/* envelope can now finish */
				voice->filter_env_raw = 0.0;
				voice->cur_filter_interval = ENV_INTERVAL_DONE;
				/* intentional fall-through */
			case ENV_INTERVAL_DONE:
				/* for all modes, set envelope to zero when done. */
				voice->filter_env_raw  = 0.0;
				break;
			}
			voice->cur_filter_sample = voice->filter_env_dur[voice->cur_filter_interval];
		}
	}

	/* still inside the filter envelope interval */
	else {
		/* decrement our sample number for this envelope interval */
		voice->cur_filter_sample--;

		/* add the per-sample delta to the envelope */
		if ((voice->filter_env_raw += voice->filter_env_delta[voice->cur_filter_interval]) < 0.0) {
			voice->filter_env_raw = 0.0;
			voice->cur_filter_sample = -1;
		}
		else if (voice->filter_env_raw > 1.0) {
			voice->filter_env_raw = 1.0;
			voice->cur_filter_sample = -1;
		}
	}

	/* find max of per-voice envelopes in case per-part lfo needs it */
	if (voice->amp_env_raw > part->amp_env_max) {
		part->amp_env_max    = voice->amp_env_raw;
	}
	if (voice->filter_env_raw > part->filter_env_max) {
		part->filter_env_max = voice->filter_env_raw;
	}
}


/*****************************************************************************
 * run_lfos()
 *
 * Generate all LFOs for current sample.
 *****************************************************************************/
void
run_lfos(PART *part, PATCH_STATE *state, unsigned int part_num)
{
	unsigned int        lfo;

	/* standard calculations for all LFOs */
	for (lfo = 0; lfo < NUM_LFOS; lfo++) {
		run_lfo(part, state, lfo, part_num);
	}
}


/*****************************************************************************
 * run_lfo()
 *
 * Generate specific LFO for current sample.
 *****************************************************************************/
void run_lfo(PART         *part,
             PATCH_STATE  *state,
             unsigned int lfo,
             unsigned int UNUSED(part_num))
{

	/* current pitch bend for this lfo */
	part->lfo_pitch_bend[lfo] = part->pitch_bend_base * state->lfo_pitchbend[lfo];

	/* grab the generic waveform data */
	switch (state->lfo_freq_base[lfo]) {

	case FREQ_BASE_MIDI_KEY:
		/* handle portamento if necessary */
		if (part->portamento_sample > 0) {
			part->lfo_freq[lfo] += part->lfo_portamento[lfo];
		}
		/* otherwise set frequency directly */
		else {
			part->lfo_freq[lfo] = freq_table[state->patch_tune_cc][256 + part->lfo_key[lfo]];
		}

		/* intentional fall-through */
	case FREQ_BASE_TEMPO_KEYTRIG:
	case FREQ_BASE_TEMPO:
		/* calculate lfo index adjustment based on current freq and
		   pitch bend */
		part->lfo_adjust[lfo] = part->lfo_freq[lfo] *
			halfsteps_to_freq_mult(state->lfo_transpose[lfo] +
			                       part->lfo_pitch_bend[lfo] +
			                       (part->lfo_freq_lfo_mod[lfo] *
			                        part->lfo_out[1])) * wave_period;

		/* grab LFO output from osc table */
#ifdef INTERPOLATE_WAVETABLE_LOOKUPS
		part->lfo_out[lfo]   = osc_table_hermite(state->lfo_wave[lfo], part->lfo_index[lfo]);
#else
		part->lfo_out[lfo]   = wave_lookup(state->lfo_wave[lfo], (int) part->lfo_index[lfo]);
#endif
		part->lfo_index[lfo] += part->lfo_adjust[lfo];
		while (part->lfo_index[lfo] < 0.0) {
			part->lfo_index[lfo] += F_WAVEFORM_SIZE;
		}
		while (part->lfo_index[lfo] >= F_WAVEFORM_SIZE) {
			part->lfo_index[lfo] -= F_WAVEFORM_SIZE;
		}
		break;

	case FREQ_BASE_AMP_ENVELOPE:
		part->lfo_out[lfo] =
			(2.0 * env_curve[(int)(part->amp_env_max * F_ENV_CURVE_SIZE)]) - 1.0;
		break;

	case FREQ_BASE_FILTER_ENVELOPE:
		part->lfo_out[lfo] =
			(2.0 * env_curve[(int)(part->filter_env_max * F_ENV_CURVE_SIZE)]) - 1.0;
		break;

	case FREQ_BASE_INPUT_1:
		part->lfo_out[lfo] = part->in1;
		break;

	case FREQ_BASE_INPUT_2:
		part->lfo_out[lfo] = part->in2;
		break;

	case FREQ_BASE_INPUT_STEREO:
		part->lfo_out[lfo] = (part->in1 + part->in2) * 0.5;
		break;

	case FREQ_BASE_VELOCITY:
		part->lfo_out[lfo] = part->velocity_coef;
		break;
	}

	/* resacle for unipolar lfo, if necessary */
	if (state->lfo_polarity_cc[lfo] == POLARITY_UNIPOLAR) {
		part->lfo_out[lfo] += 1.0;
		part->lfo_out[lfo] *= 0.5;
	}
}


/*****************************************************************************
 * run_voices()
 *
 * Generate all voices for current part / current sample.
 *****************************************************************************/
void
run_voices(PART *part, PATCH_STATE *state, unsigned int part_num)
{
	VOICE           *voice;
	unsigned int    voice_num;

	/* cycle through voices in play */
	for (voice_num = 0; voice_num < (unsigned int) setting_polyphony; voice_num++) {
		voice = get_voice(part_num, voice_num);

		/* skip over inactive voices */
		if (voice->active == 0) {
			continue;
		}
		run_voice(voice, part, state);
	}
}


/*****************************************************************************
 * run_voice()
 *
 * Generate a single voice for current part / current sample.
 *****************************************************************************/
void
run_voice(VOICE *voice, PART *part, PATCH_STATE *state)
{
	sample_t    tmp;

	/* set voice output to zero.  oscs will be mixed in */
	voice->out1 = voice->out2 = 0.0;

	/* velocity smoothing (needed for smooth aftertouch) */
	voice->velocity_coef_linear = ((aftertouch_smooth_len * voice->velocity_coef_linear) +
	                               voice->velocity_target_linear) * aftertouch_smooth_factor;
	voice->velocity_coef_log    = ((aftertouch_smooth_len * voice->velocity_coef_log) +
	                               voice->velocity_target_log) * aftertouch_smooth_factor;

	/* the real heavy lifting / osc modulations happens here */
	run_oscillators(voice, part, state);

	/* filters are run per voice! */
	switch (state->filter_type) {
	case FILTER_TYPE_DIST:
	case FILTER_TYPE_RETRO:
		run_filter(voice, part, state);
		break;
	case FILTER_TYPE_MOOG_DIST:
	case FILTER_TYPE_MOOG_CLEAN:
		run_moog_filter(voice, part, state);
		break;
	case FILTER_TYPE_EXPERIMENTAL_DIST:
	case FILTER_TYPE_EXPERIMENTAL_CLEAN:
		run_experimental_filter(voice, part, state);
		break;
	}

	/* Apply dedicated LFO AM for this voice */
	tmp = (1.0 + state->lfo_1_voice_am * (part->lfo_out[0] - 1.0));

	/* Apply the amp velocity and amp envelope for this voice */
	tmp *= voice->velocity_coef_log * env_curve[(int)(voice->amp_env_raw * F_ENV_CURVE_SIZE)];
	voice->out1 *= tmp;
	voice->out2 *= tmp;

	/* end of per voice parameters.  mix voices */
	part->out1 += ((voice->out1 * state->stereo_width) +
	               (voice->out2 * (1.0 - state->stereo_width)));
	part->out2 += ((voice->out2 * state->stereo_width) +
	               (voice->out1 * (1.0 - state->stereo_width)));

	/* keep track of voice's age for note stealing */
	voice->age++;
}


/*****************************************************************************
 * run_oscillators()
 *
 * Generate all oscillators for current part / current sample.
 *****************************************************************************/
void
run_oscillators(VOICE *voice, PART *part, PATCH_STATE *state)
{
	unsigned int        osc;

	/* cycle through all of the oscillators */
	for (osc = 0; osc < NUM_OSCS; osc++) {

		/* skip over inactive oscillators */
		if (state->osc_modulation[osc] == MOD_TYPE_OFF) {
			continue;
		}

		/* the real work is done here */
		run_osc(voice, part, state, osc);
	}

	/* apply DC filters */
	voice->dcf1 += global.wdcf * (voice->out1 - voice->dcf1);
	voice->out1 -= voice->dcf1;
	voice->dcf2 += global.wdcf * (voice->out2 - voice->dcf2);
	voice->out2 -= voice->dcf2;

	/* oscs are mixed.  now apply AM oscs. */
	for (osc = 0; osc < NUM_OSCS; osc++) {
		if (state->osc_modulation[osc] == MOD_TYPE_AM) {
			voice->out1 *= voice->osc_out1[osc];
			voice->out2 *= voice->osc_out2[osc];
		}
	}

}


/*****************************************************************************
 * run_osc()
 *
 * Generate a single oscillator for current part / current sample.
 *****************************************************************************/
void
run_osc(VOICE *voice, PART *part, PATCH_STATE *state, unsigned int osc)
{
	sample_t        freq_adjust;
	sample_t        phase_adjust1;
	sample_t        phase_adjust2;
	sample_t        tmp_1;
	sample_t        tmp_2;
	int             j;

	/* current pitch bend for this osc */
	part->osc_pitch_bend[osc] = part->pitch_bend_base * state->osc_pitchbend[osc];

	/* frequency modulations */
	switch (state->osc_freq_base[osc]) {

	case FREQ_BASE_MIDI_KEY:
		/* handle portamento if necessary */
		if (voice->portamento_sample > 0) {
			voice->osc_freq[osc] += voice->osc_portamento[osc];
			voice->portamento_sample--;
		}
		/* otherwise set frequency directly */
		else {
			voice->osc_freq[osc] = freq_table
				[state->patch_tune_cc]
				[256 + voice->osc_key[osc] + state->transpose +
				 state->osc_transpose_cc[osc] - 64];
		}
		/* intentional fall-through */
	case FREQ_BASE_TEMPO:
	case FREQ_BASE_TEMPO_KEYTRIG:
		/* get frequency modulator */
		switch (state->freq_mod_type[osc]) {
		case MOD_TYPE_LFO:
			tmp_1 = part->lfo_out[state->freq_lfo[osc]];
			break;
		case MOD_TYPE_OSC_LATCH:
			/* latch the oscillator's phase to the phase of
			   the modulator. */
			if (voice->latch[part->osc_freq_mod[osc]]) {
				voice->index[osc] = part->osc_init_index[osc];
			}
			/* intentional fall-through */
		case MOD_TYPE_OSC:
			/* Get the mono-downmixed output of the modulator.
			   This preserves some of the harmonic content added by
			   phase modulations. */
			j = part->osc_freq_mod[osc];
			tmp_1 = (voice->osc_out1[j] + voice->osc_out2[j]) * 0.5;
			/* saturation / soft clipping */
			tmp_2 = (sample_t) MATH_ABS(tmp_1);
			tmp_1 *= (tmp_2 + 1.1) / ((tmp_2 * tmp_2) + (1.1 - 1.0) * tmp_2 + 1.0);
			break;
		case MOD_TYPE_VELOCITY:
			tmp_1 = voice->velocity_coef_linear;
			break;
		default:
			tmp_1 = 0.0;
			break;
		}

		/* Calculate current note frequency with current freq lfo,
		   pitch bender, etc, and come up with adjustment to current
		   index. */
		freq_adjust = halfsteps_to_freq_mult((tmp_1
		                                      * state->freq_lfo_amount[osc])
		                                     + part->osc_pitch_bend[osc]
		                                     + state->osc_transpose[osc])
			* voice->osc_freq[osc] * wave_period;

		/* shift the wavetable index by amounts determined above */
		voice->index[osc] += freq_adjust;
		voice->latch[osc] = 0;
		while (voice->index[osc] < 0.0) {
			voice->index[osc] += F_WAVEFORM_SIZE;
			voice->latch[osc] = 1;
		}
		while (voice->index[osc] >= F_WAVEFORM_SIZE) {
			voice->index[osc] -= F_WAVEFORM_SIZE;
			voice->latch[osc] = 1;
		}

		/* mark oscillator as latchable when phase passes init index */
		if (state->osc_init_phase_cc[osc] > 0) {
			voice->latch[osc] = 0;
			if ((voice->index[osc] >= part->osc_init_index[osc]) &&
			    ((voice->last_index[osc] < part->osc_init_index[osc]) ||
			     (voice->last_index[osc] > voice->index[osc]))) {
				voice->latch[osc] = 1;
			}
		}
		voice->last_index[osc] = voice->index[osc];

		/* calculate current phase shift */

		/* get data from phase modulation source */
		switch (state->phase_mod_type[osc]) {
		case MOD_TYPE_LFO:
			tmp_1 = tmp_2 = part->lfo_out[state->phase_lfo[osc]];
			break;
		case MOD_TYPE_OSC_LATCH:
			if (voice->latch[part->osc_phase_mod[osc]]) {
				voice->index[osc] = part->osc_init_index[osc];
			}
			/* intentional fall-through */
		case MOD_TYPE_OSC:
			/* swap channels here to reduce DC offset */
			tmp_2 = voice->osc_out1[part->osc_phase_mod[osc]];
			tmp_1 = voice->osc_out2[part->osc_phase_mod[osc]];
			break;
		case MOD_TYPE_VELOCITY:
			tmp_1 = tmp_2 = voice->velocity_coef_linear;
			break;
		default:
			tmp_1 = tmp_2 = 0.0;
			break;
		}

		/* calculate phase adjustment */
		phase_adjust1 = tmp_1 * state->phase_lfo_amount[osc] * F_WAVEFORM_SIZE;
		phase_adjust2 = tmp_2 * state->phase_lfo_amount[osc] * F_WAVEFORM_SIZE;

		/* grab osc output from osc table, applying phase adjustments
		   to right and left */
#ifdef INTERPOLATE_WAVETABLE_LOOKUPS
		voice->osc_out1[osc] = osc_table_hermite(part->osc_wave[osc],
		                                          (voice->index[osc] - phase_adjust1));
		voice->osc_out2[osc] = osc_table_hermite(part->osc_wave[osc],
		                                          (voice->index[osc] + phase_adjust2));
#else
		voice->osc_out1[osc] =
			(wave_lookup(part->osc_wave[osc], 
				   (((int)(voice->index[osc] - phase_adjust1)
	                                   + WAVEFORM_SIZE) % WAVEFORM_SIZE)));

		voice->osc_out2[osc] =
			(wave_lookup(part->osc_wave[osc], 
				   (((int)(voice->index[osc] + phase_adjust2)
	                                   + WAVEFORM_SIZE) % WAVEFORM_SIZE)));
#endif
		break;

	case FREQ_BASE_INPUT_1:
		voice->osc_out1[osc] = part->in1;
		voice->osc_out2[osc] = part->in1;
		break;

	case FREQ_BASE_INPUT_2:
		voice->osc_out1[osc] = part->in2;
		voice->osc_out2[osc] = part->in2;
		break;

	case FREQ_BASE_INPUT_STEREO:
		voice->osc_out1[osc] = part->in1;
		voice->osc_out2[osc] = part->in2;
		break;

	case FREQ_BASE_AMP_ENVELOPE:
		tmp_1 = (2.0 * env_curve[(int)(voice->amp_env_raw * F_ENV_CURVE_SIZE)]) - 1.0;
		voice->osc_out1[osc] = tmp_1;
		voice->osc_out2[osc] = tmp_1;
		break;

	case FREQ_BASE_FILTER_ENVELOPE:
		tmp_1 = (2.0 * env_curve[(int)(voice->filter_env_raw * F_ENV_CURVE_SIZE)]) - 1.0;
		voice->osc_out1[osc] = tmp_1;
		voice->osc_out2[osc] = tmp_1;
		break;

	case FREQ_BASE_VELOCITY:
		voice->osc_out1[osc] = voice->velocity_coef_linear;
		voice->osc_out2[osc] = voice->velocity_coef_linear;
		break;
	}

	/* rescale if osc is unipolar */
	if (state->osc_polarity_cc[osc] == POLARITY_UNIPOLAR) {
		voice->osc_out1[osc] += 1.0;
		voice->osc_out1[osc] *= 0.5;
		voice->osc_out2[osc] += 1.0;
		voice->osc_out2[osc] *= 0.5;
	}

	/* last modulation to apply is AM */
	switch (state->am_mod_type[osc]) {
	case MOD_TYPE_OSC_LATCH:
		if (voice->latch[part->osc_am_mod[osc]]) {
			voice->index[osc] = part->osc_init_index[osc];
		}
		/* intentional fall-through */
	case MOD_TYPE_OSC:
		if (state->am_lfo_amount[osc] > 0.0) {
			voice->osc_out1[osc] *= ((voice->osc_out1[part->osc_am_mod[osc]] *
			                          state->am_lfo_amount[osc]) + 1.0) * 0.5;
			voice->osc_out2[osc] *= ((voice->osc_out2[part->osc_am_mod[osc]] *
			                          state->am_lfo_amount[osc]) + 1.0) * 0.5;
		}
		else if (state->am_lfo_amount[osc] < 0.0) {
			voice->osc_out1[osc] *= ((voice->osc_out1[part->osc_am_mod[osc]] *
			                          state->am_lfo_amount[osc]) - 1.0) * 0.5;
			voice->osc_out2[osc] *= ((voice->osc_out2[part->osc_am_mod[osc]] *
			                          state->am_lfo_amount[osc]) - 1.0) * 0.5;
		}
		break;
	case MOD_TYPE_LFO:
		if (state->am_lfo_amount[osc] > 0.0) {
			tmp_1 = ((part->lfo_out[state->am_lfo[osc]] * state->am_lfo_amount[osc]) + 1.0) * 0.5;
			voice->osc_out1[osc] *= tmp_1;
			voice->osc_out2[osc] *= tmp_1;
		}
		else if (state->am_lfo_amount[osc] < 0.0) {
			tmp_1 = ((part->lfo_out[state->am_lfo[osc]] * state->am_lfo_amount[osc]) - 1.0) * 0.5;
			voice->osc_out1[osc] *= tmp_1;
			voice->osc_out2[osc] *= tmp_1;
		}
		break;
	case MOD_TYPE_VELOCITY:
		tmp_1 = 1.0 - ((1.0 - voice->velocity_coef_linear) * state->am_lfo_amount[osc]);
		voice->osc_out1[osc] *= tmp_1;
		voice->osc_out2[osc] *= tmp_1;
		break;
	}

	/* add oscillator to voice mix, if necessary */
	if (state->osc_modulation[osc] == MOD_TYPE_MIX) {
		voice->out1 += voice->osc_out1[osc];
		voice->out2 += voice->osc_out2[osc];
	}
}


/*****************************************************************************
 * run_delay()
 *
 * Apply delay effect to current part / current sample.
 *****************************************************************************/
void
run_delay(DELAY *delay, PART *part, PATCH_STATE *state)
{
	sample_t    tmp_1, tmp_2, tmp_3, tmp_4;

	/* set read position into delay buffer */
	if (state->delay_lfo == LFO_OFF) {
		delay->read_index =
			(delay->bufsize + delay->write_index -
			 delay->length - 1) & delay->bufsize_mask;
	}

	/* set read position into delay buffer based on delay lfo */
	else {
		delay->read_index =
			(delay->bufsize + delay->write_index -
			 (int)(((part->lfo_out[state->delay_lfo] + 1.0) *
			        delay->size * 0.5)) - 1) & delay->bufsize_mask;
	}

	/* read delayed signal from delay buffer */
	tmp_1 = delay->buf[2 * delay->read_index];
	tmp_2 = delay->buf[2 * delay->read_index + 1];

	/* keep original input signal around for buffer writing */
	tmp_3 = part->out1;
	tmp_4 = part->out2;

	/* mix delayed signal with input */
	part->out1 = (part->out1 * (mix_table[127 - state->delay_mix_cc])) +
		(tmp_1 * mix_table[state->delay_mix_cc]);
	part->out2 = (part->out2 * (mix_table[127 - state->delay_mix_cc])) +
		(tmp_2 * mix_table[state->delay_mix_cc]);

	/* write input to delay buffer with feedback */
	delay->buf[2 * delay->write_index + state->delay_crossover] =
		(tmp_1 * (mix_table[state->delay_feed_cc])) +
		(tmp_3 * (mix_table[127 - state->delay_feed_cc])) - part->denormal_offset;
	delay->buf[2 * delay->write_index + (1 - state->delay_crossover)] =
		(tmp_2 * (mix_table[state->delay_feed_cc])) +
		(tmp_4 * (mix_table[127 - state->delay_feed_cc])) - part->denormal_offset;

	/* increment delay write index */
	delay->write_index++;
	delay->write_index &= delay->bufsize_mask;
}


/*****************************************************************************
 * run_chorus()
 *
 * Apply chorus effect to current part / current sample.
 *****************************************************************************/
void
run_chorus(CHORUS *chorus, PART *part, PATCH_STATE *state)
{
	sample_t        tmp_1,   tmp_2,   tmp_3,   tmp_4;
	sample_t        tmp_1_a, tmp_1_b, tmp_1_c, tmp_1_d;
	sample_t        tmp_2_a, tmp_2_b, tmp_2_c, tmp_2_d;

#ifdef INTERPOLATE_CHORUS
	/* with interpolation, chorus buffer must be two separate mono buffers */

	/* set phase offset read indices into chorus delay buffer */
	chorus->read_index_a =
		((sample_t)(chorus->bufsize + chorus->write_index - chorus->length - 1) +
		 ((wave_lookup(state->chorus_lfo_wave, (int) chorus->lfo_index_a) + 1.0) *
		  chorus->half_size * state->chorus_amount));

	chorus->read_index_b =
		((sample_t)(chorus->bufsize + chorus->write_index - chorus->length - 1) +
		 ((wave_lookup(state->chorus_lfo_wave, (int) chorus->lfo_index_b) + 1.0) *
		  chorus->half_size * state->chorus_amount));

	chorus->read_index_c =
		((sample_t)(chorus->bufsize + chorus->write_index - chorus->length - 1) +
		 ((wave_lookup(state->chorus_lfo_wave, (int) chorus->lfo_index_c) + 1.0) *
		  chorus->half_size * state->chorus_amount));

	chorus->read_index_d =
		((sample_t)(chorus->bufsize + chorus->write_index - chorus->length - 1) +
		 ((wave_lookup(state->chorus_lfo_wave, (int) chorus->lfo_index_d) + 1.0) *
		  chorus->half_size * state->chorus_amount));

	/* grab values from phase offset positions within chorus delay buffer */
	tmp_1_a = chorus_hermite(chorus->buf_1, chorus->read_index_a);
	tmp_2_a = chorus_hermite(chorus->buf_2, chorus->read_index_a);

	tmp_1_b = chorus_hermite(chorus->buf_1, chorus->read_index_b);
	tmp_2_b = chorus_hermite(chorus->buf_2, chorus->read_index_b);

	tmp_1_c = chorus_hermite(chorus->buf_1, chorus->read_index_c);
	tmp_2_c = chorus_hermite(chorus->buf_2, chorus->read_index_c);

	tmp_1_d = chorus_hermite(chorus->buf_1, chorus->read_index_d);
	tmp_2_d = chorus_hermite(chorus->buf_2, chorus->read_index_d);
#else
	/* chorus_buf MUST be a single stereo width buffer, not separate buffers!
	   Set phase offset read indices into chorus delay buffer */
	chorus->read_index_a =
		(chorus->bufsize + chorus->write_index +
		 (int)(((wave_lookup(state->chorus_lfo_wave, (int)(chorus->lfo_index_a)) + 1.0) *
		        chorus->half_size * state->chorus_amount)) -
		 chorus->length - 1) & chorus->bufsize_mask;

	chorus->read_index_b =
		(chorus->bufsize + chorus->write_index +
		 (int)(((wave_lookup(state->chorus_lfo_wave, (int)(chorus->lfo_index_b)) + 1.0) *
		        chorus->half_size * state->chorus_amount)) -
		 chorus->length - 1) & chorus->bufsize_mask;

	chorus->read_index_c =
		(chorus->bufsize + chorus->write_index +
		 (int)(((wave_lookup(state->chorus_lfo_wave, (int)(chorus->lfo_index_c)) * 1.0) *
		        chorus->half_size * state->chorus_amount)) -
		 chorus->length - 1) & chorus->bufsize_mask;

	chorus->read_index_d =
		(chorus->bufsize + chorus->write_index +
		 (int)(((wave_lookup(state->chorus_lfo_wave, (int)(chorus->lfo_index_d)) * 1.0) *
		        chorus->half_size * state->chorus_amount)) -
		 chorus->length - 1) & chorus->bufsize_mask;

	/* grab values from phase offset positions within chorus delay buffer */
	tmp_1_a = chorus->buf[2 * chorus->read_index_a];
	tmp_2_a = chorus->buf[2 * chorus->read_index_a + 1];

	tmp_1_b = chorus->buf[2 * chorus->read_index_b];
	tmp_2_b = chorus->buf[2 * chorus->read_index_b + 1];

	tmp_1_c = chorus->buf[2 * chorus->read_index_c];
	tmp_2_c = chorus->buf[2 * chorus->read_index_c + 1];

	tmp_1_d = chorus->buf[2 * chorus->read_index_d];
	tmp_2_d = chorus->buf[2 * chorus->read_index_d + 1];
#endif

	/* add them together, with channel crossing */
	tmp_1 = ((tmp_1_a * chorus->phase_amount_a) + (tmp_2_b * chorus->phase_amount_b) +
	         (tmp_1_c * chorus->phase_amount_c) + (tmp_2_d * chorus->phase_amount_d));
	tmp_2 = ((tmp_2_a * chorus->phase_amount_a) + (tmp_1_b * chorus->phase_amount_b) +
	         (tmp_2_c * chorus->phase_amount_c) + (tmp_1_d * chorus->phase_amount_d));

	/* keep dry signal around for chorus delay buffer mixing */
	tmp_3 = part->out1;
	tmp_4 = part->out2;

	/* combine dry/wet for final output */
	part->out1 = (tmp_3 * (mix_table[127 - state->chorus_mix_cc])) +
		(tmp_1 * mix_table[state->chorus_mix_cc]);
	part->out2 = (tmp_4 * (mix_table[127 - state->chorus_mix_cc])) +
		(tmp_2 * mix_table[state->chorus_mix_cc]);

#ifdef INTERPOLATE_CHORUS
	/* write to chorus delay buffer with feedback */
	tmp_1 = ((chorus->buf_1[chorus->delay_index] * mix_table[state->chorus_feed_cc])
	         + (tmp_3 * mix_table[127 - state->chorus_feed_cc])) - part->denormal_offset;

	tmp_2 = ((chorus->buf_2[chorus->delay_index] * mix_table[state->chorus_feed_cc])
	         + (tmp_4 * mix_table[127 - state->chorus_feed_cc])) - part->denormal_offset;

	if (state->chorus_crossover) {
		chorus->buf_1[chorus->write_index] = tmp_2;
		chorus->buf_2[chorus->write_index] = tmp_1;
	}
	else {
		chorus->buf_1[chorus->write_index] = tmp_1;
		chorus->buf_2[chorus->write_index] = tmp_2;
	}
#else
	/* write to chorus delay buffer with feedback */
	chorus->buf[2 * chorus->write_index + state->chorus_crossover] =
		((chorus->buf[2 * chorus->delay_index]     * mix_table[state->chorus_feed_cc])
		 + (tmp_3 * mix_table[127 - state->chorus_feed_cc])) - part->denormal_offset;

	chorus->buf[2 * chorus->write_index + (1 - state->chorus_crossover)] =
		((chorus->buf[2 * chorus->delay_index + 1] * mix_table[state->chorus_feed_cc])
		 + (tmp_4 * mix_table[127 - state->chorus_feed_cc])) - part->denormal_offset;
#endif

	/* set phase lfo indices */
	chorus->phase_index_a += chorus->phase_adjust;
	if (chorus->phase_index_a >= F_WAVEFORM_SIZE) {
		chorus->phase_index_a -= F_WAVEFORM_SIZE;
	}

	chorus->phase_index_b = chorus->phase_index_a + (F_WAVEFORM_SIZE * 0.25);
	if (chorus->phase_index_b >= F_WAVEFORM_SIZE) {
		chorus->phase_index_b -= F_WAVEFORM_SIZE;
	}

	chorus->phase_index_c = chorus->phase_index_a + (F_WAVEFORM_SIZE * 0.5);
	if (chorus->phase_index_c >= F_WAVEFORM_SIZE) {
		chorus->phase_index_c -= F_WAVEFORM_SIZE;
	}

	chorus->phase_index_d = chorus->phase_index_a + (F_WAVEFORM_SIZE * 0.75);
	if (chorus->phase_index_d >= F_WAVEFORM_SIZE) {
		chorus->phase_index_d -= F_WAVEFORM_SIZE;
	}

	/* set amount used for mix weight for the LFO positions at right angles */
	chorus->phase_amount_a = (1.0 + wave_lookup(WAVE_SINE, (int)(chorus->phase_index_a)))
		* 0.5 * (mix_table[127 - state->chorus_phase_balance_cc]);

	chorus->phase_amount_c = 1.0 - chorus->phase_amount_a;

	chorus->phase_amount_b = (1.0 + wave_lookup(WAVE_SINE, (int)(chorus->phase_index_b)))
		* 0.5 * (mix_table[state->chorus_phase_balance_cc]);

	chorus->phase_amount_d = 1.0 - chorus->phase_amount_b;

	/* set lfo indices */
	chorus->lfo_index_a += chorus->lfo_adjust;
	if (chorus->lfo_index_a >= F_WAVEFORM_SIZE) {
		chorus->lfo_index_a -= F_WAVEFORM_SIZE;
	}

	chorus->lfo_index_b = chorus->lfo_index_a + (F_WAVEFORM_SIZE * 0.25);
	if (chorus->lfo_index_b >= F_WAVEFORM_SIZE) {
		chorus->lfo_index_b -= F_WAVEFORM_SIZE;
	}

	chorus->lfo_index_c = chorus->lfo_index_a + (F_WAVEFORM_SIZE * 0.5);
	if (chorus->lfo_index_c >= F_WAVEFORM_SIZE) {
		chorus->lfo_index_c -= F_WAVEFORM_SIZE;
	}

	chorus->lfo_index_d = chorus->lfo_index_a + (F_WAVEFORM_SIZE * 0.75);
	if (chorus->lfo_index_d >= F_WAVEFORM_SIZE) {
		chorus->lfo_index_d -= F_WAVEFORM_SIZE;
	}

	/* increment chorus write index */
	chorus->write_index++;
	chorus->write_index &= chorus->bufsize_mask;

	/* increment delayed position into chorus buffer */
	chorus->delay_index++;
	chorus->delay_index &= chorus->bufsize_mask;
}
