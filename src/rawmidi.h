/******************************************************************************
 *
 * rawmidi.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2001-2015 Willaim Weston <william.h.weston@gmail.com>
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
 ******************************************************************************/
#ifndef _PHASEX_RAWMIDI_H_
#define _PHASEX_RAWMIDI_H_

#include "phasex.h"


#if !defined(ENABLE_RAWMIDI_OSS) && !defined(ENABLE_RAWMIDI_ALSA_RAW) && !defined(ENABLE_RAWMIDI_GENERIC)
# error "Please enable at least one raw MIDI driver to build with raw MIDI support."
#endif


#ifdef ENABLE_RAWMIDI_OSS
# include <linux/soundcard.h>
#endif

#ifdef ENABLE_RAWMIDI_ALSA_RAW
# include <alsa/asoundlib.h>
#endif


typedef struct rawmidi_info {
	char                *device;
#if defined(ENABLE_RAWMIDI_OSS) || defined(ENABLE_RAWMIDI_GENERIC)
	int                 fd;
#endif
#ifdef ENABLE_RAWMIDI_ALSA_RAW
	snd_rawmidi_t       *handle;
	struct pollfd       *pfds;
	int                 npfds;
#endif
} RAWMIDI_INFO;


#ifdef ENABLE_RAWMIDI_ALSA_RAW

typedef struct alsa_rawmidi_hw_info {
	int                         card_num;
	int                         device_num;
	int                         subdevice_num;
	int                         connect_request;
	int                         disconnect_request;
	char                        *device_id;
	char                        *device_name;
	char                        *subdevice_name;
	char                        alsa_name[32];
	struct alsa_rawmidi_hw_info *next;
} ALSA_RAWMIDI_HW_INFO;

#endif


extern RAWMIDI_INFO         *rawmidi_info;

extern ALSA_RAWMIDI_HW_INFO *alsa_rawmidi_hw;

extern int                  alsa_rawmidi_hw_changed;


#ifdef ENABLE_RAWMIDI_ALSA_RAW
void alsa_rawmidi_hw_info_free(ALSA_RAWMIDI_HW_INFO *hwinfo);
ALSA_RAWMIDI_HW_INFO *alsa_rawmidi_get_hw_list(void);
#endif
RAWMIDI_INFO *rawmidi_open(char *device);
int rawmidi_close(RAWMIDI_INFO *rawmidi);
int rawmidi_free(RAWMIDI_INFO *rawmidi);
int rawmidi_read(RAWMIDI_INFO *rawmidi, unsigned char *buf, int len);
int rawmidi_flush(RAWMIDI_INFO *rawmidi);
void rawmidi_watchdog_cycle(void);
int rawmidi_init(void);
void rawmidi_cleanup(void *UNUSED(arg));
void *rawmidi_thread(void *UNUSED(arg));


#endif /* _PHASEX_RAWMIDI_H_ */
