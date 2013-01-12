/*****************************************************************************
 *
 * midi_event.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012-2013 William Weston <whw@linuxmail.org>
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
#ifndef _MIDI_EVENT_H_
#define _MIDI_EVENT_H_

#include "mididefs.h"
#include "engine.h"


void init_midi_event_queue(unsigned int part_num);
void queue_midi_event(unsigned int part_num,
                      MIDI_EVENT *event,
                      unsigned int cycle_frame,
                      unsigned int index);
void queue_midi_realtime_event(unsigned int part_num,
                               unsigned char type,
                               unsigned int cycle_frame,
                               unsigned int index);
void queue_midi_param_event(unsigned int part_num, unsigned int id, int cc_val);


#endif /* _MIDI_EVENT_H_ */
