/*****************************************************************************
 *
 * alsa_seq.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2012 William Weston <whw@linuxmail.org>
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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <asoundlib.h>
#include "phasex.h"
#include "config.h"
#include "timekeeping.h"
#include "buffer.h"
#include "alsa_seq.h"
#include "mididefs.h"
#include "midi_event.h"
#include "midi_process.h"
#include "engine.h"
#include "patch.h"
#include "param.h"
#include "filter.h"
#include "bank.h"
#include "settings.h"
#include "driver.h"
#include "debug.h"

#ifndef WITHOUT_LASH
# include "lash.h"
#endif


ALSA_SEQ_INFO   *alsa_seq_info;

int             alsa_seq_ports_changed      = 0;


/*****************************************************************************
 * alsa_error_handler()
 *
 * Placeholder for a real error handling function.  Please note that this
 * handles errors for all of ALSA lib, not just ALSA seq.
 *****************************************************************************/
void
alsa_error_handler(const char *file, int line, const char *func, int err, const char *fmt, ...)
{
	PHASEX_ERROR("Unhandled ALSA error %d in function %s from file %s line %d:\n",
	             err, func, file, line);
	PHASEX_ERROR(fmt);
}


/*****************************************************************************
 * alsa_seq_port_free()
 *****************************************************************************/
void
alsa_seq_port_free(ALSA_SEQ_PORT *portinfo)
{
	ALSA_SEQ_PORT   *cur = portinfo;
	ALSA_SEQ_PORT   *next;

	while (cur != NULL) {
		if (cur->client_name != NULL) {
			free(cur->client_name);
		}
		if (cur->port_name != NULL) {
			free(cur->port_name);
		}
		if (cur->subs != NULL) {
			snd_seq_port_subscribe_free(cur->subs);
		}
		next = cur->next;
		free(cur);
		cur = next;
	}
}


/*****************************************************************************
 * alsa_seq_port_list_compare()
 *****************************************************************************/
int
alsa_seq_port_list_compare(ALSA_SEQ_PORT *a, ALSA_SEQ_PORT *b)
{
	ALSA_SEQ_PORT   *cur_a = a;
	ALSA_SEQ_PORT   *cur_b = b;

	while (cur_a != NULL) {
		if ((cur_b == NULL) ||
		    (cur_a->client != cur_b->client) ||
		    (cur_a->port != cur_b->port) ||
		    (strcmp(cur_a->client_name, cur_b->client_name) != 0)) {
			return 1;
		}
		cur_a = cur_a->next;
		cur_b = cur_b->next;
	}
	if (cur_b != NULL) {
		return 1;
	}

	return 0;
}


/*****************************************************************************
 * alsa_seq_port_in_list()
 *****************************************************************************/
int
alsa_seq_port_in_list(int client, int port, ALSA_SEQ_PORT *port_list)
{
	ALSA_SEQ_PORT   *cur = port_list;

	while (cur != NULL) {
		if ((client == cur->client) && (port == cur->port)) {
			return 1;
		}
		cur = cur->next;
	}

	return 0;
}


/*****************************************************************************
 * alsa_seq_get_port_list()
 *****************************************************************************/
