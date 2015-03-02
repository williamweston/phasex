/*****************************************************************************
 *
 * midi_process.c
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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "phasex.h"
#include "mididefs.h"
#include "midi_event.h"
#include "midi_process.h"
#include "midimap.h"
#include "engine.h"
#include "buffer.h"
#include "patch.h"
#include "param.h"
#include "bpm.h"
#include "filter.h"
#include "bank.h"
#include "session.h"
#include "settings.h"
#include "gui_param.h"
#include "gui_patch.h"
#include "gui_midimap.h"
#include "debug.h"


int             vnum[MAX_PARTS];    /* round robin voice selectors */


/*****************************************************************************
 * init_midi_processor()
 *
 * initialize keylist nodes and round-robin voice selectors
 *****************************************************************************/
void
init_midi_processor()
{
	PART            *part;
	unsigned int    part_num;
	int             j;

	/* initialize keylist nodes and round-robin voice selectors */
	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		part = get_part(part_num);
		/* initialize keylist nodes */
		for (j = 0; j < 128; j++) {
			part->keylist[j].midi_key = j & 0x7F;
			part->keylist[j].next = NULL;
		}
		/* initialize round robin voice indices */
		vnum[part_num] = 0;

		/* initialize per-part midi event buffer */
		init_midi_event_queue(part_num);
	}
}


/*****************************************************************************
 * process_note_on()
 *
 * Process a note on event for a single part.  Add the note to the current
 * part's keylist and select a voice, stealing one if necessary.  All note on
 * events are processed with process_keytrigger() unless the velocity is set
 * to zero, and process_note_off() is used instead.
 *****************************************************************************/
void
process_note_on(MIDI_EVENT *event, unsigned int part_num)
{
	VOICE           *voice;
	VOICE           *old_voice     = NULL;
	VOICE           *loop_voice;
	PART            *part          = get_part(part_num);
	PATCH_STATE     *state         = get_active_state(part_num);
	int             voice_num;
	int             oldest_age;
	int             free_voice;
	int             steal_voice;
	int             same_key;

	/* if this is velocity 0 style note off, fall through */
	if (event->velocity > 0) {

		/* keep track of previous to newest key pressed! */
		part->prev_key = part->midi_key;
		part->midi_key = event->note & 0x7F;
		part->last_key = event->note & 0x7F;

		/* allocate voice for the different keymodes */
		switch (state->keymode) {
		case KEYMODE_MONO_SMOOTH:
			old_voice = get_voice(part_num, vnum[part_num]);
			old_voice->keypressed = -1;
			/* allocate new voice for staccato in mid-release only. */
			if (old_voice->allocated &&
			    ((old_voice->cur_amp_interval == ENV_INTERVAL_DECAY) ||
			     (old_voice->cur_amp_interval == ENV_INTERVAL_RELEASE) ||
			     (old_voice->cur_amp_interval == ENV_INTERVAL_FADE))) {
				old_voice->keypressed = -1;
				old_voice->cur_amp_interval = ENV_INTERVAL_SUSTAIN;
				old_voice->cur_amp_sample = -1;
				PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE,
				             "voice %2d:  ^^^^^^^  Mono-Smooth:  "
				             "Fading out voice.  (Note On)  ^^^^^^^\n",
				             (old_voice->id + 1));
				vnum[part_num] = (vnum[part_num] + 1) % setting_polyphony;
			}
			break;
		case KEYMODE_MONO_MULTIKEY:
			old_voice = get_voice(part_num, vnum[part_num]);
			break;
		case KEYMODE_MONO_RETRIGGER:
			for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
				loop_voice = get_voice(part_num, voice_num);
				if (loop_voice->allocated > 0) {
					loop_voice->keypressed = -1;
					loop_voice->cur_amp_interval = ENV_INTERVAL_RELEASE;
					loop_voice->cur_amp_sample = -1;
					old_voice = loop_voice;
					PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE,
					             "voice %2d:  ^^^^^^^  Mono-Retrigger:  "
					             "Fading out voice.  (Note On)  ^^^^^^^\n",
					             (loop_voice->id + 1));
				}
			}
			vnum[part_num] = (vnum[part_num] + setting_polyphony - 1) % setting_polyphony;
			break;
		case KEYMODE_POLY:
			vnum[part_num] = 0;

			/* voice allocation with note stealing */
			oldest_age          = 0;
			free_voice          = -1;
			steal_voice         = setting_polyphony - 1;
			same_key            = -1;

			/* look through all the voices */
			for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
				loop_voice = get_voice(part_num, voice_num);

				/* priority 1: a free voice */
				if (loop_voice->allocated == 0) {
					free_voice = voice_num;
					vnum[part_num] = free_voice;
					voice_num = setting_polyphony;
					break;
				}
				else {
					if (loop_voice->midi_key == part->midi_key) {
						PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
						             DEBUG_COLOR_RED "+++++++++++ "
						             DEBUG_COLOR_DEFAULT);
					}

					/* priority 2: find the absolute oldest in play */
					if ((same_key == -1) && (loop_voice->age > oldest_age)) {
						oldest_age = loop_voice->age;
						steal_voice = voice_num;
					}
				}
			}

			/* priorities 1 and 2 */
			if (free_voice >= 0) {
				vnum[part_num] = free_voice;
			}
			else {
				vnum[part_num] = steal_voice;
				PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE,
				             "*** Part %d:  stealing voice %d!\n",
				             (part_num + 1), vnum[part_num]);
				PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
				             DEBUG_COLOR_YELLOW "+++++++++++ "
				             DEBUG_COLOR_DEFAULT);
			}

			break;
		}

		/* keep pointer to current voice around */
		voice = get_voice(part_num, vnum[part_num]);

		/* assign midi note */
		voice->midi_key   = part->midi_key;
		voice->keypressed = part->midi_key;
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE, "  *** keypressed = %d  Allocating voice %d ***\n",
		             voice->keypressed, vnum[part_num]);
		voice->age        = 0;

		/* keep velocity for this note event */
		voice->velocity               = event->velocity;
		voice->velocity_target_linear = voice->velocity_coef_linear =
			part->velocity_target = part->velocity_coef = ((sample_t) event->velocity) * 0.01;
		voice->velocity_target_log    = voice->velocity_coef_log =
			velocity_gain_table[state->amp_velocity_cc][event->velocity];

		if (event->velocity > 0) {
			part->velocity = event->velocity;
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE,
			             "voice %2d:  Event Note On:   part=%d  "
			             "voice=%2d  note=%3d  velocity=%3d  keylist=:",
			             (vnum[part_num] + 1),
			             (part_num + 1),
			             (vnum[part_num] + 1),
			             event->note,
			             event->velocity);
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
			             DEBUG_COLOR_RED "!!!!!!! %d !!!!!!! "
			             DEBUG_COLOR_DEFAULT,
			             event->note);
		}

		/* staccato, or no previous notes in play */
		if ((part->prev_key == -1) || (part->head == NULL)) {
			old_voice = NULL;

			/* put this key at the start of the list */
			part->keylist[part->midi_key].next = NULL;
			part->head = & (part->keylist[part->midi_key]);
		}

		/* legato, or previous notes still in play */
		else {
			/* Link this key to the end of the list,
			   unlinking from the middle if necessary. */
			part->cur = part->head;
			part->prev = NULL;
			while (part->cur != NULL) {
				PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE, "%d:", part->cur->midi_key);
				if (part->cur == &part->keylist[part->midi_key]) {
					if (part->prev != NULL) {
						part->prev->next = part->cur->next;
					}
				}
				part->prev = part->cur;
				part->cur = part->cur->next;
			}
			//PHASEX_DEBUG (DEBUG_CLASS_MIDI_NOTE, "\n");
			part->cur = & (part->keylist[part->midi_key]);

			/* if there is no end of the list, link it to the head */
			if (part->prev == NULL) {
				part->head = part->cur;
				PHASEX_WARN("*** process_note_on(): [part%d] found "
				            "previous key in play with no keylist!\n",
				            (part_num + 1));
			}
			else {
				part->prev->next = part->cur;
			}
			part->cur->next = NULL;

			/* handle mono multikey when still in release tail */
			if ( (state->keymode == KEYMODE_MONO_MULTIKEY) &&
			     (old_voice->allocated > 0) &&
			     (old_voice->keypressed == -1) &&
			     (old_voice->cur_amp_interval < ENV_INTERVAL_DONE) ) {
				old_voice->cur_amp_sample   = -1;
				old_voice->cur_amp_interval = ENV_INTERVAL_RELEASE;
				vnum[part_num] = (vnum[part_num] + 1) % setting_polyphony;
				voice = get_voice(part_num, vnum[part_num]);
				voice->id = vnum[part_num];
			}
		}

		if (event->velocity > 0) {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE, "\n");
		}

		/* process parameters dependent on keytrigger events */
		process_keytrigger(event, old_voice, voice, part_num);
	}

	/* velocity 0 style note off */
	else {
		process_note_off(event, part_num);
	}
}


