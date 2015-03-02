/*****************************************************************************
 *
 * patch.c
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
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include "phasex.h"
#include "config.h"
#include "engine.h"
#include "filter.h"
#include "patch.h"
#include "param.h"
#include "param_parse.h"
#include "bank.h"
#include "session.h"
#include "gui_patch.h"
#include "string_util.h"
#include "debug.h"


/* per-part patch and patch state */
PATCH       *active_patch[MAX_PARTS];
PATCH_STATE *active_state[MAX_PARTS];

DIR_LIST    *patch_dir_list         = NULL;

int         patch_load_in_progress  = 0;

int         patch_name_changed      = 0;


/*****************************************************************************
 * get_visible_program_number()
 *****************************************************************************/
unsigned int
get_visible_program_number(void)
{
	if (visible_prog_num[visible_part_num] !=
	    session_bank[visible_sess_num].prog_num[visible_part_num]) {
		PHASEX_ERROR("visible_prog_num[visible_part_num] = %d     "
		             "session_bank[visible_sess_num].prog_num[visible_part_num] = %d\n",
		             visible_prog_num[visible_part_num],
		             session_bank[visible_sess_num].prog_num[visible_part_num]);
	}
	return session_bank[visible_sess_num].prog_num[visible_part_num];
}


/*****************************************************************************
 * get_patch()
 *****************************************************************************/
PATCH *
get_patch(unsigned int sess_num, unsigned int part_num, unsigned int prog_num)
{
	if (prog_num == 0) {
		return & (session_bank[sess_num].patch[part_num]);
	}
	return & (patch_bank[part_num][prog_num]);
}


/*****************************************************************************
 * get_visible_patch()
 *****************************************************************************/
PATCH *
get_visible_patch(void)
{
	return get_patch(visible_sess_num, visible_part_num, visible_prog_num[visible_part_num]);
}


/*****************************************************************************
 * get_visible_part()
 *****************************************************************************/
PART *
get_visible_part(void)
{
	return get_part(visible_part_num);
}


/*****************************************************************************
 * get_patch_state()
 *****************************************************************************/
PATCH_STATE *
get_patch_state(unsigned int sess_num, unsigned int part_num, unsigned int prog_num)
{
	if (prog_num == 0) {
		return & (session_bank[sess_num].state[part_num]);
	}
	return & (state_bank[part_num][prog_num]);
}


/*****************************************************************************
 * get_active_patch()
 *****************************************************************************/
PATCH *
get_active_patch(unsigned int part_num)
{
	return active_patch[part_num];
}


/*****************************************************************************
 * get_active_state()
 *****************************************************************************/
PATCH_STATE *
get_active_state(unsigned int part_num)
{
	return active_state[part_num];
}


/*****************************************************************************
 * set_patch()
 *****************************************************************************/
PATCH *
set_active_patch(unsigned int sess_num, unsigned int part_num, unsigned int prog_num)
{
	PATCH           *patch;
	PATCH_STATE     *state;

	if (prog_num == 0) {
		patch = & (session_bank[sess_num].patch[part_num]);
		state = & (session_bank[sess_num].state[part_num]);
	}
	else {
		patch = & (patch_bank[part_num][prog_num]);
		state = & (state_bank[part_num][prog_num]);
	}

	patch->sess_num = sess_num;

	active_patch[part_num] = patch;
	active_state[part_num] = state;

	return patch;
}


/*****************************************************************************
 * get_param_strval()
 *****************************************************************************/
char *
get_param_strval(PARAM *param)
{
	/* If a list of string values is available, use it. */
	if (param->info->strval_list != NULL) {
		return param->info->strval_list[param->value.cc_val];
	}
	return NULL;
}


/*****************************************************************************
 * init_patch_state()
 *****************************************************************************/
void
init_patch_state(PATCH *patch)
{
	PARAM           *param;
	unsigned int    param_num;

	for (param_num = 0; param_num < (NUM_PARAMS + 0); param_num++) {
		param = & (patch->param[param_num]);
		if ((param->info->locked) && (gp != NULL)) {
			param->value.cc_val  = gp->param[param_num].value.cc_val;
			param->value.int_val = gp->param[param_num].value.int_val;
		}
		cb_info[param_num].update_patch_state(& (patch->param[param_num]));
	}
}


/*****************************************************************************
 * init_patch()
 *****************************************************************************/
void
init_patch(PATCH *patch)
{
	PARAM           *param;
	unsigned int    param_num;

	for (param_num = 0; param_num < NUM_PARAMS; param_num++) {
		param = & (patch->param[param_num]);
		param->value.cc_val  = param->info->cc_default;
		param->value.int_val = param->info->cc_default + param->info->cc_offset;
		param->value.cc_prev = param->value.cc_val;
		param->updated = 0;
	}
}