ALSA_SEQ_PORT *
alsa_seq_get_port_list(ALSA_SEQ_INFO *seq_info, unsigned int caps, ALSA_SEQ_PORT *list)
{
	ALSA_SEQ_PORT           *head  = list;
	ALSA_SEQ_PORT           *check = list;
	ALSA_SEQ_PORT           *prev  = NULL;
	ALSA_SEQ_PORT           *cur   = NULL;
	ALSA_SEQ_PORT           *seq_port;
	snd_seq_client_info_t   *cinfo;
	snd_seq_port_info_t     *pinfo;
	unsigned int            type;
	int                     client;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);

	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(seq_info->seq, cinfo) >= 0) {
		client = snd_seq_client_info_get_client(cinfo);

		snd_seq_port_info_set_client(pinfo, client);
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(seq_info->seq, pinfo) >= 0) {
			if (!((type = snd_seq_port_info_get_type(pinfo)) &
			      SND_SEQ_PORT_TYPE_MIDI_GENERIC)) {
				continue;
			}
			if ((snd_seq_port_info_get_capability(pinfo) & caps) != caps) {
				continue;
			}
			if (alsa_seq_port_in_list(snd_seq_port_info_get_client(pinfo),
			                          snd_seq_port_info_get_port(pinfo), list)) {
				continue;
			}
			if ((seq_port = malloc(sizeof(ALSA_SEQ_PORT))) == NULL) {
				phasex_shutdown("Out of memory!\n");
			}
			seq_port->type        = type;
			seq_port->client      = snd_seq_port_info_get_client(pinfo);
			seq_port->port        = snd_seq_port_info_get_port(pinfo);
			seq_port->client_name = strdup(snd_seq_client_info_get_name(cinfo));
			seq_port->port_name   = strdup(snd_seq_port_info_get_name(pinfo));
			snprintf(seq_port->alsa_name,
			         sizeof(seq_port->alsa_name),
			         "%d:%d",
			         seq_port->client,
			         seq_port->port);
			seq_port->subs = NULL;
			seq_port->next = NULL;
			seq_port->subscribe_request   = 0;
			seq_port->unsubscribe_request = 0;
			if (head == NULL) {
				head = check = cur = seq_port;
			}
			else {
				while (check != NULL) {
					if ((check->client > seq_port->client) ||
					    ((check->client == seq_port->client) &&
					     (check->port > seq_port->port))) {
						seq_port->next = check;
						if (prev == NULL) {
							seq_port->next = head;
							head = prev = seq_port;
						}
						else {
							prev->next = seq_port;
						}
						prev = seq_port;
						break;
					}
					else {
						prev = check;
						check = check->next;
					}
				}
				if (check == NULL) {
					cur->next = seq_port;
					cur = seq_port;
				}
			}
		}
	}

	return head;
}


/*****************************************************************************
 * alsa_seq_subscribe_port()
 *****************************************************************************/
void
alsa_seq_subscribe_port(ALSA_SEQ_INFO *seq_info, ALSA_SEQ_PORT *seq_port, char *port_str_list)
{
	snd_seq_addr_t              sender;
	snd_seq_addr_t              dest;
	snd_seq_port_subscribe_t    *subs;

	if (seq_port->subs == NULL) {
		sender.client = (unsigned char)(seq_port->client          & 0xFF);
		sender.port   = (unsigned char)(seq_port->port            & 0xFF);
		dest.client   = (unsigned char)(seq_info->in_port->client & 0xFF);
		dest.port     = (unsigned char)(seq_info->in_port->port   & 0xFF);

		snd_seq_port_subscribe_malloc(&subs);
		snd_seq_port_subscribe_set_sender(subs, &sender);
		snd_seq_port_subscribe_set_dest(subs, &dest);
		snd_seq_subscribe_port(seq_info->seq, subs);

		seq_port->subs = subs;
		seq_port->subscribe_request   = 0;
		seq_port->unsubscribe_request = 0;

		if (port_str_list != NULL) {
			strcat(port_str_list, seq_port->alsa_name);
			strcat(port_str_list, ",");
		}
		PHASEX_DEBUG(DEBUG_CLASS_MIDI, "  Subscribed to [%s] %s: %s\n",
		             seq_port->alsa_name, seq_port->client_name, seq_port->port_name);
	}
}


/*****************************************************************************
 * alsa_seq_unsubscribe_port()
 *****************************************************************************/
void
alsa_seq_unsubscribe_port(ALSA_SEQ_INFO *seq_info, ALSA_SEQ_PORT *seq_port)
{
	if (seq_port->subs != NULL) {
		snd_seq_unsubscribe_port(seq_info->seq, seq_port->subs);
		snd_seq_port_subscribe_free(seq_port->subs);

		seq_port->subs = NULL;
		seq_port->subscribe_request   = 0;
		seq_port->unsubscribe_request = 0;

		PHASEX_DEBUG(DEBUG_CLASS_MIDI,
		             "  Unsubscribed from [%s] %s: %s\n",
		             seq_port->alsa_name,
		             seq_port->client_name,
		             seq_port->port_name);
	}
}