/*****************************************************************************
 * process_note_off()
 *
 * Process a single note-off event (either a note-off message, or a note-on
 * message with velocity set to zero).  Select a voices to be turned on/off,
 * remove key from list, and call process_keytrigger() for any portamento,
 * envelope, osc and lfo init-phase, and filter-follow triggering actions for
 * note-off events that cause new notes to be triggered (mono modes).
 *****************************************************************************/
void
process_note_off(MIDI_EVENT *event, unsigned int part_num)
{
	VOICE           *voice;
	VOICE           *old_voice      = NULL;
	VOICE           *loop_voice;
	PART            *part           = get_part(part_num);
	PATCH_STATE     *state          = get_active_state(part_num);
	int             keytrigger      = 0;
	int             free_voice      = -1;
	int             voice_num;
	int             unlink;

	switch (state->keymode) {
	case KEYMODE_POLY:
		/* find voice mapped to note being shut off */
		vnum[part_num] = -1;
		for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
			loop_voice = get_voice(part_num, voice_num);
			if (loop_voice->midi_key == event->note) {
				loop_voice->keypressed       = -1;
				loop_voice->cur_amp_sample   = -1;
				loop_voice->cur_amp_interval = ENV_INTERVAL_SUSTAIN;
				vnum[part_num] = voice_num;
			}
		}
		if (vnum[part_num] == -1) {
			vnum[part_num] = 0;
			keytrigger = 0;
		}
		break;

	case KEYMODE_MONO_SMOOTH:
		/* find voice mapped to note being shut off, if any */
		keytrigger = 0;
		for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
			loop_voice = get_voice(part_num, voice_num);
			if (loop_voice->keypressed == event->note) {
				loop_voice->keypressed       = -1;
				loop_voice->cur_amp_sample   = -1;
				loop_voice->cur_amp_interval = ENV_INTERVAL_SUSTAIN;
				if (event->note == part->midi_key) {
					old_voice = loop_voice;
				}
			}
		}
		break;

	case KEYMODE_MONO_RETRIGGER:
		/* find voice mapped to note being shut off, if any */
		keytrigger = 0;
		for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
			loop_voice = get_voice(part_num,
			                       ((voice_num + vnum[part_num] + 1) % setting_polyphony));
			if (loop_voice->allocated == 0) {
				free_voice = loop_voice->id;
			}
			else if (loop_voice->midi_key == event->note) {
				loop_voice->keypressed       = -1;
				loop_voice->cur_amp_sample   = -1;
				loop_voice->cur_amp_interval = ENV_INTERVAL_RELEASE;
				vnum[part_num] = (voice_num + 1) % setting_polyphony;
				if (event->note == part->midi_key) {
					old_voice = loop_voice;
				}
			}
		}
		if ((old_voice != NULL) && (free_voice > -1)) {
			vnum[part_num] = free_voice;
		}
		break;

	case KEYMODE_MONO_MULTIKEY:
		old_voice = get_voice(part_num, vnum[part_num]);
		break;
	}

	part->prev_key = part->midi_key;
	part->midi_key = event->note;

	PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE, "voice %2d:  Event Note Off:  part=%d  "
	             "voice=%2d  note=%3d  velocity=%3d  keylist=:",
	             (vnum[part_num] + 1),
	             (part_num + 1),
	             (vnum[part_num] + 1),
	             event->note,
	             event->velocity);

	/* remove this key from the list and then find the last key */
	part->prev = NULL;
	part->cur = part->head;
	unlink = 0;
	while (part->cur != NULL) {

		/* if note is found, unlink it from the list */
		if (part->cur->midi_key == event->note) {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE, "-%d-:", part->cur->midi_key);
			unlink = 1;
			if (part->prev != NULL) {
				part->prev->next = part->cur->next;
				part->cur->next  = NULL;
				part->cur        = part->prev->next;
			}
			else {
				part->head       = part->cur->next;
				part->cur->next  = NULL;
				part->cur        = part->head;
			}
		}
		/* otherwise, on to the next key in the list */
		else {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE, "%d:", part->cur->midi_key);
			part->prev = part->cur;
			part->cur  = part->cur->next;
		}
	}

	PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE, "\n");
	PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
	             DEBUG_COLOR_BLUE "------- %d ------- " DEBUG_COLOR_DEFAULT,
	             event->note);

	if (!unlink) {
		/* Received note-off w/o corresponding note-on. */
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
		             DEBUG_COLOR_RED "----------- " DEBUG_COLOR_DEFAULT);
	}

	/* keep pointer to current voice around */
	voice = get_voice(part_num, vnum[part_num]);

	/* ignore the note in the note off message if found in list */
	if ((part->prev != NULL) && (part->prev->midi_key == event->note)) {
		part->cur = part->head;
		while (part->cur != NULL) {
			if (part->cur->midi_key != event->note) {
				part->prev = part->cur;
			}
			part->cur  = part->cur->next;
		}
	}
	/* check for keys left on the list */
	if (part->prev != NULL) {
		/* set last/current keys in play respective of notes still held */
		part->last_key = part->prev->midi_key;
		part->midi_key = part->prev->midi_key;
		part->prev->next = NULL;

		/* Retrigger and smooth modes need voice allocation with keys
		   still on list multikey always need osc remapping until all
		   keys are done. */
		switch (state->keymode) {
		case KEYMODE_MONO_RETRIGGER:
			if (old_voice == NULL) {
				keytrigger--;
				break;
			}
			old_voice->cur_amp_interval = ENV_INTERVAL_RELEASE;
			old_voice->cur_amp_sample = -1;
			/* intentional fall-through */
		case KEYMODE_MONO_SMOOTH:
			voice->midi_key   = part->midi_key;
			voice->keypressed = part->midi_key;

			/* use previous velocity for this generated note event */
			voice->velocity               = part->velocity;
			voice->velocity_target_linear = voice->velocity_coef_linear =
				part->velocity_target     = part->velocity_coef =
				((sample_t) part->velocity) * 0.01;
			voice->velocity_target_log    = voice->velocity_coef_log =
				velocity_gain_table[state->amp_velocity_cc][part->velocity];

			/* intentional fall-through */
		case KEYMODE_MONO_MULTIKEY:
			keytrigger++;
			break;
		}
	}
	/* re-init list if no keys */
	else {
		voice->midi_key         = -1;
		voice->keypressed       = -1;
		part->head              = NULL;
		if (!part->hold_pedal) {
			part->midi_key      = -1;
		}
		if ( (state->keymode == KEYMODE_MONO_MULTIKEY) &&
		     (old_voice->allocated > 0) &&
		     (old_voice->keypressed == -1) &&
		     (old_voice->cur_amp_interval < ENV_INTERVAL_DONE) ) {
			old_voice->cur_amp_sample   = -1;
			old_voice->cur_amp_interval = ENV_INTERVAL_SUSTAIN;
			vnum[part_num] = (vnum[part_num] + 1) % setting_polyphony;
			voice = get_voice(part_num, vnum[part_num]);
			//voice->id = vnum[part_num];
			keytrigger = 0;
		}
	}

	if (keytrigger > 0) {
		process_keytrigger(event, old_voice, voice, part_num);
	}
}


