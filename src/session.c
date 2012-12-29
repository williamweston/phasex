/*****************************************************************************
 *
 * session.c
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
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include "phasex.h"
#include "config.h"
#include "patch.h"
#include "param.h"
#include "bank.h"
#include "session.h"
#include "settings.h"
#include "engine.h"
#include "midimap.h"
#include "gui_main.h"
#include "gui_patch.h"
#include "string_util.h"
#include "debug.h"


DIR_LIST    *session_dir_list       = NULL;

SESSION     session_bank[SESSION_BANK_SIZE];

int         current_session         = 0;

int         session_name_changed    = 0;


/*****************************************************************************
 * get_current_session()
 *****************************************************************************/
SESSION *
get_current_session(void)
{
	return & (session_bank[visible_sess_num]);
}


/*****************************************************************************
 * get_session()
 *****************************************************************************/
SESSION *
get_session(unsigned int sess_num)
{
	if (sess_num >= SESSION_BANK_SIZE) {
		return & (session_bank[visible_sess_num]);
	}
	return & (session_bank[sess_num]);
}


/*****************************************************************************
 * set_patch_from_session_bank()
 *****************************************************************************/
PATCH *
set_patch_from_session_bank(unsigned int sess_num, unsigned int part_num)
{
	if (sess_num >= SESSION_BANK_SIZE) {
		sess_num = 0;
	}

	active_patch[part_num] = & (session_bank[sess_num].patch[part_num]);
	active_state[part_num] = & (session_bank[sess_num].state[part_num]);

	return active_patch[part_num];
}


/*****************************************************************************
 * init_session_patch_bank()
 *****************************************************************************/
void
init_session_bank(char *filename)
{
	SESSION         *session;
	PATCH           *patch;
	unsigned int    part_num;
	unsigned int    sess_num;

	/* initialize session_bank memory */
	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		visible_prog_num[part_num]       = 0;
		get_part(part_num)->midi_channel = (int) part_num;
		for (sess_num = 0; sess_num < SESSION_BANK_SIZE; sess_num++) {
			session                     = get_session(sess_num);
			session->prog_num[part_num] = 0;
			session->name               = NULL;
			session->directory          = NULL;
			session->parent_dir         = NULL;
			patch                       = get_patch_from_session_bank(sess_num, part_num);
			patch->name                 = NULL;
			patch->filename             = NULL;
			patch->directory            = NULL;
			init_patch_data_structures(patch, sess_num, part_num, 0);
			patch->param[PARAM_MIDI_CHANNEL].value.cc_prev = (int) part_num;
			patch->param[PARAM_MIDI_CHANNEL].value.cc_val  = (int) part_num;
			patch->param[PARAM_MIDI_CHANNEL].value.int_val = (int) part_num +
				patch->param[PARAM_MIDI_CHANNEL].info->cc_offset;
		}
		patch = set_active_patch(0, part_num, 0);
		init_patch_state(patch);
	}

	/* load the session_bank for all parts */
	load_session_bank(filename);
}


/*****************************************************************************
 * load_session()
 *****************************************************************************/