/*****************************************************************************
 * alsa_seq_subscribe_ports()
 *   ALSA_SEQ_INFO *seq_info        collected information about our seq client.
 *   ALSA_SEQ_PORT *port_list   list of available subscription ports
 *   unsigned int type          type mask for sequencer port  (0 == no mask)
 *   char *port_str_list        buffer for building list (NULL to disable).
 *   char *port_name_match      substring match for port name (NULL == no match)
 *****************************************************************************/
void
alsa_seq_subscribe_ports(ALSA_SEQ_INFO *seq_info,
                         ALSA_SEQ_PORT *seq_port_list,
                         unsigned int  type,
                         char          *port_str_list,
                         char          *port_name_match,
                         ALSA_SEQ_PORT *selection_list)
{
	char                *alsa_name_match = NULL;
	ALSA_SEQ_PORT       *cur             = seq_port_list;
	ALSA_SEQ_PORT       *check;
	int                 port_match_found;

	while (cur != NULL) {

		/* if port match is given, ignore port names that don't match */
		if ((port_name_match != NULL) &&
		    (strstr(cur->port_name, port_name_match) == NULL)) {
			cur = cur->next;
			continue;
		}

		/* if another port list is given to select from, check it for a match */
		check = selection_list;
		port_match_found = 0;
		while (check != NULL) {
			if ((cur->client == check->client) && (cur->port == check->port)) {
				port_match_found = 1;
			}
			check = check->next;
		}
		if ((selection_list != NULL) && !port_match_found) {
			cur = cur->next;
			continue;
		}

		/* if alsa_name match is given, ignore port names that don't match */
		if ((alsa_name_match != NULL) &&
		    (strstr(cur->alsa_name, alsa_name_match) == NULL)) {
			cur = cur->next;
			continue;
		}
		/* ?????: ignore other phasex clients */
		if ((strstr(cur->client_name, "PHASEX") != NULL) ||
		    (strstr(cur->client_name, "phasex") != NULL)) {
			cur = cur->next;
			continue;
		}

		/* check for valid client and type match */
		if ((cur->client >= 0) &&
		    (cur->client != SND_SEQ_ADDRESS_SUBSCRIBERS) &&
		    ((cur->type & type) == type)) {

			alsa_seq_subscribe_port(seq_info, cur, port_str_list);
		}

		cur = cur->next;
	}
}


/*****************************************************************************
 * alsa_seq_watchdog_cycle()
 *****************************************************************************/
void
alsa_seq_watchdog_cycle(void)
{
	ALSA_SEQ_PORT   *cur;
	ALSA_SEQ_PORT   *old_capture_ports;
	ALSA_SEQ_PORT   *new_capture_ports;

	if ((midi_driver == MIDI_DRIVER_ALSA_SEQ) && (alsa_seq_info != NULL)) {
		old_capture_ports = alsa_seq_info->capture_ports;
		cur = alsa_seq_info->capture_ports;
		while (cur != NULL) {
			if (cur->subscribe_request) {
				alsa_seq_subscribe_port(alsa_seq_info, cur, NULL);
			}
			else if (cur->unsubscribe_request) {
				alsa_seq_unsubscribe_port(alsa_seq_info, cur);
			}
			cur = cur->next;
		}
		new_capture_ports =
			alsa_seq_get_port_list(alsa_seq_info,
			                       (SND_SEQ_PORT_CAP_READ |
			                        SND_SEQ_PORT_CAP_SUBS_READ),
			                       NULL);
		if (alsa_seq_port_list_compare(old_capture_ports, new_capture_ports) == 0) {
			alsa_seq_port_free(new_capture_ports);
		}
		else {
			alsa_seq_info->capture_ports = new_capture_ports;
			alsa_seq_port_free(old_capture_ports);
			alsa_seq_ports_changed = 1;
		}
	}
}


/*****************************************************************************
 * open_alsa_seq_in()
 *
 * Creates MIDI input port and connects specified MIDI output ports to it.
 *****************************************************************************/