/*****************************************************************************
 * process_all_notes_off()
 *
 * Process all notes off for a single channel.  The events for each channel
 * should have been queued once for each engine already.
 *****************************************************************************/
void
process_all_notes_off(MIDI_EVENT *event, unsigned int part_num)
{
	PART            *part = get_part(part_num);
	VOICE           *voice;
	unsigned int    voice_num;
	int             j;

	PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT, "*** All notes off!  Part %d:  event_type=0x%02x\n",
	             (part_num + 1), event->type);

	/* let engine shut off notes gracefully */
	for (voice_num = 0; voice_num < (unsigned int) setting_polyphony; voice_num++) {
		voice = get_voice(part_num, voice_num);
		voice->keypressed = -1;
	}

	/* re-initialize this part's keylists */
	part->head     = NULL;
	part->cur      = NULL;
	part->prev     = NULL;
	part->midi_key = -1;
	part->prev_key = -1;

	/* re-initialize this part's keylist nodes */
	for (j = 0; j < 128; j++) {
		part->keylist[j].midi_key = (short) j;
		part->keylist[j].next     = NULL;
	}
}


/*****************************************************************************
 * broadcast_notes_off()
 *
 * Called from any threads that needs to shut off all notes.  Queues a
 * notes-off event for all engine threads.
 *****************************************************************************/
