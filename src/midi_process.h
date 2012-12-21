/*****************************************************************************
 *
 * midi_process.h
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
#ifndef _MIDI_PROCESS_H_
#define _MIDI_PROCESS_H_

#include "midi_event.h"


extern int              vnum[MAX_PARTS];    /* round robin voice selectors */

extern unsigned char    all_notes_off[MAX_PARTS];


void init_midi_processor();
MIDI_EVENT *process_midi_event(MIDI_EVENT *event, unsigned int part_num);

void process_note_on(MIDI_EVENT *event, unsigned int part_num);
void process_note_off(MIDI_EVENT *event, unsigned int part_num);
void process_all_notes_off(MIDI_EVENT *event, unsigned int part_num);
void broadcast_notes_off(void);
void process_all_sound_off(MIDI_EVENT *event, unsigned int part_num);
void process_keytrigger(MIDI_EVENT *UNUSED(event),
                        VOICE *old_voice,
                        VOICE *voice,
                        unsigned int part_num);
void process_aftertouch(MIDI_EVENT *event, unsigned int part_num);
void process_polypressure(MIDI_EVENT *event, unsigned int part_num);
void param_midi_update(PARAM *param, int cc_val);
void process_controller(MIDI_EVENT *event, unsigned int part_num);
void process_pitchbend(MIDI_EVENT *event, unsigned int part_num);
void process_program_change(MIDI_EVENT *event, unsigned int part_num);
void process_phase_sync(MIDI_EVENT *event, unsigned int part_num);
void process_bpm_change(MIDI_EVENT *event, unsigned int part_num);
void process_hold_pedal(MIDI_EVENT *event, unsigned int part_num);
void process_midi_clock(MIDI_EVENT *event, unsigned int part_num);


#endif /* _MIDI_PROCESS_H_ */
