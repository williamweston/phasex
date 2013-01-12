/*****************************************************************************
 *
 * alsa_seq.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2013 William Weston <whw@linuxmail.org>
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
#ifndef _ALSA_SEQ_H_
#define _ALSA_SEQ_H_

#include <pthread.h>
#include <asoundlib.h>
#include "phasex.h"
#include "mididefs.h"


typedef struct alsa_seq_port {
	int                         client;
	int                         port;
	unsigned int                type;
	char                        *client_name;
	char                        *port_name;
	char                        alsa_name[16];
	snd_seq_port_subscribe_t    *subs;
	short                       subscribe_request;
	short                       unsubscribe_request;
	struct alsa_seq_port        *next;
} ALSA_SEQ_PORT;

typedef struct alsa_seq_info {
	snd_seq_t                   *seq;
	struct pollfd               *pfd;
	int                         npfds;
	short                       auto_hw;
	short                       auto_sw;
	ALSA_SEQ_PORT               *in_port;
	ALSA_SEQ_PORT               *src_ports;
	ALSA_SEQ_PORT               *capture_ports;
	ALSA_SEQ_PORT               *playback_ports;
	MIDI_EVENT                  event;
} ALSA_SEQ_INFO;


extern ALSA_SEQ_INFO           *alsa_seq_info;

extern int                     alsa_seq_ports_changed;


void alsa_error_handler(const char *file,
                        int line,
                        const char *func,
                        int err,
                        const char *fmt, ...);
void alsa_seq_port_free(ALSA_SEQ_PORT *portinfo);
int alsa_seq_port_list_compare(ALSA_SEQ_PORT *a, ALSA_SEQ_PORT *b);
ALSA_SEQ_PORT *alsa_seq_get_port_list(ALSA_SEQ_INFO *midi,
                                      unsigned int caps,
                                      ALSA_SEQ_PORT *orig_list);
void alsa_seq_subscribe_port(ALSA_SEQ_INFO *midi, ALSA_SEQ_PORT *seq_port, char *port_str_list);
void alsa_seq_unsubscribe_port(ALSA_SEQ_INFO *midi, ALSA_SEQ_PORT *seq_port);
void alsa_seq_subscribe_ports(ALSA_SEQ_INFO *midi,
                              ALSA_SEQ_PORT *seq_port_list,
                              unsigned int type,
                              char *port_str_list,
                              char *port_name_match,
                              ALSA_SEQ_PORT *selection_list);
void alsa_seq_watchdog_cycle(void);
ALSA_SEQ_INFO *open_alsa_seq_in(char *alsa_port);
void alsa_seq_cleanup(void *UNUSED(arg));
int alsa_seq_init(void);
void *alsa_seq_thread(void *UNUSED(arg));


#endif /* _ALSA_SEQ_H_ */