int
load_session(char *directory, unsigned int sess_num, int managed)
{
	SESSION         *session        = get_session(sess_num);
	PATCH           *patch;
	PATCH           *active_patch;
	DIR_LIST        *pdir           = session_dir_list;
	DIR_LIST        *ldir           = NULL;
	char            *tmpname;
	char            filename[PATH_MAX];
	unsigned int    part_num;
	int             return_code     = 1;
	int             dir_found       = 0;

	if (session->directory != NULL) {
		free(session->directory);
	}

	if (!managed) {
		session->directory = strdup(directory);
		tmpname = session->name;
		session->name = get_session_name_from_directory(directory);
		if (tmpname != NULL) {
			free(tmpname);
		}
	}

	/* read session patches */
	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		session->prog_num[part_num] = 0;
		snprintf(filename, sizeof(filename), "%s/phasex-%02d.phx", directory, (part_num + 1));
		patch = get_patch_from_session_bank(sess_num, part_num);
		if (read_patch(filename, patch) == 0) {
			if ((patch->name == NULL) && (session->name != NULL)) {
				snprintf(filename, PATCH_NAME_LEN, "%s-%02d", session->name, (part_num + 1));
				patch->name = strdup(filename);
			}
			/* ????? Should this be handled by caller in gui code? */
			if ((sess_num == visible_sess_num)) {
				visible_prog_num[part_num] = 0;
				active_patch = set_active_patch(sess_num, part_num, 0);
				if (patch != active_patch) {
					PHASEX_ERROR("patch != active_patch !!!!!!!!!!!!!!!\n");
				}
				init_patch_state(patch);
				if (gtkui_ready && (part_num == visible_part_num)) {
					update_gui_patch(patch, 0);
				}
			}
			return_code = 0;
		}
	}

	if (!managed) {
		if (return_code == 0) {
			tmpname = session->name;
			session->name = strdup(basename(directory));
			if (tmpname != NULL) {
				free(tmpname);
			}
			save_session_bank(NULL);
		}

		if (session->name == NULL) {
			sprintf(filename, "Untitled-%04d", sess_num);
			session->name = strdup(filename);
		}
		update_gui_session_name();

		/* keep track of session container directory list */
		strncpy(filename, directory, PATH_MAX);
		directory = dirname(filename);
		session->parent_dir = strdup(directory);
		if ((session->directory != NULL) &&
		    strcmp(session->directory, user_session_dump_dir) != 0) {
			if ((directory != NULL) && (* (directory) != '\0') &&
			    (strcmp(directory, user_session_dir) != 0)) {
				while ((pdir != NULL) && (!dir_found)) {
					if (strcmp(pdir->name, directory) == 0) {
						dir_found = 1;
					}
					else {
						ldir = pdir;
						pdir = pdir->next;
					}
				}
				if (!dir_found) {
					if ((pdir = malloc(sizeof(DIR_LIST))) == NULL) {
						phasex_shutdown("Out of Memory!\n");
					}
					pdir->name = strdup(directory);
					pdir->load_shortcut = pdir->save_shortcut = 0;
					if (ldir == NULL) {
						pdir->next = NULL;
						session_dir_list = pdir;
					}
					else {
						pdir->next = ldir->next;
						ldir->next = pdir;
					}
				}
			}
		}
	}

	session->modified = 0;

	return return_code;
}


/*****************************************************************************
 * load_session_bank()
 *****************************************************************************/