ALSA_SEQ_INFO *
open_alsa_seq_in(char *alsa_port)
{
	char                    port_str_list[128]  = "\0";
	char                    client_name[32];
	char                    port_name[32];
	ALSA_SEQ_INFO           *new_seq_info;
	ALSA_SEQ_PORT           *cur                = NULL;
	ALSA_SEQ_PORT           *prev               = NULL;
	snd_seq_client_info_t   *cinfo;
	snd_seq_port_info_t     *pinfo;
	char                    *o;
	char                    *p;
	char                    *q;
	char                    *tokbuf;
	int                     src_client          = 0;
	int                     src_port            = 0;

	/* allocate our MIDI structure for returning everything */
	if ((new_seq_info = malloc(sizeof(ALSA_SEQ_INFO))) == NULL) {
		phasex_shutdown("Out of memory!\n");
	}
	memset(new_seq_info, 0, sizeof(ALSA_SEQ_INFO));
	if ((new_seq_info->in_port = malloc(sizeof(ALSA_SEQ_PORT))) == NULL) {
		phasex_shutdown("Out of memory!\n");
	}
	memset(new_seq_info->in_port, 0, sizeof(ALSA_SEQ_PORT));
	new_seq_info->in_port->next = NULL;
	new_seq_info->in_port->subs = NULL;
	new_seq_info->src_ports     = NULL;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);

	/* TODO: need a better handler here */
	snd_lib_error_set_handler(alsa_error_handler);

	/* open the sequencer */
	if (snd_seq_open(&new_seq_info->seq, "default",
	                 SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) < 0) {
		PHASEX_ERROR("Unable to open ALSA sequencer.\n");
		return NULL;
	}

	/* get a comma separated client:port list from command line, */
	/*  or a '-' (or no -p arg) for open subscription. */
	if (alsa_port != NULL) {
		if ((tokbuf = alloca(strlen((const char *) alsa_port) * 4)) == NULL) {
			phasex_shutdown("Out of memory!\n");
		}
		o = alsa_port;
		while ((p = strtok_r(o, ",", &tokbuf)) != NULL) {
			o = NULL;
			prev = cur;
			if (*p == '-') {
				continue;
			}
			else if (isdigit(*p)) {
				if ((q = index(p, ':')) == NULL) {
					PHASEX_ERROR("Invalid ALSA MIDI client port '%s'.\n",
					             alsa_port);
					continue;
				}
				src_client = atoi(p);
				src_port   = atoi(q + 1);
			}
			else if (strcmp(p, "autohw") == 0) {
				new_seq_info->auto_hw = 1;
				continue;
			}
			else if (strcmp(p, "autosw") == 0) {
				new_seq_info->auto_sw = 1;
				continue;
			}
			else {
				PHASEX_ERROR("Invalid ALSA Sequencer client port '%s'.\n",
				             alsa_port);
				continue;
			}
			if ((cur = malloc(sizeof(ALSA_SEQ_PORT))) == NULL) {
				phasex_shutdown("Out of memory!\n");
			}
			snd_seq_get_any_client_info(new_seq_info->seq, src_client, cinfo);
			snd_seq_get_any_port_info(new_seq_info->seq, src_client, src_port, pinfo);
			cur->client      = src_client;
			cur->port        = src_port;
			cur->type        = SND_SEQ_PORT_TYPE_MIDI_GENERIC;
			cur->next        = NULL;
			cur->subs        = NULL;
			cur->client_name = strdup(snd_seq_client_info_get_name(cinfo));
			cur->port_name   = strdup(snd_seq_port_info_get_name(pinfo));
			snprintf(cur->alsa_name, sizeof(cur->alsa_name), "%d:%d",
			         cur->client, cur->port);
			if (prev == NULL) {
				new_seq_info->src_ports = cur;
			}
			else {
				prev->next = cur;
			}
		}
	}

	/* extract our client id */
	new_seq_info->in_port->client = snd_seq_client_id(new_seq_info->seq);

	/* set our client name */
	snprintf(client_name, sizeof(client_name), "phasex");
	if (snd_seq_set_client_name(new_seq_info->seq, client_name) < 0) {
		PHASEX_ERROR("Unable to set ALSA sequencer client name.\n");
		alsa_seq_cleanup(NULL);
		return NULL;
	}

	/* create a port */
	snprintf(port_name, sizeof(port_name), "phasex in");
	new_seq_info->in_port->port =
		snd_seq_create_simple_port(new_seq_info->seq, port_name,
		                           SND_SEQ_PORT_CAP_WRITE |
		                           SND_SEQ_PORT_CAP_SUBS_WRITE,
#ifdef SND_SEQ_PORT_TYPE_SOFTWARE
		                           SND_SEQ_PORT_TYPE_SOFTWARE |
#endif
#ifdef SND_SEQ_PORT_TYPE_SYNTHESIZER
		                           SND_SEQ_PORT_TYPE_SYNTHESIZER |
#endif
#ifdef SND_SEQ_PORT_TYPE_APPLICATION
		                           SND_SEQ_PORT_TYPE_APPLICATION |
#endif
		                           SND_SEQ_PORT_TYPE_MIDI_GENERIC);

	/* since we opened nonblocking, we need our poll descriptors */
	if ((new_seq_info->npfds = snd_seq_poll_descriptors_count
	     (new_seq_info->seq, POLLIN)) > 0) {
		if ((new_seq_info->pfd = malloc((unsigned int)(new_seq_info->npfds) *
		                                sizeof(struct pollfd))) == NULL) {
			phasex_shutdown("Out of memory!\n");
		}
		if (snd_seq_poll_descriptors(new_seq_info->seq, new_seq_info->pfd,
		                             (unsigned int) new_seq_info->npfds, POLLIN) <= 0) {
			PHASEX_ERROR("No ALSA sequencer descriptors to poll.\n");
			alsa_seq_cleanup(NULL);
			return NULL;
		}
	}

	/* get list of all capture ports  */
	if ((new_seq_info->capture_ports =
	     alsa_seq_get_port_list(new_seq_info,
	                            (SND_SEQ_PORT_CAP_READ |
	                             SND_SEQ_PORT_CAP_SUBS_READ),
	                            new_seq_info->capture_ports)) == NULL) {
		PHASEX_DEBUG(DEBUG_CLASS_MIDI, "Unable to get ALSA sequencer port list.\n");
	}
	PHASEX_DEBUG(DEBUG_CLASS_MIDI, "\n");
	if (debug & (DEBUG_CLASS_INIT | DEBUG_CLASS_MIDI)) {
		cur = new_seq_info->capture_ports;
		if (cur != NULL) {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI, "Found ALSA sequencer capture ports:\n");
		}
		while (cur != NULL) {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI,
			             "  [%s] (type %d) %s: %s\n",
			             cur->alsa_name,
			             cur->type,
			             cur->client_name,
			             cur->port_name);
			cur = cur->next;
		}
		PHASEX_DEBUG(DEBUG_CLASS_MIDI, "\n");
	}

	/* get list of playback ports */
	/* leave disabled until MIDI output is needed. */
	/* if ((new_seq_info->playback_ports = */
	/*      alsa_seq_get_port_list (new_seq_info, */
	/*                           (SND_SEQ_PORT_CAP_WRITE | */
	/*                            SND_SEQ_PORT_CAP_SUBS_WRITE) */
	/*                           new_seq_info->playback_ports)) */
	/*     == NULL) { */
	/*      PHASEX_DEBUG (DEBUG_CLASS_MIDI, */
	/*                    "Unable to get ALSA sequencer port list.\n"); */
	/* } */
	/* if (debug & (DEBUG_CLASS_INIT | DEBUG_CLASS_MIDI)) { */
	/*      cur = new_seq_info->playback_ports; */
	/*      if (cur != NULL) { */
	/*              PHASEX_DEBUG (DEBUG_CLASS_MIDI, */
	/*                            "Found ALSA sequencer playback ports:\n"); */
	/*      } */
	/*      while (cur != NULL) { */
	/*              PHASEX_DEBUG (DEBUG_CLASS_MIDI, */
	/*                            "  [%s] (type %d) %s: %s\n", */
	/*                            cur->alsa_name, */
	/*                            cur->type, */
	/*                            cur->client_name, */
	/*                            cur->port_name); */
	/*              cur = cur->next; */
	/*      } */
	/*      PHASEX_DEBUG (DEBUG_CLASS_MIDI, "\n"); */
	/* } */

	/* subscribe to ports if any capture ports are available */
	if (new_seq_info->capture_ports != NULL) {
		/* subscribe to hardware ports. */
		if (new_seq_info->auto_hw) {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI,
			             "Auto-subscribing to ALSA sequencer hardware ports:\n");
			alsa_seq_subscribe_ports(new_seq_info,
			                         new_seq_info->capture_ports,
			                         SND_SEQ_PORT_TYPE_HARDWARE,
			                         NULL, NULL, NULL);
			strcat(port_str_list, "autohw,");
			PHASEX_DEBUG(DEBUG_CLASS_MIDI, "\n");
		}
		/* subscribe to software (sequencer) ports w/ port name matching "phasex" */
		if (new_seq_info->auto_sw) {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI,
			             "Auto-subscribing to ALSA sequencer software ports:\n");
			alsa_seq_subscribe_ports(new_seq_info,
			                         new_seq_info->capture_ports,
			                         SND_SEQ_PORT_TYPE_SOFTWARE,
			                         NULL, "phasex", NULL);
			strcat(port_str_list, "autosw,");
			PHASEX_DEBUG(DEBUG_CLASS_MIDI, "\n");
		}
		/* subscribe to specified ports */
		if (new_seq_info->src_ports != NULL) {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI,
			             "Subscribing to user specified ALSA sequencer ports:\n");
			alsa_seq_subscribe_ports(new_seq_info,
			                         new_seq_info->capture_ports,
			                         0,
			                         port_str_list, NULL, new_seq_info->src_ports);
			PHASEX_DEBUG(DEBUG_CLASS_MIDI, "\n");
		}
	}

	/* ALSA sequencer interface is up and running. */
	/* keep settings updated. */
	if (port_str_list[0] != '\0') {
		* (rindex(port_str_list, ',')) = '\0';
		if (setting_alsa_seq_port != NULL) {
			free(setting_alsa_seq_port);
		}
		setting_alsa_seq_port = strdup(port_str_list);
		config_changed = 1;
	}
	if (setting_midi_driver != MIDI_DRIVER_ALSA_SEQ) {
		setting_midi_driver = MIDI_DRIVER_ALSA_SEQ;
		config_changed = 1;
	}

	/* we're done, so hand over the goods */
	return new_seq_info;
}