/*****************************************************************************
 * init_patch_data_structures()
 *****************************************************************************/
void
init_patch_data_structures(PATCH        *patch,
                           unsigned int sess_num,
                           unsigned int part_num,
                           unsigned int prog_num)
{
	PARAM           *param;
	unsigned int    param_num;

	patch->sess_num = sess_num;
	patch->part_num = part_num;
	patch->prog_num = prog_num;

	patch->part  = get_part(part_num);
	if (sess_num == SESSION_BANK_SIZE) {
		patch->state = & (state_bank[part_num][prog_num]);
		patch->sess_num = 0;
	}
	else {
		patch->state = & (session_bank[sess_num].state[part_num]);
	}

	for (param_num = 0; param_num < NUM_HELP_PARAMS; param_num++) {
		param                = & (patch->param[param_num]);
		param->info          = get_param_info_by_id(param_num);
		param->patch         = patch;
		param->value.cc_val  = param->info->cc_default;
		param->value.int_val = param->info->cc_default + param->info->cc_offset;
		param->value.cc_prev = param->value.cc_val;
	}
}


/*****************************************************************************
 * init_patch_param_data()
 *****************************************************************************/
void
init_patch_param_data(void)
{
	PARAM           *param;
	PATCH           *patch;
	unsigned int    sess_num;
	unsigned int    part_num;
	unsigned int    prog_num;
	unsigned int    param_num;

	/* patch bank */
	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		for (prog_num = 0; prog_num < MAX_PARTS; prog_num++) {
			patch           = & (patch_bank[part_num][prog_num]);
			patch->state    = & (state_bank[part_num][prog_num]);
			patch->sess_num = 0;
			patch->part_num = part_num;
			patch->prog_num = prog_num;
			patch->part = get_part(part_num);
			for (param_num = 0; param_num < NUM_HELP_PARAMS; param_num++) {
				param                = & (patch->param[param_num]);
				param->info          = get_param_info_by_id(param_num);
				param->patch         = patch;
				param->value.cc_val  = param->info->cc_default;
				param->value.int_val = param->info->cc_default + param->info->cc_offset;
				param->value.cc_prev = param->value.cc_val;
			}
		}
	}

	/* session patch bank */
	for (sess_num = 0; sess_num < MAX_PARTS; sess_num++) {
		for (part_num = 0; part_num < MAX_PARTS; part_num++) {
			patch           = & (session_bank[sess_num].patch[part_num]);
			patch->state    = & (session_bank[sess_num].state[part_num]);
			patch->sess_num = sess_num;
			patch->part_num = part_num;
			patch->prog_num = 0;
			patch->part     = get_part(part_num);
			for (param_num = 0; param_num < NUM_HELP_PARAMS; param_num++) {
				param                = & (patch->param[param_num]);
				param->info          = get_param_info_by_id(param_num);
				param->patch         = patch;
				param->value.cc_val  = param->info->cc_default;
				param->value.int_val = param->info->cc_default + param->info->cc_offset;
				param->value.cc_prev = param->value.cc_val;
			}
		}
	}
}


/*****************************************************************************
 * read_patch()
 *****************************************************************************/