void
broadcast_notes_off(void)
{
	PART                *part;
	MIDI_EVENT          queue_event;
	struct timespec     now;
	timecalc_t          delta_nsec;
	unsigned int        cycle_frame;
	unsigned int        m_index;
	unsigned int        part_num;

	delta_nsec  = get_time_delta(&now);
	cycle_frame = get_midi_cycle_frame(delta_nsec);
	m_index     = get_midi_index();
	PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
	             DEBUG_COLOR_CYAN "[%d] " DEBUG_COLOR_DEFAULT,
	             (m_index / buffer_period_size));

	queue_event.type      = MIDI_EVENT_NOTES_OFF;
	queue_event.state     = EVENT_STATE_ALLOCATED;

	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		part = get_part(part_num);
		queue_event.channel = (unsigned char)(part->midi_channel);
		queue_midi_event(part_num, &queue_event, cycle_frame, m_index);
	}
}


/*****************************************************************************
 * process_all_sound_off()
 *
 * Process all sound off for a single channel by first turning off all notes
 * and then zeroing out all audio buffers.  This event should have already
 * been queued once for each engine already.
 *****************************************************************************/
void
process_all_sound_off(MIDI_EVENT *event, unsigned int part_num)
{
	PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT, "*** All notes off!  Part %d:  event_type=0x%02x\n",
	             (part_num + 1), event->type);

	process_all_notes_off(event, part_num);

	init_engine_buffers();
}


/*****************************************************************************
 * process_keytrigger()
 *
 * Process voice/patch/param updates for keytrigger events.  This function
 * contains most of the logic for activating voices, and should be broken up
 * into its logical components.
 *****************************************************************************/