/*****************************************************************************
 * alsa_seq_init()
 *****************************************************************************/
int
alsa_seq_init(void)
{
	char    *port_name = setting_alsa_seq_port;

	PHASEX_DEBUG(DEBUG_CLASS_INIT, "alsa_seq_init() port_name='%s'\n",
	             port_name);

	/* open ALSA sequencer MIDI input (OK of port_name is NULL). */
	if ((alsa_seq_info = open_alsa_seq_in(port_name)) == NULL) {
		return -1;
	}

#ifndef WITHOUT_LASH
	if (!lash_disabled) {
		lash_client_set_alsa_id(alsa_seq_info->seq);
	}
#endif

	return 0;
}


/*****************************************************************************
 * alsa_seq_cleanup()
 *
 * Cleanup handler for MIDI thread.
 * Closes MIDI ports.
 *****************************************************************************/
void
alsa_seq_cleanup(void *UNUSED(arg))
{
	ALSA_SEQ_PORT   *cur;
	ALSA_SEQ_PORT   *prev;

	/* disconnect from list of specified source ports, if any */
	if (alsa_seq_info != NULL) {
		cur = alsa_seq_info->src_ports;
		while (cur != NULL) {
			if (cur->subs != NULL) {
				snd_seq_unsubscribe_port(alsa_seq_info->seq, cur->subs);
				snd_seq_port_subscribe_free(cur->subs);
			}
			if ((cur->client >= 0) && (cur->client != SND_SEQ_ADDRESS_SUBSCRIBERS)) {
				snd_seq_disconnect_from(alsa_seq_info->seq, 0, cur->client, cur->port);
			}
			if (cur->client_name != NULL) {
				free(cur->client_name);
			}
			if (cur->port_name != NULL) {
				free(cur->port_name);
			}
			prev = cur;
			cur = cur->next;
			free(prev);
		}
		cur = alsa_seq_info->in_port;
		while (cur != NULL) {
			if (cur->client_name != NULL) {
				free(cur->client_name);
			}
			if (cur->port_name != NULL) {
				free(cur->port_name);
			}
			prev = cur;
			cur = cur->next;
			free(prev);
		}

		/* close sequencer */
		if (alsa_seq_info->seq != NULL) {
			snd_seq_close(alsa_seq_info->seq);
		}
		snd_config_update_free_global();
		if (alsa_seq_info->pfd != NULL) {
			free(alsa_seq_info->pfd);
		}
		free(alsa_seq_info);
		alsa_seq_info = NULL;
	}

	/* Add some guard time, in case MIDI hardware is re-initialized soon. */
	usleep(125000);

	midi_thread_p = 0;
}