int
read_patch(char *filename, PATCH *patch)
{
	char            new_file_name[PATH_MAX];
	char            lash_patch_filename[16];
	PARAM           *param;
	DIR_LIST        *pdir       = patch_dir_list;
	DIR_LIST        *ldir       = NULL;
	FILE            *patch_f    = NULL;
	char            *token;
	char            *p;
	char            *tmpname;
	char            param_name[32];
	char            param_str_val[32];
	char            **cur;
	char            buffer[128];
	char            c;
	int             j;
	int             line        = 0;
	int             dir_found   = 0;
	int             cc_val;
	unsigned int    param_num;

	/* return error on missing filename */
	if ((filename == NULL) || (filename[0] == '\0')) {
		PHASEX_ERROR("***** read_patch():  filename == NULL !!!!!!!!!!!!!!!!!!!!!\n");
		return -1;
	}

	/* handle patch name supplied in place of patch file name */
	if (index(filename, '/') == NULL) {
		while ((pdir != NULL) && (!dir_found)) {
			snprintf(new_file_name,
			         sizeof(char *) * PATH_MAX,
			         "%s/%s.phx",
			         pdir->name,
			         filename);
			if ((patch_f = fopen(new_file_name, "rt")) != NULL) {
				filename = new_file_name;
				break;
			}
			pdir = pdir->next;
		}
	}
	/* otherwise open the patch with supplied filename */
	else if ((patch_f = fopen(filename, "rt")) == NULL) {
		return -1;
	}

	patch_load_in_progress = 1;

	/* free strings (will be rebuilt next) */
	if (patch->filename != NULL) {
		free(patch->filename);
		patch->filename = NULL;
	}
	if (patch->directory != NULL) {
		free(patch->directory);
		patch->directory = NULL;
	}

	/* keep track of filename, directory, and patch name, ignoring dump files */
	snprintf(lash_patch_filename,
	         sizeof(lash_patch_filename),
	         "phasex-%02d.phx",
	         (patch->part_num + 1));
	if ((filename != user_default_patch) &&
	    (filename != sys_default_patch) &&
	    (strncmp(filename, "/tmp/patch", 10) != 0) &&
	    (strstr(filename, lash_patch_filename) == NULL)) {
		patch->filename = strdup(filename);
		p = strdup(filename);
		patch->directory = strdup(dirname(p));
		memcpy(p, filename, (strlen(filename) + 1));
		token = basename(p);
		j = (int) strlen(token) - 4;
		if (strcmp(token + j, ".phx") == 0) {
			token[j] = '\0';
		}
		tmpname = patch->name;
		patch->name = strdup(token);
		if (tmpname != NULL) {
			free(tmpname);
		}
		free(p);

		/* maintain patch directory list */
		if ((patch->directory != NULL) &&
		    (* (patch->directory) != '\0') &&
		    (strcmp(patch->directory, PATCH_DIR) != 0) &&
		    (strcmp(patch->directory, user_patch_dir) != 0)) {
			while ((pdir != NULL) && (!dir_found)) {
				if (strcmp(pdir->name, patch->directory) == 0) {
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
				pdir->name = strdup(patch->directory);
				pdir->load_shortcut = pdir->save_shortcut = 0;
				if (ldir == NULL) {
					pdir->next = NULL;
					patch_dir_list = pdir;
				}
				else {
					pdir->next = ldir->next;
					ldir->next = pdir;
				}
			}
		}
	}

	/* start with a "blank" slate in case any parameters are missing */
	init_patch(patch);

	param_name[sizeof(param_name) - 1]       = '\0';
	param_str_val[sizeof(param_str_val) - 1] = '\0';

	/* read patch entries */
	while (fgets(buffer, sizeof(buffer), patch_f) != NULL) {
		line++;

		/* discard comments and blank lines */
		if ((buffer[0] == '\n') || (buffer[0] == '#')) {
			continue;
		}

		/* convert to lower case and strip comments for simpler parsing */
		p = buffer;
		while ((p < (buffer + sizeof(buffer))) && ((c = *p) != '\0')) {
			if (isupper(c)) {
				c  = (char) tolower(c);
				*p = c;
			}
			else if (c == '#') {
				*p = '\0';
			}
			p++;
		}

		/* get param name */
		if ((p = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		strncpy(param_name, p, (sizeof(param_name) - 1));

		/* make sure there's an equal '=' */
		if ((p = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		if (*p != '=') {
			while (get_next_token(buffer) != NULL);
			continue;
		}

		/* get param value */
		if ((p = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		strncpy(param_str_val, p, (sizeof(param_str_val) - 1));

		/* make sure there's a ';' */
		if ((p = get_next_token(buffer)) == NULL) {
			while (get_next_token(buffer) != NULL);
			continue;
		}
		if (*p != ';') {
			while (get_next_token(buffer) != NULL);
			continue;
		}

		/* if param_name is a valid param, parse param_str_value */
		if ((param = get_param_by_name(patch, param_name)) != NULL) {
			param_num = param->info->id;
			/* ignore locked parameters only after gui patch is initialized */
			if (param->info->locked && (gp != NULL)) {
				param->value.cc_val  = gp->param[param_num].value.cc_val;
				param->value.int_val = gp->param[param_num].value.int_val;
				param->value.cc_prev = gp->param[param_num].value.cc_prev;
			}
			/* unlocked parameters are parsed as normal */
			else {
				cc_val = 0;
				cur = param->info->strval_list;
				if (cur != NULL) {
					while (*cur != NULL) {
						if (strcmp(param_str_val, *cur) == 0) {
							param->value.cc_val  = cc_val;
							param->value.int_val = cc_val + param->info->cc_offset;
							break;
						}
						cur++;
						cc_val++;
					}
				}
				else {
					param->value.int_val = atoi(param_str_val);
					param->value.cc_val  = param->value.int_val - param->info->cc_offset;
				}
			}
		}
		/* param_name could be a patch info parameter */
		/* TODO:  add info params to standardized parameter system */
		else {
			if ((strcmp(param_name, "name") == 0) ||
			    (strcmp(param_name, "patch_name") == 0)) {
				tmpname = patch->name;
				patch->name = strdup(param_str_val);
				if (tmpname != NULL) {
					free(tmpname);
				}
			}
			//                      else if (strcmp (param_name, "phasex_version") == 0) {
			//                      }
		}

		/* flush remainder of line */
		while (get_next_token(buffer) != NULL);
	}

	/* done parsing */
	fclose(patch_f);

	/* set midi channel from current channel in part data */
	patch->param[PARAM_MIDI_CHANNEL].value.cc_prev =
		patch->param[PARAM_MIDI_CHANNEL].value.cc_val;

	patch->param[PARAM_MIDI_CHANNEL].value.cc_val  =
		patch->part->midi_channel;

	patch->param[PARAM_MIDI_CHANNEL].value.int_val =
		patch->part->midi_channel +
		patch->param[PARAM_MIDI_CHANNEL].info->cc_offset;

	patch->param[PARAM_MIDI_CHANNEL].updated = 1;

	patch->modified = 0;
	patch_load_in_progress = 0;

	return 0;
}


/*****************************************************************************
 * save_patch()
 *
 * Saves a patch in the same human and machine readable format
 * that patches are loaded in.
 *****************************************************************************/
int
save_patch(char *filename, PATCH *patch)
{
	PARAM           *param;
	char            lash_patch_filename[16];
	DIR_LIST        *pdir      = patch_dir_list;
	DIR_LIST        *ldir      = NULL;
	FILE            *patch_f;
	char            *name;
	char            *tmp;
	char            *param_str_val;
	char            *tmpname;
	int             j;
	int             param_num;
	int             dir_found  = 0;
	int             dump       = 0;

	/* return error on missing filename */
	if ((filename == NULL) || (filename[0] == '\0')) {
		return -1;
	}

	/* sanity check filename */
	for (j = 0; filename[j] != '\0'; j++) {
		if (filename[j] < 32) {
			filename[j] = '_';
		}
	}

	/* open the patch file */
	if ((patch_f = fopen(filename, "wt")) == NULL) {
		if (debug) {
			PHASEX_ERROR("Error opening patch file %s for write: %s\n",
			             filename, strerror(errno));
		}
		return -1;
	}

	if (filename == user_patchdump_file[patch->part_num]) {
		dump = 1;
	}

	/* keep track of filename changes, ignoring dump files and lash save files */
	snprintf(lash_patch_filename,
	         sizeof(lash_patch_filename),
	         "phasex-%02d.phx",
	         (patch->part_num + 1));
	if (!dump && (filename != patch->filename) &&
	    (strstr(filename, lash_patch_filename) == NULL)) {
		if (patch->filename != NULL) {
			free(patch->filename);
		}
		patch->filename = strdup(filename);

		/* keep track of directory and patch name as well */
		if (patch->directory != NULL) {
			free(patch->directory);
		}
		tmp = strdup(filename);
		patch->directory = strdup(dirname(tmp));
		memcpy(tmp, filename, (strlen(filename) + 1));
		name = basename(tmp);
		j = (int) strlen(name) - 4;
		if (strcmp(name + j, ".phx") == 0) {
			name[j] = '\0';
		}
		tmpname = patch->name;
		patch->name = strdup(name);
		if (tmpname != NULL) {
			free(tmpname);
		}
		free(tmp);

		/* maintain patch directory list */
		if ((patch->directory != NULL) &&
		    (* (patch->directory) != '\0') &&
		    (strcmp(patch->directory, PATCH_DIR) != 0) &&
		    (strcmp(patch->directory, user_patch_dir) != 0)) {
			while ((pdir != NULL) && (!dir_found)) {
				if (strcmp(pdir->name, patch->directory) == 0) {
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
				pdir->name = strdup(patch->directory);
				pdir->load_shortcut = pdir->save_shortcut = 0;
				if (ldir == NULL) {
					pdir->next = NULL;
					patch_dir_list = pdir;
				}
				else {
					pdir->next = ldir->next;
					ldir->next = pdir;
				}
			}
		}
	}

	/* write the patch */
	fprintf(patch_f, "# phasex patch\n");
	fprintf(patch_f, "phasex_version\t\t= %s;\n", PACKAGE_VERSION);

	if ((patch->name != NULL) && (patch->name[0] != '\0')) {
		fprintf(patch_f, "patch_name\t\t= \"%s\";\n", patch->name);
	}

	for (param_num = 0; param_num < NUM_PARAMS; param_num++) {
		param = & (patch->param[param_num]);
		fputs(param->info->name, patch_f);
		j = 32 - (int) strlen(param->info->name);
		while (j > 8) {
			fputc('\t', patch_f);
			j -= 8;
		}
		if ((param_str_val = get_param_strval(param)) != NULL) {
			fprintf(patch_f, "= %s;\n", param_str_val);
		}
		else {
			fprintf(patch_f, "= %d;\n", param->value.int_val);
		}
	}

	/* Done writing patch */
	fclose(patch_f);

	/* Mark patch unmodified for non buffer dumps */
	if (!dump) {
		patch->modified = 0;
	}

	return 0;
}