void
process_keytrigger(MIDI_EVENT   *UNUSED(event),
                   VOICE        *old_voice,
                   VOICE        *voice,
                   unsigned int part_num)
{
	VOICE           *loop_voice;
	PART            *part           = get_part(part_num);
	PATCH_STATE     *state          = get_active_state(part_num);
	int             osc;
	int             lfo;
	int             voice_num;
	int             staccato        = 1;
	int             env_trigger     = 0;
	sample_t        tmp;


	PHASEX_DEBUG(DEBUG_CLASS_MIDI_NOTE,
	             "voice %2d:  process_keytrigger():  old_voice=%2d  voice=%2d\n",
	             (voice->id + 1),
	             (old_voice == NULL ? 0 : (old_voice->id + 1)),
	             (voice->id + 1));

	/* check for notes currently in play */
	switch (state->keymode) {
	case KEYMODE_MONO_RETRIGGER:
		env_trigger = 1;
		/* intentional fall-through */
	case KEYMODE_MONO_MULTIKEY:
	case KEYMODE_MONO_SMOOTH:
		if (voice->cur_amp_interval >= ENV_INTERVAL_RELEASE) {
			staccato = 1;
			env_trigger = 1;
		}
		else {
			staccato = 0;
		}
		break;
	case KEYMODE_POLY:
		staccato = 1;
		for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
			loop_voice = get_voice(part_num, voice_num);
			if (loop_voice->cur_amp_interval < ENV_INTERVAL_RELEASE) {
				staccato = 0;
				break;
			}
		}
		env_trigger = 1;
		break;
	}

	/* staccato, mono retrig, and poly get new envelopes and initphases */
	if (env_trigger) {

		/* start new amp and filter envelopes */
		voice->cur_amp_interval    = ENV_INTERVAL_ATTACK;
		voice->cur_filter_interval = ENV_INTERVAL_ATTACK;

		voice->cur_amp_sample    = voice->amp_env_dur[ENV_INTERVAL_ATTACK] =
			env_interval_dur[ENV_INTERVAL_ATTACK][state->amp_attack];
		voice->cur_filter_sample = voice->filter_env_dur[ENV_INTERVAL_ATTACK] =
			env_interval_dur[ENV_INTERVAL_ATTACK][state->filter_attack];

		/* TODO: test with hold pedal.                                */
		/* without hold pedal:                                        */
		/* voice->amp_env_delta[ENV_INTERVAL_ATTACK]    =             */
		/*      (1.0 - voice->amp_env_raw) /                          */
		/*      (sample_t)voice->amp_env_dur[ENV_INTERVAL_ATTACK];    */
		/* voice->filter_env_delta[ENV_INTERVAL_ATTACK] =             */
		/*      (1.0 - voice->filter_env_raw) /                       */
		/*      (sample_t)voice->filter_env_dur[ENV_INTERVAL_ATTACK]; */
		/* with hold pedal:  everything until next comment.           */
		voice->amp_env_raw = 0.0;
		voice->filter_env_raw = 0.0;

		if (state->amp_attack || state->amp_decay) {
			voice->amp_env_delta[ENV_INTERVAL_ATTACK]    =
				(1.0 - voice->amp_env_raw) /
				(sample_t) voice->amp_env_dur[ENV_INTERVAL_ATTACK];
		}
		else {
			voice->amp_env_delta[ENV_INTERVAL_ATTACK]    =
				(state->amp_sustain - voice->amp_env_raw) /
				(sample_t) voice->amp_env_dur[ENV_INTERVAL_ATTACK];
		}
		if (state->filter_attack || state->filter_decay) {
			voice->filter_env_delta[ENV_INTERVAL_ATTACK] =
				(1.0 - voice->filter_env_raw) /
				(sample_t) voice->filter_env_dur[ENV_INTERVAL_ATTACK];
		}
		else {
			voice->filter_env_delta[ENV_INTERVAL_ATTACK] =
				(state->filter_sustain - voice->filter_env_raw) /
				(sample_t) voice->filter_env_dur[ENV_INTERVAL_ATTACK];
		}
		/* end of hold pedal code changes */

		/* everything except mono multikey gets new init phases. */
		if (state->keymode != KEYMODE_MONO_MULTIKEY) {
			/* init phase for keytrig oscs */
			for (osc = 0; osc < NUM_OSCS; osc++) {
				/* init phase (unshifted by lfo) at note start */
				if ((state->osc_freq_base[osc] == FREQ_BASE_TEMPO_KEYTRIG) ||
				    (state->osc_freq_base[osc] == FREQ_BASE_MIDI_KEY)) {
					voice->index[osc] = part->osc_init_index[osc];
				}
			}
		}

		/* init phase for keytrig lfos if needed */
		/* TODO:  determine if including env retrigger is appropriate here */
		//if ((staccato) || (env_trigger)) {
		if (staccato) {
			for (lfo = 0; lfo < NUM_LFOS; lfo++) {
				switch (state->lfo_freq_base[lfo]) {
				case FREQ_BASE_MIDI_KEY:
				case FREQ_BASE_TEMPO_KEYTRIG:
					part->lfo_index[lfo] = part->lfo_init_index[lfo];
					/* intentional fallthrough */
				case FREQ_BASE_TEMPO:
					part->lfo_adjust[lfo] = part->lfo_freq[lfo] * wave_period;
					break;
				}
			}
		}
	}

	/* Both high key and low key are set to the last key and adjusted later
	   if necessary */
	part->high_key = part->low_key = part->last_key;

	/* set highest and lowest keys in play for things that need
	   keyfollow */
	part->cur = part->head;
	while (part->cur != NULL) {
		if (part->cur->midi_key < part->low_key) {
			part->low_key = part->cur->midi_key;
		}
		if (part->cur->midi_key > part->high_key) {
			part->high_key = part->cur->midi_key;
		}
		part->cur = part->cur->next;
	}

	/* volume keyfollow is set by last key for poly */
	if (state->keymode == KEYMODE_POLY) {
		voice->vol_key = part->last_key;
	}
	/* volume keyfollow is set by high key for mono */
	else {
		voice->vol_key = part->high_key;
	}

	/* set filter keyfollow key based on keyfollow mode */
	switch (state->filter_keyfollow) {
	case KEYFOLLOW_LAST:
		tmp = (sample_t)(part->last_key + state->transpose - 64);
		for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
			loop_voice = get_voice(part_num, voice_num);
			loop_voice->filter_key_adj = tmp;
		}
		break;
	case KEYFOLLOW_HIGH:
		tmp = (sample_t)(part->high_key + state->transpose - 64);
		for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
			loop_voice = get_voice(part_num, voice_num);
			loop_voice->filter_key_adj = tmp;
		}
		break;
	case KEYFOLLOW_LOW:
		tmp = (sample_t)(part->low_key + state->transpose - 64);
		for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
			loop_voice = get_voice(part_num, voice_num);
			loop_voice->filter_key_adj = tmp;
		}
		break;
	case KEYFOLLOW_MIDI:
		voice->filter_key_adj = (sample_t)(part->last_key + state->transpose - 64);
		break;
	case KEYFOLLOW_NONE:
		voice->filter_key_adj = 0.0;
		break;
	}

	/* start at beginning of list of keys in play */
	part->cur = part->head;

	/* Keytrigger volume only applicable to midi key based oscs portamento
	   applicable to midi key based oscs _and_ lfos. */

	/* handle per-osc portamento for the different keymodes */
	for (osc = 0; osc < NUM_OSCS; osc++) {

		/* portamento for midi key based main osc */
		if (state->osc_freq_base[osc] == FREQ_BASE_MIDI_KEY) {

			/* decide which key in play to assign to this oscillator */
			switch (state->keymode) {
			case KEYMODE_MONO_MULTIKEY:
				/* use notes in order in oscillators */
				if (part->cur != NULL) {
					voice->osc_key[osc] = part->cur->midi_key;
					if (part->cur->next != NULL) {
						part->cur = part->cur->next;
					}
					else {
						part->cur = part->head;
					}
				}
				else {
					voice->osc_key[osc] = part->last_key;
				}
				break;
			case KEYMODE_MONO_SMOOTH:
			case KEYMODE_MONO_RETRIGGER:
				/* default mono -- use key just pressed */
				voice->osc_key[osc] = part->last_key;
				break;
			case KEYMODE_POLY:
				/* use midi key assigned to voice */
				voice->osc_key[osc] = voice->midi_key;
				break;
			}

			/* Set oscillator frequencies based on midi note, global transpose
			   value, and optional portamento.  state->osc_transpose[osc] is
			   taken into account every sample in the engine. */
			if ((state->portamento > 0)) {
				/* Portamento starts from previous key, no matter which voice. */
				if (part->prev_key == -1) {
					voice->osc_freq[osc] =
						freq_table[state->patch_tune_cc]
						[256 + part->last_key + state->transpose +
						 state->osc_transpose_cc[osc] - 64];
				}
				else {
					voice->osc_freq[osc] =
						freq_table[state->patch_tune_cc]
						[256 + part->prev_key + state->transpose +
						 state->osc_transpose_cc[osc] - 64];
				}
				/* Portamento slide calculation works the same for all
				   keymodes.  Start portamento now that frequency adjustment
				   is known. */
				if ((old_voice == NULL) || (old_voice == voice)) {
					voice->osc_portamento[osc] = 4.0 *
						(freq_table[state->patch_tune_cc]
						 [256 + voice->osc_key[osc] + state->transpose +
						  state->osc_transpose_cc[osc] - 64]
						 - voice->osc_freq[osc]) /
						(sample_t)(voice->portamento_samples - 1);
					part->portamento_sample  = part->portamento_samples;
					voice->portamento_sample = voice->portamento_samples;
				}
				else {
					voice->osc_portamento[osc] = 4.0 *
						(freq_table[state->patch_tune_cc]
						 [256 + voice->osc_key[osc] + state->transpose +
						  state->osc_transpose_cc[osc] - 64]
						 - old_voice->osc_freq[osc]) /
						(sample_t)(voice->portamento_samples - 1);
					part->portamento_sample  = part->portamento_samples;
					voice->portamento_sample = voice->portamento_samples;
					/* Mono modes set portamento on voice just finishing to
					   match new voice. */
					if ((state->keymode == KEYMODE_MONO_SMOOTH) ||
					    (state->keymode == KEYMODE_MONO_RETRIGGER)) {
						old_voice->osc_portamento[osc] =
							voice->osc_portamento[osc];
						old_voice->portamento_sample   =
							voice->portamento_sample;
						old_voice->portamento_samples  =
							voice->portamento_samples;
					}
				}
			}

			/* If portamento is not needed, set the oscillator frequency
			   directly. */
			else {
				voice->osc_freq[osc] = freq_table[state->patch_tune_cc]
					[256 + voice->osc_key[osc] +
					 state->transpose + state->osc_transpose_cc[osc] - 64];
				voice->osc_portamento[osc] = 0.0;
				voice->portamento_sample   = 0;
				voice->portamento_samples  = 0;
			}
		}
	}

	/* portamento for midi key based lfo */
	for (lfo = 0; lfo < NUM_LFOS; lfo++) {

		if (state->lfo_freq_base[lfo] == FREQ_BASE_MIDI_KEY) {

			/* decide which key in play to assign to this lfo */
			switch (state->keymode) {
			case KEYMODE_MONO_MULTIKEY:
				/* use notes in order in lfos */
				if (part->cur != NULL) {
					part->lfo_key[lfo] = part->cur->midi_key;
					if (part->cur->next != NULL) {
						part->cur = part->cur->next;
					}
					else {
						part->cur = part->head;
					}
				}
				else {
					part->lfo_key[lfo] = part->last_key;
				}
				break;
			case KEYMODE_MONO_SMOOTH:
			case KEYMODE_MONO_RETRIGGER:
				/* default mono -- use key just pressed */
				part->lfo_key[lfo] = part->last_key;
				break;
			case KEYMODE_POLY:
				/* use midi key assigned to allocated voice */
				part->lfo_key[lfo] = voice->midi_key;
				break;
			}

			/* Set lfo portamento frequencies based on
			   midi note and transpose value. */
			if ((state->portamento > 0) && (part->portamento_samples > 0)) {
				part->lfo_portamento[lfo] =
					(freq_table[state->patch_tune_cc]
					 [256 + part->lfo_key[lfo]] - part->lfo_freq[lfo]) /
					(sample_t) part->portamento_samples;
			}

			/* If portamento is not needed, set the lfo frequency directly. */
			else {
				part->lfo_portamento[lfo] = 0.0;
				part->lfo_freq[lfo] = freq_table[state->patch_tune_cc][256 + part->lfo_key[lfo]];
			}

		}
	}

	/* allocate voice (engine actually activates allocated voices) */
	voice->allocated = 1;
}