/*****************************************************************************
 * midi_thread()
 *
 * MIDI input thread function.
 * Modifies patch, part, voice, and global parameters.
 *****************************************************************************/
void *
alsa_seq_thread(void *UNUSED(arg))
{
	PART                *part;
	MIDI_EVENT          *event      = & (alsa_seq_info->event);
	timecalc_t          delta_nsec;
	struct timespec     now;
	struct sched_param  schedparam;
	pthread_t           thread_id;
	snd_seq_event_t     *ev         = NULL;
	unsigned int        part_num;
	unsigned int        cycle_frame = 0;
	unsigned int        index;

	/* clear outgoing event buffer */
	memset(event, 0, sizeof(MIDI_EVENT));

	/* set realtime scheduling and priority */
	thread_id = pthread_self();
	memset(&schedparam, 0, sizeof(struct sched_param));
	schedparam.sched_priority = setting_midi_priority;
	pthread_setschedparam(thread_id, setting_sched_policy, &schedparam);

	/* setup thread cleanup handler */
	pthread_cleanup_push(&alsa_seq_cleanup, NULL);

	/* flush MIDI input */
	if (poll(alsa_seq_info->pfd, (nfds_t) alsa_seq_info->npfds, 0) > 0) {
		while ((snd_seq_event_input(alsa_seq_info->seq, &ev) >= 0) && (ev != NULL));
	}

	/* broadcast the midi ready condition */
	pthread_mutex_lock(&midi_ready_mutex);
	midi_ready = 1;
	pthread_cond_broadcast(&midi_ready_cond);
	pthread_mutex_unlock(&midi_ready_mutex);

	/* MAIN LOOP: poll for midi input and process events */
	while (!midi_stopped && !pending_shutdown) {

		/* set thread cancelation point */
		pthread_testcancel();

		/* poll for new MIDI input */
		if (poll(alsa_seq_info->pfd, (nfds_t) alsa_seq_info->npfds, 100) > 0) {

			/* cycle through all available events */
			while ((snd_seq_event_input(alsa_seq_info->seq, &ev) >= 0) &&
			       ev != NULL) {

				/* get timestamp and determine index and frame position of
				   this event. */
				delta_nsec  = get_time_delta(&now);
				cycle_frame = get_midi_cycle_frame(delta_nsec);
				index = get_midi_index();
				PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
				             DEBUG_COLOR_CYAN "[%d] " DEBUG_COLOR_DEFAULT,
				             (index / buffer_period_size));

				event->type  = MIDI_EVENT_NO_EVENT;
				event->state = EVENT_STATE_ALLOCATED;

				/* process event for each part that wants it */
				for (part_num = 0; part_num < MAX_PARTS; part_num++) {
					part = get_part(part_num);

					/* make sure this part wants this channel */
					if ((ev->data.note.channel == part->midi_channel) ||
					    (part->midi_channel == 16)) {

						event->channel = ev->data.note.channel;

						switch (ev->type) {

						case SND_SEQ_EVENT_NOTEON:
							event->type           = MIDI_EVENT_NOTE_ON;
							event->note           = ev->data.note.note & 0x7F;
							event->velocity       = ev->data.note.velocity & 0x7F;
							break;
						case SND_SEQ_EVENT_NOTEOFF:
							event->type           = MIDI_EVENT_NOTE_OFF;
							event->note           = ev->data.note.note & 0x7F;
							event->velocity       = ev->data.note.velocity & 0x7F;
							break;
						case SND_SEQ_EVENT_KEYPRESS:
							event->type           = MIDI_EVENT_AFTERTOUCH;
							event->note           = ev->data.note.note & 0x7F;
							event->velocity       = ev->data.note.velocity & 0x7F;
							break;
						case SND_SEQ_EVENT_PGMCHANGE:
							event->type           = MIDI_EVENT_PROGRAM_CHANGE;
							event->program        = ev->data.control.value & 0x7F;
							break;
						case SND_SEQ_EVENT_CHANPRESS:
							event->type           = MIDI_EVENT_POLYPRESSURE;
							event->polypressure   = ev->data.control.value & 0x7F;
							break;
						case SND_SEQ_EVENT_CONTROLLER:
							event->type           = MIDI_EVENT_CONTROLLER;
							event->controller     = ev->data.control.param & 0x7F;
							event->value          = ev->data.control.value & 0x7F;
							break;
						case SND_SEQ_EVENT_PITCHBEND:
							event->type           = MIDI_EVENT_PITCHBEND;
							event->lsb            = (ev->data.control.value + 8192) & 0x7F;
							event->msb            = ((ev->data.control.value + 8192) >> 7) & 0x7F;
							break;
						case SND_SEQ_EVENT_SENSING:
							set_active_sensing_timeout();
							event->type = MIDI_EVENT_NO_EVENT;
							break;
						case SND_SEQ_EVENT_STOP:
							queue_midi_realtime_event(ALL_PARTS, MIDI_EVENT_STOP,
							                          cycle_frame, index);
							event->type = MIDI_EVENT_NO_EVENT;
							break;
#ifdef MIDI_CLOCK_SYNC
						case SND_SEQ_EVENT_CLOCK:
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
							             "-- Clock msg: %d %d\n",
							             ev->data.queue.param.d32[0],
							             ev->data.queue.param.d32[1]);
							break;
						case SND_SEQ_EVENT_SONGPOS:
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
							             "-- Song Position: %d %d\n",
							             ev->data.control.param,
							             ev->data.control.value);
							break;
						case SND_SEQ_EVENT_START:
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
							             "-- Start msg: %d %d\n",
							             ev->data.queue.param.d32[0],
							             ev->data.queue.param.d32[1]);
							break;
						case SND_SEQ_EVENT_CONTINUE:
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
							             "-- Continue msg: %d %d\n",
							             ev->data.queue.param.d32[0],
							             ev->data.queue.param.d32[1]);
							break;
						case SND_SEQ_EVENT_SETPOS_TICK:
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
							             "-- SetPos Tick msg: %d %d\n",
							             ev->data.queue.param.d32[0],
							             ev->data.queue.param.d32[1]);
							break;
						case SND_SEQ_EVENT_TICK:
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
							             "-- Tick msg: %d %d\n",
							             ev->data.queue.param.d32[0],
							             ev->data.queue.param.d32[1]);
							break;
#endif
						default:
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
							             "*** WARNING:  Unhandled ALSA Seq event!  type=%d  ***\n",
							             ev->type);
							break;
						}

						/* queue event for engine thread */
						if (event->type != MIDI_EVENT_NO_EVENT) {
							queue_midi_event(part_num, event, cycle_frame, index);
						}
					}
				}

				snd_seq_free_event(ev);
				ev = NULL;
			}
		}
	}

	/* execute cleanup handler and remove it */
	pthread_cleanup_pop(1);

	/* end of MIDI thread */
	pthread_exit(NULL);
	return NULL;
}