void
load_session_bank(char *filename)
{
	SESSION         *session;
	PATCH           *patch;
	FILE            *session_bank_f;
	char            *session_bank_file;
	char            *p;
	char            directory[PATH_MAX];
	char            *tmpname;
	char            buffer[256];
	char            *streambuf;
	unsigned int    part_num    = 0;
	unsigned int    sess_num    = 0;
	int             line        = 0;
	int             result;
	char            loaded[SESSION_BANK_SIZE];

	if (filename == NULL) {
		session_bank_file = user_session_bank_file;
	}
	else {
		session_bank_file = filename;
	}

	memset(loaded, 0, sizeof(loaded));

	/* Allocate a separate buffer for reading the bank file, due to the number
	   of patch files (up to PATCH_BANK_SIZE * MAX_PARTS) that are read while
	   session_bank_f is still open, confusing the buffers and limiting reads
	   on this file to 4096 bytes. */
	if ((streambuf = malloc(1024 * 1024)) == NULL) {
		phasex_shutdown("Out of Memory!\n");
	}

	/* open the session_bank file */
	if ((session_bank_f = fopen(session_bank_file, "r")) == NULL) {
		if ((session_bank_f = fopen(user_session_bank_file, "r")) == NULL) {
			return;
		}
	}
	setvbuf(session_bank_f, streambuf, _IOFBF, (1024 * 1024));

	/* read session_bank entries */
	while (fgets(buffer, sizeof(buffer), session_bank_f) != NULL) {
		line++;

		/* discard comments and blank lines */
		if ((buffer[0] == '\n') || (buffer[0] == '#')) {
			continue;
		}

		/* get session number */
		if ((p = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		result = atoi(p) - 1;
		if ((result < 0) || (result >= SESSION_BANK_SIZE)) {
			sess_num = 0;
		}
		else {
			sess_num = (unsigned int) result;
		}
		session = get_session(sess_num);

		/* make sure there's an '=' */
		if ((p = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		if (*p != '=') {
			while (get_next_token(buffer) != NULL);
			continue;
		}

		/* get session directory */
		if ((tmpname = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		if (*tmpname == ';') {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		strncpy(directory, tmpname, (sizeof(directory) - 1));
		directory[(sizeof(directory) - 1)] = '\0';

		/* make sure there's a ';' */
		if ((p = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		if (*p != ';') {
			while (get_next_token(buffer) != NULL);
			continue;
		}

		/* flush remainder of line */
		while (get_next_token(buffer) != NULL);

		/* load session patches into session_bank */
		result = load_session(directory, sess_num, 0);

		/* initialize on failure and set name based on session number */
		if (result == 0) {
			loaded[sess_num] = 1;
		}
		else {
			tmpname = session->name;
			snprintf(buffer, sizeof(buffer), "Session-%04d", (sess_num + 1));
			session->name = strdup(buffer);
			if (tmpname != NULL) {
				free(tmpname);
			}
			if (session->directory != NULL) {
				free(session->directory);
			}
			session->directory = NULL;
			if (session->parent_dir != NULL) {
				free(session->parent_dir);
			}
			session->parent_dir = NULL;
		}
	}

	/* done parsing */
	fclose(session_bank_f);
	free(streambuf);

	/* now fill the empty session_bank slots with the default session */
	for (sess_num = 0; sess_num < SESSION_BANK_SIZE; sess_num++) {
		session = get_session(sess_num);
		for (part_num = 0; part_num < MAX_PARTS; part_num++) {
			patch = get_patch_from_session_bank(sess_num, part_num);
			if (loaded[sess_num] == 0) {
				if (read_patch(user_default_patch, patch) != 0) {
					if (read_patch(sys_default_patch, patch) != 0) {
						PHASEX_WARN("Unable to load system default patch '%s'\n", sys_default_patch);
					}
				}
				snprintf(buffer, sizeof(buffer), "Untitled-%04d", (sess_num + 1));
				patch->name = strdup(buffer);
				if (patch->directory != NULL) {
					free(patch->directory);
				}
				patch->directory = strdup(user_patch_dir);
			}
			patch->modified = 0;
		}
		if (session->name == NULL) {
			snprintf(buffer, sizeof(buffer), "Untitled-%04d", (sess_num + 1));
			session->name = strdup(buffer);
			if (session->parent_dir != NULL) {
				free(session->parent_dir);
			}
			session->parent_dir = strdup(user_session_dir);
		}
		session->modified = 0;
	}
}


/*****************************************************************************
 * save_session()
 *****************************************************************************/
int
save_session(char *directory, unsigned int sess_num, int managed)
{
	SESSION         *session        = get_current_session();
	SESSION         *new_session    = get_session(sess_num);
	PATCH           *patch;
	DIR             *dir;
	char            *tmpdir;
	char            *tmpname;
	char            filename[PATH_MAX];
	unsigned int    part_num;
	int             return_code     = 0;
	int             dump            = 0;

	if (directory == NULL) {
		return -1;
	}

	if (directory == user_session_dump_dir) {
		dump = 1;
	}
	else if (!managed) {
		if (session->directory != NULL) {
			free(session->directory);
		}
		session->directory = strdup(directory);
	}

	if ((dir = opendir(directory)) == NULL) {
		if (errno == ENOENT) {
			if (mkdir(directory, 0755) != 0) {
				PHASEX_ERROR("Unable to create session directory '%s'.\n", directory);
				PHASEX_ERROR("Error %d: %s\n", errno, strerror(errno));
				closedir(dir);
				return -1;
			}
		}
	}
	closedir(dir);

	snprintf(filename, sizeof(filename), "%s/phasex.map", directory);
	save_midimap(filename);
	snprintf(filename, sizeof(filename), "%s/phasex.cfg", directory);
	save_settings(filename);
	snprintf(filename, sizeof(filename), "%s/%s", directory, USER_BANK_FILE);
	save_patch_bank(filename);
	snprintf(filename, sizeof(filename), "%s/%s", directory, USER_SESSION_BANK_FILE);
	save_session_bank(filename);

	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		patch = get_patch(sess_num, part_num, session_bank[sess_num].prog_num[part_num]);
		if (dump) {
			return_code |= save_patch(user_patchdump_file[part_num], patch);
		}
		else {
			snprintf(filename, sizeof(filename), "%s/phasex-%02d.phx", directory, (part_num + 1));
			return_code |= save_patch(filename, patch);
		}
	}

	if (!dump && (return_code == 0)) {
		session->modified = 0;
		if (!managed) {
			tmpname = session->name;
			session->name = get_session_name_from_directory(directory);
			if (tmpname != NULL) {
				free(tmpname);
			}
			if (session->parent_dir != NULL) {
				free(session->parent_dir);
			}
			strncpy(filename, session->directory, PATH_MAX);
			tmpdir = dirname(filename);
			session->parent_dir = strdup(tmpdir);
			if (sess_num == visible_sess_num) {
				update_gui_session_name();
			}
			else {
				new_session = get_session(sess_num);
				tmpname = new_session->name;
				new_session->name = strdup(session->name);
				if (tmpname != NULL) {
					free(tmpname);
				}
				if (new_session->directory != NULL) {
					free(new_session->directory);
				}
				new_session->directory = strdup(session->directory);
				if (new_session->parent_dir != NULL) {
					free(new_session->parent_dir);
				}
				new_session->parent_dir = strdup(session->parent_dir);
				for (part_num = 0; part_num < MAX_PARTS; part_num++) {
					if (new_session->patch[part_num].name != NULL) {
						free(new_session->patch[part_num].name);
					}
					if (new_session->patch[part_num].filename != NULL) {
						free(new_session->patch[part_num].filename);
					}
					if (new_session->patch[part_num].directory != NULL) {
						free(new_session->patch[part_num].directory);
					}
					patch = get_active_patch(part_num);
					bcopy(patch, & (new_session->patch[part_num]), sizeof(PATCH));
					if (patch->name != NULL) {
						new_session->patch[part_num].name      = strdup(patch->name);
					}
					if (patch->filename != NULL) {
						new_session->patch[part_num].filename  = strdup(patch->filename);
					}
					if (patch->directory != NULL) {
						new_session->patch[part_num].directory = strdup(patch->directory);
					}
					new_session->prog_num[part_num] = 0;
				}
				new_session->modified = 0;
			}
		}
	}

	return return_code;
}


/*****************************************************************************
 * save_session_bank()
 *****************************************************************************/
void
save_session_bank(char *filename)
{
	SESSION         *session;
	FILE            *session_bank_f;
	char            *session_bank_file;
	unsigned int    sess_num;

	if (filename == NULL) {
		session_bank_file = user_session_bank_file;
	}
	else {
		session_bank_file = filename;
	}

	/* open the session_bank file */
	if ((session_bank_f = fopen(session_bank_file, "wt")) == NULL) {
		PHASEX_ERROR("Error opening session bank file %s for write: %s\n",
		             session_bank_file, strerror(errno));
		return;
	}

	/* write the session_bank in the easy to read format */
	fprintf(session_bank_f, "# PHASEX User Session Bank\n");

	/* first session is always the session dump */
	fprintf(session_bank_f, "0001 = %s;\n", user_session_dump_dir);

	/* fill in the rest from the current in-memory bank */
	for (sess_num = 1; sess_num < SESSION_BANK_SIZE; sess_num++) {
		session = get_session(sess_num);
		if (session->directory != NULL) {
			fprintf(session_bank_f, "%04d = %s;\n", (sess_num + 1), session->directory);
		}
	}

	/* done saving */
	fclose(session_bank_f);
}


/*****************************************************************************
 * get_session_name_from_directory()
 *****************************************************************************/
char *
get_session_name_from_directory(char *directory)
{
	char    *f;
	char    *tmpname;
	char    *name = NULL;

	/* missing directory gets name of "Untitled" */
	if ((directory == NULL) || (strcmp(directory, user_session_dump_dir) == 0)) {
		name = strdup("Untitled");
	}
	else {
		/* strip off leading directory components */
		if ((f = rindex(directory, '/')) == NULL) {
			tmpname = directory;
		}
		else {
			tmpname = f + 1;
		}

		/* make a copy that can be modified and freed safely */
		if ((name = strdup(tmpname)) == NULL) {
			phasex_shutdown("Out of memory!\n");
		}
	}

	/* send name back to caller */
	return name;
}