/*****************************************************************************
 * process_aftertouch()
 *****************************************************************************/
void
process_aftertouch(MIDI_EVENT *event, unsigned int part_num)
{
	VOICE       *voice;
	PATCH_STATE *state = get_active_state(part_num);
	int         voice_num;

	PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
	             "Event Key Presssure:  part=%d  note=%d  velocity=%d\n",
	             (part_num + 1), event->note, event->velocity);

	for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
		voice = get_voice(part_num, voice_num);
		if (event->note == voice->midi_key) {
			voice->velocity               = event->aftertouch;
			voice->velocity_target_linear = (sample_t) event->aftertouch * 0.01;
			voice->velocity_target_log    =
				velocity_gain_table[state->amp_velocity_cc][event->aftertouch];
		}
	}
}


/*****************************************************************************
 * process_polypressure()
 *****************************************************************************/
void
process_polypressure(MIDI_EVENT *event, unsigned int part_num)
{
	PART            *part  = get_part(part_num);
	PATCH_STATE     *state = get_active_state(part_num);
	VOICE           *voice;
	int             voice_num;

	PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
	             "Event Channel Pressure:  part=%d  value=%d\n",
	             (part_num + 1), event->polypressure);

	part->velocity_target = (sample_t) event->polypressure * 0.01;
	for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
		voice = get_voice(part_num, voice_num);
		if (voice->active) {
			voice->velocity               = event->polypressure;
			voice->velocity_target_linear = (sample_t) event->polypressure * 0.01;
			voice->velocity_target_log    =
				velocity_gain_table[state->amp_velocity_cc][event->polypressure];
		}
	}
}


/*****************************************************************************
 * param_midi_update()
 *
 * Handle synth engine patch state updates for cc events received via midi.
 *****************************************************************************/
void
param_midi_update(PARAM *param, int cc_val)
{
	SESSION         *session = get_current_session();
	unsigned int    id = param->info->id;

	/* ignore midi updates for locked params */
	if (!param->info->locked) {
		/* update param value */
		if (param->value.cc_val != cc_val) {
			param->value.cc_prev   = param->value.cc_val;
			param->value.cc_val    = cc_val;
			param->value.int_val   = cc_val + param->info->cc_offset;
			param->updated         = 1;
			param->patch->modified = 1;
			session->modified      = 1;
			/* update engine state */
			cb_info[id].update_patch_state(param);
		}
	}
}


/*****************************************************************************
 * process_controller()
 *****************************************************************************/
