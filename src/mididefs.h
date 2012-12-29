/*****************************************************************************
 *
 * mididefs.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012 William Weston <whw@linuxmail.org>
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
#ifndef _PHASEX_MIDI_DEFS_H_
#define _PHASEX_MIDI_DEFS_H_

//#include <jack/jack.h>
#include <glib.h>
#include "phasex.h"


/* must be as large as longest buffer period */
#define MIDI_EVENT_POOL_SIZE        PHASEX_MAX_BUFSIZE
#define MIDI_EVENT_POOL_MASK        (PHASEX_MAX_BUFSIZE - 1)


/* MIDI event types */

/* MIDI pseudo messages, for queuing to PHASEX engine only */
#define MIDI_EVENT_NO_EVENT         0x00    /* placeholder for empty events */
#define MIDI_EVENT_PHASE_SYNC       0x01    /* resync phases with JACK Transport */
#define MIDI_EVENT_BPM_CHANGE       0x02    /* resync BPM with JACK Transport */
#define MIDI_EVENT_PARAMETER        0x03    /* internal parameter representation */
#define MIDI_EVENT_NOTES_OFF        0x04

/* Types < 0xF0 have MIDI channel as 4 least significant bits. */
#define MIDI_EVENT_NOTE_OFF         0x80    /* note         velocity    */
#define MIDI_EVENT_NOTE_ON          0x90    /* note         velocity    */
#define MIDI_EVENT_AFTERTOUCH       0xA0    /* note         aftertouch  */
#define MIDI_EVENT_CONTROLLER       0xB0    /* controller   value       */
#define MIDI_EVENT_PROGRAM_CHANGE   0xC0    /* program                  */
#define MIDI_EVENT_POLYPRESSURE     0xD0    /* polypressure             */
#define MIDI_EVENT_PITCHBEND        0xE0    /* bend LSB bend MSB        */

/* MIDI system messages */
#define MIDI_EVENT_SYSEX            0xF0    /* 1-4 byte vendor ID + ...?    */
#define MIDI_EVENT_MTC_QFRAME       0xF1    /* 0tttvvvv t=type v=value      */
#define MIDI_EVENT_SONGPOS          0xF2    /* position LSB position MSB    */
#define MIDI_EVENT_SONG_SELECT      0xF3    /* song number                  */
#define MIDI_EVENT_BUS_SELECT       0xF5    /* See http://www.srm.com/qtma/davidsmidispec.html */
#define MIDI_EVENT_TUNE_REQUEST     0xF6    /* 0 bytes                      */
#define MIDI_EVENT_END_SYSEX        0xF7    /* 0 bytes                      */
#define MIDI_EVENT_TICK             0xF8    /* 0 bytes (24x per quarter)    */
#define MIDI_EVENT_START            0xFA    /* 0 bytes                      */
#define MIDI_EVENT_CONTINUE         0xFB    /* 0 bytes                      */
#define MIDI_EVENT_STOP             0xFC    /* 0 bytes                      */
#define MIDI_EVENT_ACTIVE_SENSING   0xFE    /* 0 bytes                      */
#define MIDI_EVENT_SYSTEM_RESET     0xFF    /* 0 bytes                      */

/* MIDI status byte bitmasks */
#define MIDI_TYPE_MASK              0xF0
#define MIDI_CHANNEL_MASK           0x0F

/* PHASEX MIDI event states */
/* Non-queued event states are negative. */
/* A positive event state reperesents frame number. */
#define EVENT_STATE_FREE            -1
#define EVENT_STATE_INIT            -2
#define EVENT_STATE_ALLOCATED       -3
#define EVENT_STATE_QUEUED          -4
#define EVENT_STATE_DEQUEUED        -5
#define EVENT_STATE_COMPLETED       -6
#define EVENT_STATE_ABANDONED       -7

/* MIDI controller definitions */
#define MIDI_CONTROLLER_HOLD_PEDAL  64


/* PHASEX MIDI event structure */
typedef struct midi_event {
	union {
		volatile gint       state;
		int                 frame;
	} __attribute__((__transparent_union__));
	unsigned char       type;
	unsigned char       channel;
	union {
		unsigned char       note;
		unsigned char       controller;
		unsigned char       program;
		unsigned char       polypressure;
		unsigned char       pitchbend;
		unsigned char       lsb;
		unsigned char       byte2;
		unsigned char       parameter;
	} __attribute__((__transparent_union__));
	union {
		unsigned char       velocity;
		unsigned char       aftertouch;
		unsigned char       value;
		unsigned char       msb;
		unsigned char       byte3;
	} __attribute__((__transparent_union__));
	sample_t            float_value;
	union {
		struct midi_event   *next;
		volatile gpointer   gnext;
	} __attribute__((__transparent_union__));
} MIDI_EVENT;


#endif /* _PHASEX_MIDI_DEFS_H_ */
