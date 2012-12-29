/*****************************************************************************
 *
 * lash.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2010 Anton Kormakov <assault64@gmail.com>
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
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <lash/lash.h>
#include <jack/jack.h>
#include <asoundlib.h>
#include "phasex.h"
#include "patch.h"
#include "param.h"
#include "bank.h"
#include "session.h"
#include "midimap.h"
#include "driver.h"
#include "settings.h"
#include "debug.h"
#include "lash.h"


lash_client_t   *lash_client;
char            *lash_buffer;

char            *lash_jackname;
snd_seq_t       *lash_alsa_id;

char            *lash_project_dir;
char            *lash_project_name;


/*****************************************************************************
 * lash_client_init()
 *****************************************************************************/
int
lash_client_init(int *argc, char ***argv)
{
	lash_event_t    *event;
	char            *lash_name;

	if ((lash_name = malloc(64 * sizeof(char))) == NULL) {
		phasex_shutdown("Out of Memory!\n");
	}

	snprintf(lash_name, (64 * sizeof(char)), "phasex");
	lash_client = lash_init(lash_extract_args(argc, argv),
	                        lash_name, LASH_Config_File, LASH_PROTOCOL(2, 0));
	if (lash_enabled(lash_client)) {

		event = lash_event_new_with_type(LASH_Client_Name);
		lash_event_set_string(event, lash_name);
		lash_send_event(lash_client, event);

		PHASEX_DEBUG((DEBUG_CLASS_INIT | DEBUG_CLASS_SESSION), "LASH client initialized.\n");
		fprintf(stderr, "LASH client initialized.  (LASH_Client_Name='%s').\n", lash_name);
		return 0;
	}
	if (debug_thread_p) {
		PHASEX_DEBUG((DEBUG_CLASS_INIT | DEBUG_CLASS_SESSION),
		             "LASH client initialization failed.\n");
	}
	else {
		fprintf(stderr, "LASH client initialization failed.\n");
	}
	return -1;
}


/*****************************************************************************
 * lash_client_set_jack_name()
 *****************************************************************************/
void
lash_client_set_jack_name(jack_client_t *client)
{
	lash_jackname = jack_get_client_name(client);
	if ((audio_driver == AUDIO_DRIVER_JACK) && (lash_enabled(lash_client))) {
		lash_jack_client_name(lash_client, lash_jackname);
	}
}


/*****************************************************************************
 * lash_client_set_alsa_id()
 *****************************************************************************/
void
lash_client_set_alsa_id(snd_seq_t *seq)
{
	if ((midi_driver < MIDI_DRIVER_JACK) && (lash_enabled(lash_client))) {
		lash_alsa_id = seq;
		lash_alsa_client_id(lash_client, (unsigned char) snd_seq_client_id(seq));
	}
}


/*****************************************************************************
 * lash_set_phasex_session_name()
 *****************************************************************************/
char *
lash_set_phasex_session_name(char *lash_dir)
{
	SESSION *session = get_current_session();
	char    *dir;
	char    *p;
	char    *q;
	int     slashes;

	if (lash_dir == NULL) {
		if (lash_project_name != NULL) {
			p = session->name;
			session->name = strdup(lash_project_name);
			session_name_changed = 1;
			free(p);
		}
	}
	else {

		dir = strdup(lash_dir);

		p = dir + strlen(dir) - 1;
		slashes = 2;
		while (slashes > 0) {
			if ((*p == '/') && (* (p + 1) != '.') &&
			    (* (p + 2) != 'i') && (* (p + 3) != 'd')) {
				slashes--;
			}
			p--;
			if (p < dir) {
				break;
			}
		}
		p++;
		if (slashes == 0) {
			q = ++p;
			while (*p != '/') {
				p++;
			}
			*p = '\0';
			if (lash_project_name == NULL) {
				lash_project_name = strdup(q);
			}
			else {
				p = lash_project_name;
				lash_project_name = strdup(q);
				free(p);
			}
			if (session->name == NULL) {
				session->name = strdup(q);
			}
			else {
				p = session->name;
				session->name = strdup(q);
				free(p);
			}
			session_name_changed = 1;
		}
		free(dir);

		p = lash_project_dir;
		lash_project_dir = strdup(lash_buffer);
		if (p != NULL) {
			free(p);
		}
	}

	return session->name;
}


/*****************************************************************************
 * lash_poll_event()
 *****************************************************************************/
int
lash_poll_event(void)
{
	lash_event_t    *event;

	if (lash_disabled || !lash_enabled(lash_client)) {
		return -1;
	}

	while ((event = lash_get_event(lash_client)) != NULL) {

		switch (lash_event_get_type(event)) {

		case LASH_Save_File:
			lash_buffer = (char *) lash_event_get_string(event);
			PHASEX_DEBUG(DEBUG_CLASS_SESSION, "lash_poll_event():  LASH_Save_File  dir='%s'\n",
			             lash_buffer);
			lash_set_phasex_session_name(lash_buffer);
			save_session(lash_buffer, visible_sess_num, 1);
			lash_send_event(lash_client, lash_event_new_with_type(LASH_Save_File));
			break;

		case LASH_Restore_File:
			lash_buffer = (char *) lash_event_get_string(event);
			PHASEX_DEBUG(DEBUG_CLASS_SESSION, "lash_poll_event():  LASH_Restore_File  dir='%s'\n",
			             lash_buffer);
			load_session(lash_buffer, visible_sess_num, 1);
			lash_set_phasex_session_name(lash_buffer);
			lash_send_event(lash_client, lash_event_new_with_type(LASH_Restore_File));
			break;

			/* TODO: support the complete LASH spec. */
		case LASH_Client_Name:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Client_Name'\n");
			break;
		case LASH_Jack_Client_Name:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Jack_Client_Name'\n");
			break;
		case LASH_Alsa_Client_ID:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Alsa_Client_ID'\n");
			break;
		case LASH_Save_Data_Set:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Save_Data_Set'\n");
			break;
		case LASH_Restore_Data_Set:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Restore_Data_Set'\n");
			break;
		case LASH_Save:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Save'\n");
			break;
		case LASH_Server_Lost:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Server_Lost'\n");
			break;
		case LASH_Project_Add:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Project_Add'\n");
			break;
		case LASH_Project_Remove:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Project_Remove'\n");
			break;
		case LASH_Project_Dir:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Project_Dir'\n");
			break;
		case LASH_Project_Name:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Project_Name'\n");
			break;
		case LASH_Client_Add:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Client_Add'\n");
			break;
		case LASH_Client_Remove:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Client_Remove'\n");
			break;
		case LASH_Percentage:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Percentage'\n");
			break;
		case LASH_Quit:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH LASH_Quit!  Shutting down.\n");
			pending_shutdown = 1;
			break;
			//              case LASH_Event_Unknown:
		default:
			PHASEX_DEBUG(DEBUG_CLASS_SESSION,
			             "LASH received unhandled event 'LASH_Event_Unknown'\n");
			break;
		}

		lash_event_destroy(event);
	}
	return 0;
}