void
process_controller(MIDI_EVENT *event, unsigned int part_num)
{
	PARAM           *param;
	unsigned char   j;
	unsigned char   cc; /* last controller touched for midimap updates  */
	int             id; /* parameter id, used for finding callbacks */

	/* get controller number */
	cc = event->controller & 0x7F;

	PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
	             DEBUG_COLOR_GREEN "ccccc %d:%d=%d ccccc " DEBUG_COLOR_DEFAULT,
	             (part_num + 1), cc, event->value);

	/* 0x78-0x7F (120-127) are channel mode messages. */
	/* for now, just shut notes off */
	/* TODO: polyphony modes needs to map to these */
	if (cc >= 0x78) {
		if (cc == 0x78) {
			process_all_sound_off(event, part_num);
		}
		else {
			process_all_notes_off(event, part_num);
		}
	}
	else if (cc == MIDI_CONTROLLER_HOLD_PEDAL) {
		process_hold_pedal(event, part_num);
	}
	else {

		/* now walk through the params in the matrix */
		for (j = 0; j < 16; j++) {

			/* bail out if we're at the end of this controller chain */
			if ((id = ccmatrix[cc][j]) < 0) {
				break;
			}

			param = get_param(part_num, (unsigned int) id);

			/* clamp controller to range for parameter */
			if (event->value > param->info->cc_limit) {
				event->value = (unsigned char)(param->info->cc_limit);
			}

			/* set value for parameter */
			param_midi_update(param, event->value & 0x7F);

			/* update gui's copy and mark as updated */
			gui_param_midi_update(param, event->value & 0x7F);
		}
	}

	/* check for active cc edit */
	if (!cc_edit_ignore_midi && cc_edit_active) {
		cc_edit_cc_num = cc;
	}
}


/*****************************************************************************
 * process_parameter()
 *****************************************************************************/
void
process_parameter(MIDI_EVENT *event, unsigned int part_num)
{
	int             id      = event->parameter;
	PARAM           *param  = get_param(part_num, (unsigned int)id);

	PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
	             DEBUG_COLOR_GREEN "ppppp %d:%d=%d ppppp " DEBUG_COLOR_DEFAULT,
	             (part_num + 1), id, event->value);

	/* set parameter value and run callback */
	param_midi_update(param, event->value & 0x7F);
}


/*****************************************************************************
 * process_pitchbend()
 *****************************************************************************/
void
process_pitchbend(MIDI_EVENT *event, unsigned int part_num)
{
	PART            *part = get_part(part_num);
	unsigned int    bend_value;

	bend_value = ((unsigned int)(event->lsb & 0x7F) |
	              ((unsigned int)(event->msb & 0x7F) << 7));

	PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
	             "Received pitchbend event:  2-byte value = %d  (lsb=%d msb=%d)\n",
	             bend_value, event->lsb, event->msb);

	part->pitch_bend_target = ((sample_t)(bend_value) - 8192) * 0.0001220703125; /* 1/8192 */
}


/*****************************************************************************
 * process_program_change()
 *
 * TODO: handle bank and program select controller messages.
 *****************************************************************************/
void
process_program_change(MIDI_EVENT *event, unsigned int part_num)
{
	PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT, "-- Program Change:  Part %d:  %d %d\n",
	             (part_num + 1), event->program, event->value);

	midi_select_program(part_num, event->program);
}


/*****************************************************************************
 * process_phase_sync()
 *
 * Process a phase sync internal MIDI message.  Currently used for syncing
 * JACK Transport, this function performs phase correction for a given voice.
 *****************************************************************************/
void
process_phase_sync(MIDI_EVENT *event, unsigned int part_num)
{
	PART            *part              = get_part(part_num);
	PATCH_STATE     *state             = get_active_state(part_num);
	DELAY           *delay             = get_delay(part_num);
	CHORUS          *chorus            = get_chorus(part_num);
	VOICE           *voice;
	int             voice_num;
	int             osc;
	int             lfo;
	int             phase_correction   = event->value;
	sample_t        f_phase_correction = (sample_t) phase_correction;
	sample_t        tmp_1;

	delay->write_index += phase_correction;
	while (delay->write_index < 0.0) {
		delay->write_index += delay->bufsize;
	}
	while (delay->write_index >= delay->bufsize) {
		delay->write_index -= delay->bufsize;
	}

	chorus->lfo_index_a += f_phase_correction * chorus->lfo_adjust;
	while (chorus->lfo_index_a < 0.0) {
		chorus->lfo_index_a += F_WAVEFORM_SIZE;
	}
	while (chorus->lfo_index_a >= F_WAVEFORM_SIZE) {
		chorus->lfo_index_a -= F_WAVEFORM_SIZE;
	}

	chorus->lfo_index_b = chorus->lfo_index_a + (F_WAVEFORM_SIZE * 0.25);
	while (chorus->lfo_index_b < 0.0) {
		chorus->lfo_index_b += F_WAVEFORM_SIZE;
	}
	while (chorus->lfo_index_b >= F_WAVEFORM_SIZE) {
		chorus->lfo_index_b -= F_WAVEFORM_SIZE;
	}

	chorus->lfo_index_c = chorus->lfo_index_a + (F_WAVEFORM_SIZE * 0.5);
	while (chorus->lfo_index_c < 0.0) {
		chorus->lfo_index_c += F_WAVEFORM_SIZE;
	}
	while (chorus->lfo_index_c >= F_WAVEFORM_SIZE) {
		chorus->lfo_index_c -= F_WAVEFORM_SIZE;
	}

	chorus->lfo_index_d = chorus->lfo_index_a + (F_WAVEFORM_SIZE * 0.75);
	while (chorus->lfo_index_d < 0.0) {
		chorus->lfo_index_d += F_WAVEFORM_SIZE;
	}
	while (chorus->lfo_index_d >= F_WAVEFORM_SIZE) {
		chorus->lfo_index_d -= F_WAVEFORM_SIZE;
	}

	for (voice_num = 0; voice_num < setting_polyphony; voice_num++) {
		voice = get_voice(part_num, voice_num);
		for (osc = 0; osc < NUM_OSCS; osc++) {
			if (state->osc_freq_base[osc] >= FREQ_BASE_TEMPO) {
				switch (state->freq_mod_type[osc]) {
				case MOD_TYPE_LFO:
					tmp_1 = part->lfo_out[state->freq_lfo[osc]];
					break;
				case MOD_TYPE_OSC:
					tmp_1 = (voice->osc_out1[part->osc_freq_mod[osc]] +
					         voice->osc_out2[part->osc_freq_mod[osc]]) * 0.5;
					break;
				case MOD_TYPE_VELOCITY:
					tmp_1 = voice->velocity_coef_linear;
					break;
				default:
					tmp_1 = 0.0;
					break;
				}

				voice->index[osc] +=
					f_phase_correction *
					halfsteps_to_freq_mult((tmp_1 *
					                        state->freq_lfo_amount[osc]) +
					                       part->osc_pitch_bend[osc] +
					                       state->osc_transpose[osc]) *
					voice->osc_freq[osc] * wave_period;

				while (voice->index[osc] < 0.0) {
					voice->index[osc] += F_WAVEFORM_SIZE;
				}
				while (voice->index[osc] >= F_WAVEFORM_SIZE) {
					voice->index[osc] -= F_WAVEFORM_SIZE;
				}
			}
		}
	}
	for (lfo = 0; lfo < NUM_LFOS; lfo++) {
		if (state->lfo_freq_base[lfo] >= FREQ_BASE_TEMPO) {
			part->lfo_index[lfo] +=
				f_phase_correction *
				part->lfo_freq[lfo] *
				halfsteps_to_freq_mult(state->lfo_transpose[lfo] + part->lfo_pitch_bend[lfo]) *
				wave_period;

			while (part->lfo_index[lfo] < 0.0) {
				part->lfo_index[lfo] += F_WAVEFORM_SIZE;
			}
			while (part->lfo_index[lfo] >= F_WAVEFORM_SIZE) {
				part->lfo_index[lfo] -= F_WAVEFORM_SIZE;
			}
		}
	}
}


