/*****************************************************************************
 *
 * bank.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2015 Willaim Weston <william.h.weston@gmail.com>
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
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include "phasex.h"
#include "config.h"
#include "patch.h"
#include "param.h"
#include "bank.h"
#include "session.h"
#include "settings.h"
#include "string_util.h"
#include "engine.h"
#include "gui_patch.h"
#include "debug.h"


PATCH           patch_bank[MAX_PARTS][PATCH_BANK_SIZE];
PATCH_STATE     state_bank[MAX_PARTS][PATCH_BANK_SIZE];

unsigned int    visible_sess_num        = 0;
unsigned int    visible_part_num        = 0;
unsigned int    visible_prog_num[MAX_PARTS];


/*****************************************************************************
 * get_patch_from_bank()
 *
 * Returns a pointer to the patch for the given part/prog num.
 *****************************************************************************/
PATCH *
get_patch_from_bank(unsigned int part_num, unsigned int prog_num)
{
	if (prog_num == 0) {
		return & (session_bank[visible_sess_num].patch[part_num]);
	}
	return & (patch_bank[part_num][prog_num]);
}


/*****************************************************************************
 * set_patch_from_bank()
 *
 * Sets the active patch pointer for the given part/prog num.
 *****************************************************************************/
PATCH *
set_patch_from_bank(unsigned int part_num, unsigned int prog_num)
{
	if (prog_num == 0) {
		active_patch[part_num] = & (session_bank[visible_sess_num].patch[part_num]);
		active_state[part_num] = & (session_bank[visible_sess_num].state[part_num]);
	}
	else {
		active_patch[part_num] = & (patch_bank[part_num][prog_num]);
		active_state[part_num] = & (state_bank[part_num][prog_num]);
	}

	return active_patch[part_num];
}


/*****************************************************************************
 * init_patch_bank()
 *****************************************************************************/
void
init_patch_bank(char *filename)
{
	PATCH           *patch;
	int             part_num;
	unsigned int    prog_num;

	/* initialize patchbank memory */
	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		visible_prog_num[part_num] = 0;
		get_part(part_num)->midi_channel = part_num;
		for (prog_num = 0; prog_num < PATCH_BANK_SIZE; prog_num++) {
			patch            = & (patch_bank[part_num][prog_num]);
			patch->name      = NULL;
			patch->filename  = NULL;
			patch->directory = NULL;
			init_patch_data_structures(patch, SESSION_BANK_SIZE, (unsigned int) part_num, prog_num);
			patch->param[PARAM_MIDI_CHANNEL].value.cc_prev = part_num;
			patch->param[PARAM_MIDI_CHANNEL].value.cc_val  = part_num;
			patch->param[PARAM_MIDI_CHANNEL].value.int_val = part_num +
				patch->param[PARAM_MIDI_CHANNEL].info->cc_offset;
		}
		patch = set_active_patch(0, (unsigned int) part_num, 0);
		read_patch(sys_default_patch, patch);
	}

	/* load the bank for all parts */
	load_patch_bank(filename);
}


/*****************************************************************************
 * load_patch_bank()
 *****************************************************************************/
