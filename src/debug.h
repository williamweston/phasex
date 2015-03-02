/*****************************************************************************
 *
 * debug.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012-2015 Willaim Weston <william.h.weston@gmail.com>
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
#ifndef _PHASEX_DEBUG_H_
#define _PHASEX_DEBUG_H_

#include <glib.h>
#include "stdio.h"
#include "string.h"
#include "phasex.h"


#define DEBUG_MESSAGE_SIZE          256

#define DEBUG_MESSAGE_POOL_SIZE     2048
#define DEBUG_BUFFER_MASK           (DEBUG_MESSAGE_POOL_SIZE - 1)

#define DEBUG_CLASS_NONE            0
#define DEBUG_CLASS_INIT            (1<<1)
#define DEBUG_CLASS_GUI             (1<<2)
#define DEBUG_CLASS_PARAM           (1<<3)
#define DEBUG_CLASS_MIDI            (1<<4)
#define DEBUG_CLASS_MIDI_NOTE       (1<<5)
#define DEBUG_CLASS_MIDI_EVENT      (1<<6)
#define DEBUG_CLASS_MIDI_TIMING     (1<<7)
#define DEBUG_CLASS_AUDIO           (1<<8)
#define DEBUG_CLASS_JACK_TRANSPORT  (1<<9)
#define DEBUG_CLASS_RAW_MIDI        (1<<10)
#define DEBUG_CLASS_ENGINE          (1<<11)
#define DEBUG_CLASS_ENGINE_TIMING   (1<<12)
#define DEBUG_CLASS_SESSION         (1<<13)
#define DEBUG_CLASS_ALL             ~(0UL | DEBUG_CLASS_MIDI_TIMING | DEBUG_CLASS_ENGINE_TIMING)

#define DEBUG_ATTR_RESET            0
#define DEBUG_ATTR_BRIGHT           1
#define DEBUG_ATTR_DIM              2
#define DEBUG_ATTR_UNDERLINE        3
#define DEBUG_ATTR_BLINK            4
#define DEBUG_ATTR_REVERSE          7
#define DEBUG_ATTR_HIDDEN           8

#define DEBUG_COLOR_RED             "\x1B[0;91;49m"
#define DEBUG_COLOR_GREEN           "\x1B[0;32;49m"
#define DEBUG_COLOR_ORANGE          "\x1B[0;33;49m"
#define DEBUG_COLOR_YELLOW          "\x1B[0;93;49m"
#define DEBUG_COLOR_BLUE            "\x1B[0;34;49m"
#define DEBUG_COLOR_LTBLUE          "\x1B[0;94;49m"
#define DEBUG_COLOR_MAGENTA         "\x1B[0;35;49m"
#define DEBUG_COLOR_PINK            "\x1B[0;95;49m"
#define DEBUG_COLOR_CYAN            "\x1B[0;36;49m"
#define DEBUG_COLOR_WHITE           "\x1B[0;37;49m"
#define DEBUG_COLOR_DEFAULT         "\x1B[0;37;49m"


typedef struct debug_class {
	unsigned long       id;
	char                *name;
} DEBUG_CLASS;

typedef struct debug_msg {
	char                msg[DEBUG_MESSAGE_SIZE];
} DEBUG_MESSAGE;

typedef struct debug_queue {
	DEBUG_MESSAGE       *head;
	DEBUG_MESSAGE       *divider;
	DEBUG_MESSAGE       *tail;
} DEBUG_QUEUE;

typedef struct debug_ringbuffer {
	DEBUG_MESSAGE       msgs[DEBUG_MESSAGE_POOL_SIZE];
	volatile gint       read_index;
	volatile gint       write_index;
	volatile gint       insert_index;
} DEBUG_RINGBUFFER;


extern DEBUG_RINGBUFFER main_debug_queue;

extern int              debug;
extern unsigned long    debug_class;

extern DEBUG_CLASS      debug_class_list[16];


#define PHASEX_ERROR(args...)                                           \
	{ \
		int old_debug_index; \
		int new_debug_index; \
		do { \
			old_debug_index = g_atomic_int_get (&(main_debug_queue.insert_index)); \
			new_debug_index = (old_debug_index + 1) & DEBUG_BUFFER_MASK; \
		} while (!g_atomic_int_compare_and_exchange (&(main_debug_queue.insert_index), \
		                                             old_debug_index, new_debug_index)); \
		snprintf (main_debug_queue.msgs[main_debug_queue.insert_index].msg, \
		          DEBUG_MESSAGE_SIZE, args); \
		do { \
			old_debug_index = g_atomic_int_get (&(main_debug_queue.write_index)); \
			new_debug_index = (old_debug_index + 1) & DEBUG_BUFFER_MASK; \
		} while (!g_atomic_int_compare_and_exchange (&(main_debug_queue.write_index), \
		                                             old_debug_index, new_debug_index)); \
	}

#define PHASEX_WARN(args...)                                            \
	{ \
		int old_debug_index; \
		int new_debug_index; \
		do { \
			old_debug_index = g_atomic_int_get (&(main_debug_queue.insert_index)); \
			new_debug_index = (old_debug_index + 1) & DEBUG_BUFFER_MASK; \
		} while (!g_atomic_int_compare_and_exchange (&(main_debug_queue.insert_index), \
		                                             old_debug_index, new_debug_index)); \
		snprintf (main_debug_queue.msgs[main_debug_queue.insert_index].msg, \
		          DEBUG_MESSAGE_SIZE, args); \
		do { \
			old_debug_index = g_atomic_int_get (&(main_debug_queue.write_index)); \
			new_debug_index = (old_debug_index + 1) & DEBUG_BUFFER_MASK; \
		} while (!g_atomic_int_compare_and_exchange (&(main_debug_queue.write_index), \
		                                             old_debug_index, new_debug_index)); \
	}

#ifdef ENABLE_DEBUG

# define PHASEX_DEBUG(class, args...)                                   \
	if (debug_class & class) { \
		int old_debug_index; \
		int new_debug_index; \
		do { \
			old_debug_index = g_atomic_int_get (&(main_debug_queue.insert_index)); \
			new_debug_index = (old_debug_index + 1) & DEBUG_BUFFER_MASK; \
		} while (!g_atomic_int_compare_and_exchange (&(main_debug_queue.insert_index), \
		                                             old_debug_index, new_debug_index)); \
		snprintf (main_debug_queue.msgs[main_debug_queue.insert_index].msg, \
		          DEBUG_MESSAGE_SIZE, args); \
		do { \
			old_debug_index = g_atomic_int_get (&(main_debug_queue.write_index)); \
			new_debug_index = (old_debug_index + 1) & DEBUG_BUFFER_MASK; \
		} while (!g_atomic_int_compare_and_exchange (&(main_debug_queue.write_index),	\
		                                             old_debug_index, new_debug_index)); \
	}

#else /* !ENABLE_DEBUG */

# define PHASEX_DEBUG(class, args...) {}

#endif /* !ENABLE_DEBUG */


void *phasex_debug_thread(void *UNUSED(arg));


#endif /* _PHASEX_DEBUG_H_ */
