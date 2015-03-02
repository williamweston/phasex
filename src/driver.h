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
#ifndef _PHASEX_DRIVER_H_
#define _PHASEX_DRIVER_H_


#define AUDIO_DRIVER_NONE           0
#define AUDIO_DRIVER_ALSA_PCM       1
#define AUDIO_DRIVER_JACK           2

#define MIDI_DRIVER_NONE            0
#define MIDI_DRIVER_JACK            1
#define MIDI_DRIVER_ALSA_SEQ        2
#define MIDI_DRIVER_RAW_ALSA        3
#define MIDI_DRIVER_RAW_GENERIC     4
#define MIDI_DRIVER_RAW_OSS         5


typedef void *(*THREAD_FUNC)(void *);
typedef int (*DRIVER_FUNC)(void);
typedef void (*DRIVER_VOID_FUNC)(void);
typedef int (*GET_INDEX_FUNC)(int);
typedef void (*CLEANUP_FUNC)(void *);


extern char             *audio_driver_name;
extern char             *midi_driver_name;

extern int              audio_driver;
extern int              midi_driver;

extern int              sample_rate_mode_changed;

extern DRIVER_FUNC      audio_init_func;
extern DRIVER_FUNC      audio_start_func;
extern DRIVER_FUNC      audio_stop_func;
extern DRIVER_VOID_FUNC audio_restart_func;
extern DRIVER_VOID_FUNC audio_watchdog_func;
extern THREAD_FUNC      audio_thread_func;

extern DRIVER_FUNC      midi_init_func;
extern DRIVER_FUNC      midi_start_func;
extern DRIVER_FUNC      midi_stop_func;
extern DRIVER_VOID_FUNC midi_restart_func;
extern DRIVER_VOID_FUNC midi_watchdog_func;
extern THREAD_FUNC      midi_thread_func;

extern pthread_mutex_t  audio_ready_mutex;
extern pthread_cond_t   audio_ready_cond;
extern int              audio_ready;

extern pthread_mutex_t  midi_ready_mutex;
extern pthread_cond_t   midi_ready_cond;
extern int              midi_ready;

extern int              audio_stopped;
extern int              midi_stopped;
extern int              engine_stopped;

extern char             *audio_driver_names[];
extern char             *midi_driver_names[];


void select_audio_driver(char *driver_name, int driver_id);
void select_midi_driver(char *driver_name, int driver_id);
void init_audio(void);
void init_midi(void);
void start_audio(void);
void start_midi(void);
void wait_audio_start(void);
void wait_midi_start(void);
void wait_audio_stop(void);
void wait_midi_stop(void);
void stop_audio(void);
void stop_midi(void);
void restart_audio(void);
void restart_midi(void);
void phasex_watchdog(void);
void scan_audio_and_midi(void);
int  audio_driver_running(void);
void query_audio_driver_status(char *buf);


#endif /* _PHASEX_DRIVER_H_ */