void
load_patch_bank(char *filename)
{
	PATCH           *patch;
	FILE            *bank_f;
	char            *bank_file;
	char            *p;
	char            *patch_file;
	char            *tmpname;
	char            buffer[256];
	int             part_num    = 0;
	int             prog        = 0;
	unsigned int    line        = 0;
	static int      once        = 1;
	int             result;

	if (filename == NULL) {
		bank_file = user_bank_file;
	}
	else {
		bank_file = filename;
	}

	/* open the bank file */
	if ((bank_f = fopen(bank_file, "rt")) == NULL) {
		if ((bank_f = fopen(user_bank_file, "rt")) == NULL) {
			if ((bank_f = fopen(sys_bank_file, "rt")) == NULL) {
				return;
			}
		}
	}

	/* read bank entries */
	while (fgets(buffer, sizeof(buffer), bank_f) != NULL) {
		line++;

		/* discard comments and blank lines */
		if ((buffer[0] == '\n') || (buffer[0] == '#')) {
			continue;
		}

		/* get part number */
		if ((p = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		part_num = atoi(p) - 1;
		if ((part_num < 0) || (part_num >= MAX_PARTS)) {
			part_num = 0;
		}

		/* make sure there's a comma */
		if ((p = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		if (*p != ',') {
			while (get_next_token(buffer) != NULL);
			continue;
		}

		/* get program number */
		if ((p = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		prog = atoi(p) - 1;
		if ((prog < 0) || (prog >= PATCH_BANK_SIZE)) {
			prog = 0;
		}
		patch = & (patch_bank[part_num][prog]);

		/* make sure there's an '=' */
		if ((p = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		if (*p != '=') {
			while (get_next_token(buffer) != NULL);
			continue;
		}

		/* get patch name */
		if ((tmpname = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		if (*tmpname == ';') {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		if ((patch_file = strdup(tmpname)) == NULL) {
			phasex_shutdown("Out of memory!\n");
		}

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

		/* load patch into bank */
		result = 0;

		/* handle bare patch names from 0.10.x versions */
		if (patch_file[0] != '/') {
			snprintf(buffer, sizeof(buffer), "%s/%s.phx", user_patch_dir, patch_file);
			result = read_patch(buffer, patch);
			if (result != 0) {
				snprintf(buffer, sizeof(buffer), "%s/%s.phx", PATCH_DIR, patch_file);
				result = read_patch(buffer, patch);
			}
			if (result != 0) {
				PHASEX_WARN("Failed to load patch '%s''\n", buffer);
			}
		}

		/* handle fully qualified filenames */
		else {
			result = read_patch(patch_file, patch);
		}

		/* initialize on failure and set name based on program number */
		if (result != 0) {
			if (read_patch(user_default_patch, patch) != 0) {
				if (read_patch(sys_default_patch, patch) != 0) {
					PHASEX_WARN("Unable to load system default patch '%s'\n", sys_default_patch);
				}
			}
			snprintf(buffer, sizeof(buffer), "Untitled-%04d", (prog + 1));
			tmpname = patch->name;
			patch->name = strdup(buffer);
			if (tmpname != NULL) {
				free(tmpname);
			}
			if (patch->directory != NULL) {
				free(patch->directory);
			}
			patch->directory = strdup(user_patch_dir);
		}

		/* free up memory used to piece filename together */
		free(patch_file);

		/* Lock some global parameters after the first patch is read */
		if (once) {
			get_param_info_by_id(PARAM_MIDI_CHANNEL)->locked = 1;
			get_param_info_by_id(PARAM_BPM)->locked          = 1;

			once = 0;
		}
	}

	/* done parsing */
	fclose(bank_f);

	/* now fill the empty bank slots with the default patch */
	for (prog = 0; prog < PATCH_BANK_SIZE; prog++) {
		patch = & (patch_bank[part_num][prog]);
		if (patch->name == NULL) {
			if (read_patch(user_default_patch, patch) != 0) {
				read_patch(sys_default_patch, patch);
			}
			snprintf(buffer, sizeof(buffer), "Untitled-%04d", (prog + 1));
			patch->name = strdup(buffer);
			patch->directory = strdup(user_patch_dir);
		}
		patch->modified = 0;
	}
}


/*****************************************************************************
 * save_patch_bank()
 *****************************************************************************/
void
save_patch_bank(char *filename)
{
	PATCH           *patch;
	FILE            *bank_f;
	char            *bank_file;
	unsigned int    part_num;
	unsigned int    prog;

	if (filename == NULL) {
		bank_file = user_bank_file;
	}
	else {
		bank_file = filename;
	}

	/* open the bank file */
	if ((bank_f = fopen(bank_file, "wt")) == NULL) {
		PHASEX_ERROR("Error opening bank file %s for write: %s\n",
		             bank_file, strerror(errno));
		return;
	}

	/* write the bank in the easy to read format */
	fprintf(bank_f, "# PHASEX User Patch Bank\n");
	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		/* first program for each part is always from the session dump */
		fprintf(bank_f, "%d,0001 = %s;\n", (part_num + 1), user_patchdump_file[part_num]);
		/* fill in the rest from the in-memory bank */
		for (prog = 1; prog < PATCH_BANK_SIZE; prog++) {
			patch = get_patch_from_bank(part_num, prog);
			if (patch->filename != NULL) {
				fprintf(bank_f, "%d,%04d = %s;\n", (part_num + 1), (prog + 1), patch->filename);
			}
		}
	}

	/* done saving */
	fclose(bank_f);
}


/*****************************************************************************
 * find_patch()
 *****************************************************************************/
unsigned int
find_patch(char *name, unsigned int part_num)
{
	PATCH           *patch;
	unsigned int    prog;

	for (prog = 0; prog < PATCH_BANK_SIZE; prog++) {
		patch = get_patch_from_bank(part_num, prog);
		if (strcmp(name, patch->name) == 0) {
			break;
		}
	}
	return prog;
}


/*****************************************************************************
 * load_patch_list()
 *
 * Loads a comma separated list of patches, one for each part.
 *****************************************************************************/
void
load_patch_list(char *patch_list)
{
	PATCH           *patch;
	char            filename[PATH_MAX];
	char            *patch_name;
	char            *tokbuf;
	char            *p;
	unsigned int    part_num = 0;
	int             prog = 0;

	/* Get a comma separated client:port list from command line,
	   or a '-' (or no -p arg) for open subscription. */
	if (patch_list != NULL) {
		if ((tokbuf = alloca(strlen(patch_list) * 4)) == NULL) {
			phasex_shutdown("Out of memory!\n");
		}
		p = patch_list;
		while ((part_num < MAX_PARTS) && ((patch_name = strtok_r(p, ",", &tokbuf)) != NULL)) {
			p = NULL;
			patch = get_patch_from_bank(part_num, 0);
			/* read in patchdump for part num if patch is '-' */
			if (strcmp(patch_name, "-") == 0) {
				read_patch(user_patchdump_file[part_num], patch);
			}
			/* if patch is numeric and within range 0-127, treat it
			   as a program number */
			else if (((prog = atoi(patch_name)) > 0) && (prog <= PATCH_BANK_SIZE)) {
				session_bank[visible_sess_num].prog_num[part_num] =
					(unsigned int)(prog - 1);
			}
			/* default to reading in named patch */
			else {
				/* TODO: if user patch exists (in any directory) load it and
				   skip system patch. */
				snprintf(filename, sizeof(char) * PATH_MAX, "%s/%s.phx",
				         PATCH_DIR, patch_name);
				read_patch(filename, patch);
				snprintf(filename, sizeof(char) * PATH_MAX, "%s/%s.phx",
				         user_patch_dir, patch_name);
				read_patch(filename, patch);
				session_bank[visible_sess_num].prog_num[part_num] = (unsigned int) prog - 1;
			}
			part_num++;
		}
	}
}


/*****************************************************************************
 * get_patch_name_from_filename()
 *****************************************************************************/
char *
get_patch_name_from_filename(char *filename)
{
	char    *f;
	char    *tmpname;
	char    *name = NULL;

	/* missing filename gets name of "Untitled" */
	if ((filename == NULL) || (strcmp(filename, user_patchdump_file[0]) == 0)) {
		name = strdup("Untitled");
	}
	else {
		/* strip off leading directory components */
		if ((f = rindex(filename, '/')) == NULL) {
			tmpname = filename;
		}
		else {
			tmpname = f + 1;
		}

		/* make a copy that can be modified and freed safely */
		if ((name = strdup(tmpname)) == NULL) {
			phasex_shutdown("Out of memory!\n");
		}

		/* strip off the .phx */
		if ((f = strstr(name, ".phx\0")) != NULL) {
			*f = '\0';
		}
	}

	/* send name back to caller */
	return name;
}


/*****************************************************************************
 * midi_select_program()
 *****************************************************************************/
void
midi_select_program(unsigned int part_num, unsigned int prog_num)
{
	SESSION     *session = get_current_session();
	PATCH       *patch;

	if (!setting_ignore_midi_program_change) {
		patch = set_active_patch(visible_sess_num, part_num, prog_num);
		init_patch_state(patch);
		PHASEX_DEBUG(DEBUG_CLASS_MIDI_EVENT,
		             "\n*** MIDI Program Change:  part=%d  prog=%d (%d)\n\n",
		             (part_num + 1), prog_num, (prog_num + 1));
		visible_prog_num[part_num]                        = prog_num;
		session_bank[visible_sess_num].prog_num[part_num] = prog_num;
		if ((part_num == visible_part_num)) {
			pending_visible_patch = patch;
		}
		session->modified = 1;
	}
}