/*****************************************************************************
 * process_bpm_change()
 *
 * Process a BPM change event.  Currently used for JACK Transport sync.
 *****************************************************************************/
void
process_bpm_change(MIDI_EVENT *event, unsigned int part_num)
{
	PARAM       *param  = get_param(part_num, PARAM_BPM);

	PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "+++ Processing BPM change.  New BPM = %lf +++\n",
	             event->float_value);

	set_bpm(param, event->float_value);
}


/*****************************************************************************
 * process_hold_pedal()
 *
 * Process a Hold Pedal controller message.
 *****************************************************************************/
void
process_hold_pedal(MIDI_EVENT *event, unsigned int part_num)
{
	PART    *part = get_part(part_num);

	part->hold_pedal = (event->value > 64) ? 1 : 0;

	if (!part->hold_pedal || (part->head == NULL)) {
		part->midi_key = -1;
	}
}


/*****************************************************************************
 * process_midi_event()
 *
 * Perform complete processing for an event (straight from the queue).
 * All supported event types must be handled by this function.
 *****************************************************************************/
MIDI_EVENT *
process_midi_event(MIDI_EVENT *event, unsigned int part_num)
{
	MIDI_EVENT  *next;

	switch (event->type) {
	case MIDI_EVENT_NOTE_ON:
		process_note_on(event, part_num);
		break;
	case MIDI_EVENT_NOTE_OFF:
		process_note_off(event, part_num);
		break;
	case MIDI_EVENT_AFTERTOUCH:
		process_aftertouch(event, part_num);
		break;
	case MIDI_EVENT_NOTES_OFF:
	case MIDI_EVENT_STOP:
	case MIDI_EVENT_SYSTEM_RESET:
		process_all_notes_off(event, part_num);
		break;
	case MIDI_EVENT_PROGRAM_CHANGE:
		process_program_change(event, part_num);
		break;
	case MIDI_EVENT_POLYPRESSURE:
		process_polypressure(event, part_num);
		break;
	case MIDI_EVENT_CONTROLLER:
		process_controller(event, part_num);
		break;
	case MIDI_EVENT_PITCHBEND:
		process_pitchbend(event, part_num);
		break;
#ifdef MIDI_CLOCK_SYNC
	case MIDI_EVENT_CLOCK:
		break;
#endif /* MIDI_CLOCK_SYNC */
		/* The following are internal message types */
	case MIDI_EVENT_BPM_CHANGE:
		process_bpm_change(event, part_num);
		break;
	case MIDI_EVENT_PHASE_SYNC:
		process_phase_sync(event, part_num);
		break;
	case MIDI_EVENT_PARAMETER:
		process_parameter(event, part_num);
		break;
	default:
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
		             "+++ process_midi_event():  part %d:  "
		             "received unhandled MIDI message type 0x%02x\n",
		             (part_num + 1), event->type);
		break;
	}

	/* keep track of pointers to bulk lists */
	next = event->next;

	/* Clear event. */
	event->type    = 0;
	event->channel = 0;
	event->byte2   = 0;
	event->byte3   = 0;
	event->next    = NULL;
	event->state   = EVENT_STATE_FREE;

	return next;
}


/*****************************************************************************
 * process_midi_events()
 *****************************************************************************/
void
process_midi_events(unsigned int m_index, unsigned int cycle_frame, unsigned int part_num)
{
	PART            *part   = get_part(part_num);
	MIDI_EVENT      *event;

	event = & (part->event_queue[m_index + cycle_frame]);

	while ((event != NULL) && (event->state != EVENT_STATE_FREE)) {
		event = process_midi_event(event, part_num);
	}
}
