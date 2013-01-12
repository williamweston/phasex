/*****************************************************************************
 *
 * jack_transport.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2010 Anton Kormakov <assault64@gmail.com>
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
#ifndef _PHASEX_JACK_TRANSPORT_H_
#define _PHASEX_JACK_TRANSPORT_H_

#include <jack/jack.h>
#include "phasex.h"


/* JACK transport sync states: off, tempo sync, tempo and phases sync */
#define JACK_TRANSPORT_OFF      0
#define JACK_TRANSPORT_TEMPO    1
#define JACK_TRANSPORT_TNP      2


extern jack_position_t          jack_pos;
extern jack_transport_state_t   jack_state;
extern jack_transport_state_t   jack_prev_state;

extern int                      current_frame;
extern jack_nframes_t           prev_frame;

extern int                      frames_per_tick;
extern int                      frames_per_beat;

extern int                      phase_correction;
extern int                      need_resync;


extern void jack_process_transport(jack_nframes_t nframes);


#endif /* _PHASEX_JACK_TRANSPORT_H_ */
