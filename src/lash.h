/*****************************************************************************
 *
 * lash.h
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
#ifndef _PHASEX_LASH_H_
#define _PHASEX_LASH_H_

#include <lash/lash.h>
#include <jack/jack.h>
#include <asoundlib.h>


extern lash_client_t    *lash_client;
extern char             *lash_buffer;

extern char             *lash_jackname;
extern snd_seq_t        *lash_alsa_id;

extern char             *lash_project_dir;
extern char             *lash_project_name;


int  lash_client_init(int *argc, char ***argv);
void lash_client_set_jack_name(jack_client_t *client);
void lash_client_set_alsa_id(snd_seq_t *seq);
void lash_read_patches(char *lash_path);
void lash_save_patches(char *lash_path);
void lash_set_project_info(char *lash_dir);
void lash_set_phasex_session_name(void);
int  lash_poll_event(void);


#endif /* _PHASEX_LASH_H_ */
