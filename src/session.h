/*****************************************************************************
 *
 * session.h
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
#ifndef _PHASEX_SESSION_H_
#define _PHASEX_SESSION_H_

#include <gtk/gtk.h>
#include "phasex.h"
#include "patch.h"


typedef struct session {
	char            *name;                  /* Session name */
	char            *directory;             /* session directory name */
	char            *parent_dir;            /* parent to session directory */
	int             modified;               /* flag to track session modification */
	unsigned int    prog_num[MAX_PARTS];    /* per-part program number for this session */
	PATCH           patch[MAX_PARTS];       /* per-part patches for this session */
	PATCH_STATE     state[MAX_PARTS];       /* per-part patch state for this session */
} SESSION;


#define SESSION_BANK_SIZE   1024

#define SESSION_NAME_LEN    24


extern SESSION      session_bank[SESSION_BANK_SIZE];

extern int          current_session;

extern DIR_LIST     *session_dir_list;

extern int          session_load_in_progress;

extern int          session_name_changed;


#define get_patch_from_session_bank(sess_num, part_num)                 \
	(&(session_bank[sess_num].patch[part_num]))


SESSION *get_current_session(void);
SESSION *get_session(unsigned int session_num);
PATCH *set_patch_from_session_bank(unsigned int part_num, unsigned int session_num);
void init_session_bank(char *filename);
void load_session_bank(char *filename);
void save_session_bank(char *filename);
int load_session(char *directory, unsigned int session_num, int managed);
int save_session(char *directory, unsigned int session_num, int managed);
char *get_session_name_from_directory(char *directory);


#endif /* _PHASEX_SESSION_H_ */
